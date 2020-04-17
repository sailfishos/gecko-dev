/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "nsWindow.h"
#include "EmbedLiteWindowBaseChild.h"
#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteApp.h"

#include "GLContextProvider.h"
#include "GLContext.h"                       // for GLContext

#include "ClientLayerManager.h"              // for ClientLayerManager
#include "Layers.h"                          // for LayerManager

#include "mozilla/layers/CompositorThread.h" // for CompositorThreadHolder
#include "mozilla/Preferences.h"             // for Preferences

#include "base/basictypes.h"

#include "mozilla/Hal.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/ipc/MessageChannel.h"

using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;
//using namespace mozilla::ipc;

namespace mozilla {
namespace embedlite {

NS_IMPL_ISUPPORTS_INHERITED(nsWindow, PuppetWidgetBase,
                            nsISupportsWeakReference)

static bool sFailedToCreateGLContext = false;
static bool sUseExternalGLContext = false;
static bool sRequestGLContextEarly = false;

static void InitPrefs()
{
  static bool prefsInitialized = false;
  if (!prefsInitialized) {
    Preferences::AddBoolVarCache(&sUseExternalGLContext,
        "embedlite.compositor.external_gl_context", false);
    Preferences::AddBoolVarCache(&sRequestGLContextEarly,
        "embedlite.compositor.request_external_gl_context_early", false);
    prefsInitialized = true;
  }
}

nsWindow::nsWindow(EmbedLiteWindowBaseChild *window)
  : PuppetWidgetBase()
  , mWindow(window)
{
  LOGT("nsWindow: %p window: %p", this, mWindow);
  InitPrefs();

  if (sUseExternalGLContext && sRequestGLContextEarly) {
    CompositorThreadHolder::Loop()->PostTask(NewRunnableFunction(&CreateGLContextEarly,
                                                                 window->GetUniqueID()));
  }
}

nsresult
nsWindow::Create(nsIWidget *aParent, nsNativeWidget aNativeParent, const LayoutDeviceIntRect &aRect, nsWidgetInitData *aInitData)
{
  LOGT();
  Unused << Create(aParent, aNativeParent, aRect, aInitData);

  // XXX: Move to EmbedLiteWindow?
  gfxPlatform::GetPlatform();

#if DEBUG
  DumpWidgetTree();
#endif

  return NS_OK;
}

void
nsWindow::Destroy()
{
  PuppetWidgetBase::Destroy();
  mWindow = nullptr;

  Shutdown();
#if DEBUG
  DumpWidgetTree();
#endif
}

NS_IMETHODIMP
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent *aEvent, nsEventStatus &aStatus)
{
  aStatus = DispatchEvent(aEvent);
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext &aContext, const InputContextAction &aAction)
{
  mInputContext = aContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
  return mInputContext;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
  LOGT();

  return PuppetWidgetBase::Show(aState);
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
  Unused << PuppetWidgetBase::Resize(aWidth, aHeight, aRepaint);
  // Looks that we need CompositorSession
#if 0
  if (mCompositorParent) {
    static_cast<EmbedLiteCompositorBridgeParent*>(mCompositorParent.get())->
        SetSurfaceSize(mNaturalBounds.width, mNaturalBounds.height);
  }
#endif

  return NS_OK;
}

LayoutDeviceIntRect
nsWindow::GetNaturalBounds()
{
  return mNaturalBounds;
}

void
nsWindow::CreateCompositor()
{
  LOGT();
  // Compositor should be created only for top level widgets, aka windows.
  MOZ_ASSERT(mWindow);
  LayoutDeviceIntRect size = mWindow->GetSize();
  CreateCompositor(size.width, size.height);
}

void
nsWindow::CreateCompositor(int aWidth, int aHeight)
{
  LOGT();
  nsBaseWidget::CreateCompositor(aWidth, aHeight);
}

void *
nsWindow::GetNativeData(uint32_t aDataType)
{
  LOGT("t:%p, DataType: %i", this, aDataType);
  switch (aDataType) {
    case NS_NATIVE_SHAREABLE_WINDOW: {
      LOGW("aDataType:%i\n", aDataType);
      return (void*)nullptr;
    }
    case NS_NATIVE_OPENGL_CONTEXT: {
      MOZ_ASSERT(!GetParent());
      return GetGLContext();
    }
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_DISPLAY:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_SHELLWIDGET:
    case NS_NATIVE_WIDGET:
      LOGW("nsWindow::GetNativeData not implemented for this type");
      break;
    case NS_RAW_NATIVE_IME_CONTEXT:
      return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
    default:
      NS_WARNING("nsWindow::GetNativeData called with bad value");
      break;
  }

  return nullptr;
}

LayerManager *
nsWindow::GetLayerManager(PLayerTransactionChild *aShadowManager, LayersBackend aBackendHint, LayerManagerPersistence aPersistence)
{
  if (!mLayerManager) {
    if (!mShutdownObserver) {
      // We are shutting down, do not try to re-create a LayerManager
      return nullptr;
    }
  }

  // TODO: Check from nsBaseWidget.

  if (mLayerManager) {
    // This layer manager might be used for painting outside of DoDraw(), so we need
    // to set the correct rotation on it.
    if (mLayerManager->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
        ClientLayerManager* manager =
            static_cast<ClientLayerManager*>(mLayerManager.get());
        manager->SetDefaultTargetConfiguration(mozilla::layers::BufferMode::BUFFER_NONE,
                                               mRotation);
    }
    return mLayerManager;
  }

  LOGT();

  nsIWidget* topWidget = GetTopLevelWidget();
  if (topWidget != this) {
    mLayerManager = topWidget->GetLayerManager();
  }

  if (mLayerManager) {
    return mLayerManager;
  }

  if (EmbedLiteApp::GetInstance()->GetType() == EmbedLiteApp::EMBED_INVALID) {
    printf("Create Layer Manager for Process View\n");
    mLayerManager = new ClientLayerManager(this);
    ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder();
    if (!lf->HasShadowManager() && aShadowManager) {
      lf->SetShadowManager(aShadowManager);
    }
    return mLayerManager;
  }

  // TODO : We should really split this into Android/Gonk like nsWindow and separate PuppetWidget
  // Only Widget hosting window can create compositor.
  // Bug: https://bugs.merproject.org/show_bug.cgi?id=1603
  if (mWindow && ShouldUseOffMainThreadCompositing()) {
    CreateCompositor();
    if (mLayerManager) {
      return mLayerManager;
    }
    // If we get here, then off main thread compositing failed to initialize.
    sFailedToCreateGLContext = true;
  }

  mLayerManager = new ClientLayerManager(this);
  return mLayerManager;
}

void
nsWindow::DrawWindowUnderlay(mozilla::widget::WidgetRenderingContext *aContext, LayoutDeviceIntRect aRect)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;
  Unused << aRect;
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->DrawUnderlay();
  }
}

void
nsWindow::DrawWindowOverlay(mozilla::widget::WidgetRenderingContext *aContext, LayoutDeviceIntRect aRect)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->DrawOverlay(aRect.ToUnknownRect());
  }
}

bool
nsWindow::PreRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;
  if (!IsVisible() || !mActive) {
    return false;
  }

  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    return window->GetListener()->PreRender();
  }
  return true;
}

void
nsWindow::PostRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;

  if (GetCompositorBridgeParent()) {
    static_cast<EmbedLiteCompositorBridgeParent*>(GetCompositorBridgeParent())->PresentOffscreenSurface();
  }

  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->CompositingFinished();
  }
}

void
nsWindow::AddObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.AppendElement(aObserver);
}

void
nsWindow::RemoveObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.RemoveElement(aObserver);
}

uint32_t nsWindow::GetUniqueID() const
{
  MOZ_ASSERT(mWindow);
  return mWindow->GetUniqueID();
}

nsWindow::~nsWindow()
{
  LOGT("this: %p", this);
}

void
nsWindow::ConfigureAPZCTreeManager()
{
  LOGT("Do nothing - APZEventState configured in EmbedLiteViewBaseChild");
}

void
nsWindow::ConfigureAPZControllerThread()
{
  LOGT("Do nothing - APZController thread configured in EmbedLiteViewBaseParent");
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController()
{
  return nullptr;
}

bool nsWindow::UseExternalCompositingSurface() const
{
  return true;
}

const char *
nsWindow::Type() const
{
  return "nsWindow";
}

CompositorBridgeParent *
nsWindow::GetCompositorBridgeParent() const
{
  return mCompositorSession ? mCompositorSession->GetInProcessBridge() : nullptr;
}

// Private
nsWindow::nsWindow()
{

}


GLContext*
nsWindow::GetGLContext() const
{
  LOGT("this:%p, UseExternalContext:%d", this, sUseExternalGLContext);
  if (sUseExternalGLContext) {
    EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
    void* context = nullptr;
    void* surface = nullptr;
    if (window && window->GetListener()->RequestGLContext(context, surface)) {
      MOZ_ASSERT(context && surface);
      RefPtr<GLContext> mozContext = GLContextProvider::CreateWrappingExisting(context, surface);
      if (!mozContext || !mozContext->Init()) {
        NS_ERROR("Failed to initialize external GL context!");
        return nullptr;
      }
      return mozContext.forget().take();
    } else {
      NS_ERROR("Embedder wants to use external GL context without actually providing it!");
    }
  }
  return nullptr;
}

nsEventStatus
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent *aEvent)
{
  if (mAttachedWidgetListener) {
      return mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  } else if (mWidgetListener) {
      return mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  }
  return nsEventStatus_eIgnore;
}

void
nsWindow::CreateGLContextEarly(uint32_t aWindowId)
{
  LOGT("WindowID:%u", aWindowId);
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(aWindowId);
  if (window) {
    void* context = nullptr;
    void* surface = nullptr;
    window->GetListener()->RequestGLContext(context, surface);
    MOZ_ASSERT(context && surface);
  } else {
    NS_WARNING("Trying to early create GL context for non existing window!");
  }
}

}  // namespace embedlite
}  // namespace mozilla
