/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QGuiApplication>
#include "mozqwidget.h"
#include "mozilla/DebugOnly.h"

#include <fcntl.h>

#include "mozilla/dom/TabParent.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsAutoPtr.h"
#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsScreenManagerQt.h"
#include "nsTArray.h"
#include "nsWindow.h"
#include "nsIWidgetListener.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/layers/CompositorParent.h"
#include "nsThreadUtils.h"
#include "gfxQtPlatform.h"


#define IS_TOPLEVEL() (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;

nsWindow::nsWindow()
  : mWidget(nullptr)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
}

nsWindow::~nsWindow()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget *aParent,
                 void *aNativeParent,
                 const nsIntRect &aRect,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    // only set the base parent if we're not going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aContext, aInitData);

    mVisible = true;

    // and do our common creation
    mParent = (nsWindow *)aParent;

    // save our bounds
    mBounds = aRect;

    // find native parent
    MozQWidget *parent = nullptr;

    if (aParent != nullptr) {
        parent = static_cast<MozQWidget*>(aParent->GetNativeData(NS_NATIVE_WIDGET));
    } else if (aNativeParent != nullptr) {
        parent = static_cast<MozQWidget*>(aNativeParent);
        if (parent && mParent == nullptr) {
            mParent = parent->getReceiver();
        }
    }

    LOG(("Create: nsWindow [%p] mWidget:[%p] parent:[%p], natPar:[%p] mParent:%p\n", (void *)this, (void*)mWidget, parent, aNativeParent, mParent));

    // ok, create our QGraphicsWidget
    mWidget = createQWidget(parent, aInitData);

    if (!mWidget) {
        return NS_ERROR_OUT_OF_MEMORY;
    }


    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);

    return NS_OK;
}

MozQWidget*
nsWindow::createQWidget(MozQWidget *parent,
                        nsWidgetInitData *aInitData)
{
    const char *windowName = nullptr;
    Qt::WindowFlags flags = Qt::Widget;

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        windowName = "topLevelDialog";
        flags = Qt::Dialog;
        break;
    case eWindowType_popup:
        windowName = "topLevelPopup";
        flags = Qt::Popup;
        break;
    case eWindowType_toplevel:
        windowName = "topLevelWindow";
        flags = Qt::Window;
        break;
    case eWindowType_invisible:
        windowName = "topLevelInvisible";
        break;
    case eWindowType_child:
    case eWindowType_plugin:
    default: // sheet
        windowName = "paintArea";
        break;
    }

    MozQWidget* widget = new MozQWidget(this, parent);
    if (!widget) {
        return nullptr;
    }

    widget->setObjectName(QString(windowName));
    if (mWindowType == eWindowType_invisible) {
        widget->setVisibility(QWindow::Hidden);
    }
    widget->create();

    // create a QGraphicsView if this is a new toplevel window
    LOG(("nsWindow::%s [%p] Created Window: %s, widget:%p, par:%p\n", __FUNCTION__, (void *)this, windowName, widget, parent));

    return widget;
}


NS_IMETHODIMP
nsWindow::Destroy(void)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    nsBaseWidget::OnDestroy();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));
    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }

    if (aState) {
        mWidget->show();
    } else {
        mWidget->hide();
    }

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return mVisible;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX,
               double aY)
{
    LOG(("nsWindow::Move [%p] %f %f\n", (void *)this,
         aX, aY));

    CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                   : CSSToLayoutDeviceScale(1.0);
    int32_t x = NSToIntRound(aX * scale.scale);
    int32_t y = NSToIntRound(aY * scale.scale);

   if (mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog) {
        SetSizeMode(nsSizeMode_Normal);
    }

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    if (x == mBounds.x && y == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    // XXX Should we do some AreBoundsSane check here?

    mBounds.x = x;
    mBounds.y = y;

    mWidget->setPosition(x, y);

    NotifyRollupGeometryChange();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    LOG(("nsWindow::Resize [%p] %g %g\n", (void *)this, aWidth, aHeight));

    return Resize(mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
nsWindow::Resize(double aX,
                 double aY,
                 double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %g %g %g %g\n", (void *)this, aX, aY, aWidth, aHeight));
    mBounds = nsIntRect(NSToIntRound(aX), NSToIntRound(aY),
                        NSToIntRound(aWidth), NSToIntRound(aHeight));

    if (!mWidget)
        return NS_OK;

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    if (mWidgetListener)
        mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    LOG(("nsWindow::%s [%p] aState:%i\n", __FUNCTION__, (void *)this, aState));
    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return true;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    LOG(("nsWindow::%s [%p] aRaise:%i\n", __FUNCTION__, (void *)this, aRaise));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>&)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    LOGDRAW(("nsWindow::Invalidate (rect) [%p]: %d %d %d %d\n", (void *)this, aRect.x, aRect.y, aRect.width, aRect.height));

    if (!mWidget)
        return NS_OK;

    mWidget->renderLater();

    return NS_OK;
}

nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    nsIntPoint p(0, 0);
    nsWindow *w = this;

    while (w && w->mParent) {
        p.x += w->mBounds.x;
        p.y += w->mBounds.y;

        w = w->mParent;
    }

    return p;
}

void*
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        LOG(("nsWindow::%s [%p] return mWidget:%p\n", __FUNCTION__, (void *)this, mWidget));
        return mWidget;
        break;
    }
    case NS_NATIVE_SHAREABLE_WINDOW: {
        return mWidget ? (void*)mWidget->winId() : nullptr;
    }
    case NS_NATIVE_DISPLAY: {
#ifdef MOZ_X11
        return gfxQtPlatform::GetXDisplay(mWidget);
#endif
        break;
    }
    case NS_NATIVE_PLUGIN_PORT: {
        break;
    }
    case NS_NATIVE_GRAPHIC: {
        break;
    }
    case NS_NATIVE_SHELLWIDGET: {
        break;
    }
    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nullptr;
    }
    LOG(("nsWindow::%s [%p] aDataType:%i\n", __FUNCTION__, (void *)this, aDataType));
    return nullptr;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mWidgetListener)
      aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);

    return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    mInputContext = aContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    // There is only one IME context on Gonk.
    mInputContext.mNativeIMEContext = nullptr;
    return mInputContext;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mWindowType != eWindowType_toplevel) {
        // Ignore fullscreen request for non-toplevel windows.
        NS_WARNING("MakeFullScreen() on a dialog or child widget?");
        return nsBaseWidget::MakeFullScreen(aFullScreen);
    }

    return NS_OK;
}

void
nsWindow::OnQRender()
{
    LOGDRAW(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
}

LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    LOGDRAW(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (!mLayerManager && eTransparencyTransparent == GetTransparencyMode()) {
        mLayerManager = CreateBasicLayerManager();
    }

    return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint,
                                         aPersistence, aAllowRetaining);
}

void
nsWindow::UserActivity()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (!mIdleService) {
        mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
    }

    if (mIdleService) {
        mIdleService->ResetIdleTimeOut(0);
    }
}

uint32_t
nsWindow::GetGLFrameBufferFormat()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
        // We directly map the hardware fb on Gonk.  The hardware fb
        // has RGB format.
        return LOCAL_GL_RGB;
    }
    return LOCAL_GL_NONE;
}
