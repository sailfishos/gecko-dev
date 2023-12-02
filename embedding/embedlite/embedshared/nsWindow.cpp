/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "nsWindow.h"
#include "EmbedLiteWindowChild.h"
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
#include "mozilla/StaticPrefs_embedlite.h"   // for StaticPrefs::embedlite_*()

using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;
//using namespace mozilla::ipc;

#ifdef MOZ_LOGGING
#define LOG_CONTROLLER_STACK  \
  if (MOZ_UNLIKELY(detail::log_test(GetEmbedCommonLog("EmbedLiteTrace"), LogLevel::Debug))) { \
    LogControllerStack(mControllers); \
  }
#else // MOZ_LOGGING
#define LOG_CONTROLLER_STACK  do {} while (0)
#endif // MOZ_LOGGING

namespace mozilla {
namespace embedlite {

NS_IMPL_ISUPPORTS_INHERITED(nsWindow, PuppetWidgetBase,
                            nsISupportsWeakReference)

static bool sFailedToCreateGLContext = false;

nsWindow::nsWindow(EmbedLiteWindowChild *window)
  : PuppetWidgetBase()
  , mFirstViewCreated(false)
  , mWindow(window)
{
  LOGT("nsWindow: %p window: %p external: %d early: %d", this, mWindow,
      StaticPrefs::embedlite_compositor_external_gl_context(),
      StaticPrefs::embedlite_compositor_request_external_gl_context_early());

  if (StaticPrefs::embedlite_compositor_external_gl_context() &&
      StaticPrefs::embedlite_compositor_request_external_gl_context_early()) {
    mozilla::layers::CompositorThread()->Dispatch(NewRunnableFunction(
                                                 "mozilla::embedlite::nsWindow::CreateGLContextEarly",
                                                 &CreateGLContextEarly,
                                                 mWindow->GetListener()));
  }
}

nsresult
nsWindow::Create(nsIWidget *aParent, nsNativeWidget aNativeParent, const LayoutDeviceIntRect &aRect, nsWidgetInitData *aInitData)
{
  LOGT();
  Unused << PuppetWidgetBase::Create(aParent, aNativeParent, aRect, aInitData);
  gfxPlatform::GetPlatform();

#if DEBUG
  DumpWidgetTree();
#endif

  return NS_OK;
}

void
nsWindow::Destroy()
{
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mWindow = nullptr;

  PuppetWidgetBase::Destroy();

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

void
nsWindow::SetInputContext(const InputContext &aContext, const InputContextAction &aAction)
{
  mInputContext = aContext;
}

InputContext
nsWindow::GetInputContext()
{
  return mInputContext;
}

void
nsWindow::Show(bool aState)
{
  LOGT();
  PuppetWidgetBase::Show(aState);
}

void
nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
  PuppetWidgetBase::Resize(aWidth, aHeight, aRepaint);
  if (GetCompositorBridgeParent()) {
    static_cast<EmbedLiteCompositorBridgeParent*>(GetCompositorBridgeParent())->
        SetSurfaceRect(mNaturalBounds.x, mNaturalBounds.y, mNaturalBounds.width, mNaturalBounds.height);
  }
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
  LOGC("EmbedLiteLayerManager", "lm: %p", mLayerManager.get());

  if (!mLayerManager) {
    if (!mShutdownObserver) {
      // We are shutting down, do not try to re-create a LayerManager
      return nullptr;
    }
  }

  LayerManager *lm = PuppetWidgetBase::GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  LOGC("EmbedLiteLayerManager", "lm: %p this: %p", lm, this);

  if (lm) {
    mLayerManager = lm;
    return mLayerManager;
  }

  if (mWindow && ShouldUseOffMainThreadCompositing()) {
    CreateCompositor();
    LOGC("EmbedLiteLayerManager", "Created compositor, lm: %p", mLayerManager.get());
    if (mLayerManager) {
      return mLayerManager;
    }
    // If we get here, then off main thread compositing failed to initialize.
    sFailedToCreateGLContext = true;
  }

  mLayerManager = new ClientLayerManager(this);
  LOGC("EmbedLiteLayerManager", "New client layer manager: %p", mLayerManager.get());

  return mLayerManager;
}

bool
nsWindow::PreRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;
  if (!IsVisible() || !mActive) {
    return false;
  }

  return mWindow->GetListener()->PreRender();
}

void
nsWindow::PostRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  Unused << aContext;

  if (GetCompositorBridgeParent()) {
    static_cast<EmbedLiteCompositorBridgeParent*>(GetCompositorBridgeParent())->PresentOffscreenSurface();
  }

  mWindow->GetListener()->CompositingFinished();
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

layers::LayersId nsWindow::GetRootLayerId() const
{
  return mCompositorSession ? mCompositorSession->RootLayerTreeId() : layers::LayersId{0};
}

#ifdef MOZ_LOGGING
static void LogControllerStack(std::list<EmbedContentController *> &controllers)
{
  int pos = 0;
  for (std::list<EmbedContentController *>::const_iterator it = controllers.begin(); it != controllers.end(); ++it) {
    LOGT("Stack controller %d : %u", pos, (*it)->GetUniqueID());
    ++pos;
  }
}
#endif // MOZ_LOGGING

inline static bool ControllersEqual(const EmbedContentController *aController1, const EmbedContentController *aController2)
{
  // Compare unique ids, but fallback to pointer comparison if id is zero
  return (aController1->GetUniqueID() != 0 && (aController1->GetUniqueID() == aController2->GetUniqueID()))
      || (aController1 == aController2);
}

void nsWindow::Activate(EmbedContentController *aController)
{
  if (mCompositorSession) {
    mCompositorSession->SetContentController(aController);
  }

  MOZ_ASSERT(aController);

  // If the view is deactivated we need to reactivate the previous view's
  // content controller, so we store the controllwers on a stack
  if (mControllers.empty() || !ControllersEqual(aController, mControllers.back())) {
    LOGT("Pushing content controller %u", aController->GetUniqueID());
    mControllers.push_back(aController);
    LOG_CONTROLLER_STACK;
  }
}

void nsWindow::Deactivate(EmbedContentController *aController)
{
  if (!aController || mControllers.empty()) {
    return;
  }

  if (ControllersEqual(aController, mControllers.back())) {
    // The active view deactivated, so we need to restore the previous
    // view's content controller
    LOGT("Popping content controller %u", aController->GetUniqueID());
    mControllers.pop_back();
    if (mCompositorSession && !mControllers.empty()) {
      mCompositorSession->SetContentController(mControllers.back());
    }
  } else {
    // An inactive view deactivated, so we erase all instances of its
    // content controllers from the stack
    LOGT("Erasing content controller %u", aController->GetUniqueID());
    mControllers.erase(std::remove_if(mControllers.begin(), mControllers.end(),
                                      [aController](const EmbedContentController *aOther) {
      return ControllersEqual(aController, aOther);
    }), mControllers.end());
  }
  LOG_CONTROLLER_STACK;
}

RefPtr<mozilla::layers::IAPZCTreeManager> nsWindow::GetAPZCTreeManager()
{
  if (mCompositorSession) {
    return mAPZC;
  }

  return nullptr;
}

nsWindow::~nsWindow()
{
  LOGT("this: %p", this);
}

void
nsWindow::ConfigureAPZCTreeManager()
{
  LOGT("Do nothing - APZEventState configured in EmbedLiteViewChild");
}

void
nsWindow::ConfigureAPZControllerThread()
{
  LOGT("Do nothing - APZController thread configured in EmbedLiteViewParent");
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
  LOGT("this:%p, UseExternalContext:%d", this,
      StaticPrefs::embedlite_compositor_external_gl_context());
  if (StaticPrefs::embedlite_compositor_external_gl_context()) {
    void* context = nullptr;
    void* surface = nullptr;
    void* display = nullptr;
    if (mWindow && mWindow->GetListener()->RequestGLContext(context, surface, display)) {
      MOZ_ASSERT(context && surface);
      RefPtr<GLContext> mozContext = GLContextProvider::CreateWrappingExisting(context, surface, display);
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
nsWindow::CreateGLContextEarly(EmbedLiteWindowListener *aListener)
{
  LOGT("Listener: %p", aListener);

  void* context = nullptr;
  void* surface = nullptr;
  void* display = nullptr;
  aListener->RequestGLContext(context, surface, display);
  MOZ_ASSERT(context && surface);
}

}  // namespace embedlite
}  // namespace mozilla

already_AddRefed<nsIWidget>
nsIWidget::CreateTopLevelWindow()
{
  nsCOMPtr<nsIWidget> window = new mozilla::embedlite::nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget>
nsIWidget::CreateChildWindow()
{
  nsCOMPtr<nsIWidget> window = new mozilla::embedlite::nsWindow();
  return window.forget();
}

