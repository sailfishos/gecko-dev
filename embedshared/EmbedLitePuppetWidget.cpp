/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "base/basictypes.h"

#include "BasicLayers.h"
#include "gfxPlatform.h"
#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "LayerManagerD3D10.h"
#endif
#include "mozilla/Hal.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/ipc/MessageChannel.h"
#include "EmbedLitePuppetWidget.h"
#include "nsIWidgetListener.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "ClientLayerManager.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "EmbedLiteCompositorParent.h"
#include "mozilla/Preferences.h"

#ifdef DEBUG
#include "mozilla/TextComposition.h"
#include "mozilla/IMEStateManager.h"
#endif

#include "EmbedLiteApp.h"
#include "LayerScope.h"
#include "mozilla/unused.h"
#include "mozilla/BasicEvents.h"

#include <sys/syscall.h>

using namespace mozilla::dom;
using namespace mozilla::gl;
using namespace mozilla::hal;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::ipc;

namespace mozilla {
namespace embedlite {

// Arbitrary, fungible.
const size_t EmbedLitePuppetWidget::kMaxDimension = 4000;

static nsTArray<EmbedLitePuppetWidget*> gTopLevelWindows;
static bool sFailedToCreateGLContext = false;
static bool sUseExternalGLContext = false;
static bool sRequestGLContextEarly = false;

NS_IMPL_ISUPPORTS_INHERITED(EmbedLitePuppetWidget, nsBaseWidget,
                            nsISupportsWeakReference)

static bool
IsPopup(const nsWidgetInitData* aInitData)
{
  return aInitData && aInitData->mWindowType == eWindowType_popup;
}

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

bool
EmbedLitePuppetWidget::IsTopLevel()
{
  return mWindowType == eWindowType_toplevel ||
         mWindowType == eWindowType_dialog ||
         mWindowType == eWindowType_invisible;
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget(EmbedLiteWindowBaseChild* window,
                                             EmbedLiteViewChildIface* view)
  : mWindow(window)
  , mView(view)
  , mVisible(false)
  , mEnabled(false)
  , mActive(false)
  , mHasCompositor(false)
  , mIMEComposing(false)
  , mParent(nullptr)
  , mRotation(mozilla::ROTATION_0)
  , mMargins(0, 0, 0, 0)
  , mDPI(-1.0)
{
  MOZ_COUNT_CTOR(EmbedLitePuppetWidget);
  LOGT("Puppet: %p, view: %p, window: %p", this, mView, mWindow);
  InitPrefs();
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget(EmbedLiteWindowBaseChild* window)
  : EmbedLitePuppetWidget(window, nullptr)
{
  if (sUseExternalGLContext && sRequestGLContextEarly) {
    CompositorParent::CompositorLoop()->PostTask(FROM_HERE,
        NewRunnableFunction(&CreateGLContextEarly, window->GetUniqueID()));
  }
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget(EmbedLiteViewChildIface* view)
  : EmbedLitePuppetWidget(nullptr, view)
{
}

EmbedLitePuppetWidget::~EmbedLitePuppetWidget()
{
  MOZ_COUNT_DTOR(EmbedLitePuppetWidget);
  LOGT("this:%p", this);

  if (IsTopLevel()) {
    gTopLevelWindows.RemoveElement(this);
  }
}

NS_IMETHODIMP
EmbedLitePuppetWidget::SetParent(nsIWidget* aParent)
{
  LOGT();
  if (mParent == static_cast<EmbedLitePuppetWidget*>(aParent)) {
    return NS_OK;
  }

  if (mParent) {
    mParent->mChildren.RemoveElement(this);
  }

  mParent = static_cast<EmbedLitePuppetWidget*>(aParent);

  if (mParent) {
    mParent->mChildren.AppendElement(this);
  }

  return NS_OK;
}

nsIWidget*
EmbedLitePuppetWidget::GetParent(void)
{
  return mParent;
}

void
EmbedLitePuppetWidget::SetRotation(mozilla::ScreenRotation rotation)
{
  mRotation = rotation;

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->WidgetRotationChanged(mRotation);
  }

  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->SetRotation(rotation);
  }

#ifdef DEBUG
  if (IsTopLevel()) {
    DumpWidgetTree();
  }
#endif
}

void
EmbedLitePuppetWidget::SetMargins(const nsIntMargin& margins)
{
  mMargins = margins;
  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->SetMargins(margins);
  }
}

void
EmbedLitePuppetWidget::UpdateSize()
{
  Resize(mNaturalBounds.width, mNaturalBounds.height, true);
#ifdef DEBUG
  DumpWidgetTree();
#endif
}

void EmbedLitePuppetWidget::SetActive(bool active)
{
  mActive = active;
}

NS_IMETHODIMP
EmbedLitePuppetWidget::Create(nsIWidget*        aParent,
                              nsNativeWidget    aNativeParent,
                              const LayoutDeviceIntRect& aRect,
                              nsWidgetInitData* aInitData)
{
  LOGT("Puppet: %p, parent: %p", this, aParent);

  NS_ASSERTION(!aNativeParent, "got a non-Puppet native parent");

  mParent = static_cast<EmbedLitePuppetWidget*>(aParent);

  mEnabled = true;
  mVisible = mParent ? mParent->mVisible : true;

  if (mParent) {
    mParent->mChildren.AppendElement(this);
  }
  mRotation = mParent ? mParent->mRotation : mRotation;
  mBounds = mParent ? mParent->mBounds : aRect.ToUnknownRect();
  mMargins = mParent ? mParent->mMargins : mMargins;
  mNaturalBounds = mParent ? mParent->mNaturalBounds : aRect.ToUnknownRect();;

  BaseCreate(aParent, LayoutDeviceIntRect::FromUnknownRect(mBounds), aInitData);

  if (IsTopLevel()) {
    LOGT("Append this to toplevel windows:%p", this);
    gTopLevelWindows.AppendElement(this);
  }

  // XXX: Move to EmbedLiteWindow?
  gfxPlatform::GetPlatform();

#if DEBUG
  DumpWidgetTree();
#endif

  return NS_OK;
}

already_AddRefed<nsIWidget>
EmbedLitePuppetWidget::CreateChild(const LayoutDeviceIntRect &aRect,
                                   nsWidgetInitData* aInitData,
                                   bool              aForceUseIWidgetParent)
{
  LOGT();
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = new EmbedLitePuppetWidget(nullptr, nullptr);
  nsresult rv = widget->Create(isPopup ? nullptr : this, nullptr, aRect, aInitData);
  return NS_FAILED(rv) ? nullptr : widget.forget();
}

NS_IMETHODIMP
EmbedLitePuppetWidget::Destroy()
{
  LOGT();
  if (mOnDestroyCalled) {
    return NS_OK;
  }

  mOnDestroyCalled = true;

  nsIWidget* topWidget = GetTopLevelWidget();
  if (mLayerManager && topWidget == this) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;

  Base::OnDestroy();
  Base::Destroy();

  while (mChildren.Length()) {
    mChildren[0]->SetParent(nullptr);
  }
  mChildren.Clear();

  if (mParent) {
    mParent->mChildren.RemoveElement(this);
  }

  mParent = nullptr;
  mWindow = nullptr;
  mView = nullptr;

  Shutdown();

#if DEBUG
  DumpWidgetTree();
#endif

  return NS_OK;
}

NS_IMETHODIMP
EmbedLitePuppetWidget::Show(bool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  if (mVisible == aState) {
    return NS_OK;
  }

  LOGT("this:%p, state: %i, LM:%p", this, aState, mLayerManager.get());

  bool wasVisible = mVisible;
  mVisible = aState;

  nsIWidget* topWidget = GetTopLevelWidget();
  if (!mVisible && mLayerManager && topWidget == this) {
    mLayerManager->ClearCachedResources();
  }

  if (!wasVisible && mVisible) {
    Resize(mNaturalBounds.width, mNaturalBounds.height, false);
    Invalidate(LayoutDeviceIntRect::FromUnknownRect(mBounds));
  }

  // Only propagate visibility changes for widgets backing EmbedLiteView.
  if (mView) {
    for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
      mChildren[i]->Show(aState);
    }
#if DEBUG
    // No point for dumping the tree for both show and hide calls.
    if (aState) {
      DumpWidgetTree();
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLitePuppetWidget::Resize(double aWidth, double aHeight, bool aRepaint)
{
  nsIntRect oldBounds = mBounds;
  LOGT("sz[%i,%i]->[%g,%g]", oldBounds.width, oldBounds.height, aWidth, aHeight);

  mNaturalBounds.SizeTo(nsIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));
  if (mRotation == mozilla::ROTATION_0 || mRotation == mozilla::ROTATION_180) {
    mBounds.SizeTo(nsIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));
  } else {
    mBounds.SizeTo(nsIntSize(NSToIntRound(aHeight), NSToIntRound(aWidth)));
  }

  // Do not move bounds here. Just alter size based on margins.
  mBounds.y = 0;
  mBounds.x = 0;
  mBounds.width = std::max(0, (mBounds.width - std::max(0, mMargins.left) - std::max(0, mMargins.right)));
  mBounds.height = std::max(0, (mBounds.height - std::max(0, mMargins.top) - std::max(0, mMargins.bottom)));

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->WidgetBoundsChanged(mBounds);
  }

  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->Resize(aWidth, aHeight, aRepaint);
  }

  if (aRepaint) {
    Invalidate(LayoutDeviceIntRect::FromUnknownRect(mBounds));
  }

  nsIWidgetListener* listener =
    mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
  if (!oldBounds.IsEqualEdges(mBounds) && listener) {
    listener->WindowResized(this, mBounds.width, mBounds.height);
  }

  if (mCompositorParent) {
    static_cast<EmbedLiteCompositorParent*>(mCompositorParent.get())->
        SetSurfaceSize(mNaturalBounds.width, mNaturalBounds.height);
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLitePuppetWidget::SetFocus(bool aRaise)
{
  LOGT();
  return NS_OK;
}

NS_IMETHODIMP
EmbedLitePuppetWidget::Invalidate(const LayoutDeviceIntRect& aRect)
{
  Unused << aRect;
  nsIWidgetListener* listener = GetWidgetListener();
  if (listener) {
    listener->WillPaintWindow(this);
  }

  LayerManager* lm = nsIWidget::GetLayerManager();
  if (mozilla::layers::LayersBackend::LAYERS_CLIENT == lm->GetBackendType()) {
    // No need to do anything, the compositor will handle drawing
  } else {
    NS_RUNTIMEABORT("Unexpected layer manager type");
  }

  listener = GetWidgetListener();
  if (listener) {
    listener->DidPaintWindow();
  }

  return NS_OK;
}

void*
EmbedLitePuppetWidget::GetNativeData(uint32_t aDataType)
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

NS_IMETHODIMP
EmbedLitePuppetWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
  LOGT();
  MOZ_ASSERT(event);
  aStatus = nsEventStatus_eIgnore;

  nsIWidgetListener* listener =
    mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;

  NS_ASSERTION(listener, "No listener!");

  if (event->mClass == eKeyboardEventClass) {
    RemoveIMEComposition();
  } else if (event->mClass == eCompositionEventClass) {
    // Store the latest native IME context of parent process's widget or
    // TextEventDispatcher if it's in this process.
    WidgetCompositionEvent* compositionEvent = event->AsCompositionEvent();
#ifdef DEBUG
    if (mNativeIMEContext.IsValid() &&
      mNativeIMEContext != compositionEvent->mNativeIMEContext) {
      RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(this);
      MOZ_ASSERT(!composition,
        "When there is composition caused by old native IME context, "
        "composition events caused by different native IME context are not "
        "allowed");
    }
#endif // #ifdef DEBUG
     mNativeIMEContext = compositionEvent->mNativeIMEContext;
  }

  if (listener) {
    aStatus = listener->HandleEvent(event, mUseAttachedEvents);
  } else {
    aStatus = nsEventStatus_eIgnore;
  }

  switch (event->mMessage) {
    case eCompositionStart:
      MOZ_ASSERT(!mIMEComposing);
      mIMEComposing = true;
      break;
    case eCompositionEnd:
      MOZ_ASSERT(mIMEComposing);
      mIMEComposing = false;
      mIMEComposingText.Truncate();
      break;
    case eCompositionChange:
      MOZ_ASSERT(mIMEComposing);
      mIMEComposingText = static_cast<WidgetCompositionEvent*>(event)->mData;
      break;
    default:
      // Do nothing
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
EmbedLitePuppetWidget::SetInputContext(const InputContext& aContext,
                                       const InputContextAction& aAction)
{
  LOGT("IME: SetInputContext: s=0x%X, 0x%X, action=0x%X, 0x%X",
       aContext.mIMEState.mEnabled, aContext.mIMEState.mOpen,
       aAction.mCause, aAction.mFocusChange);

  if (Destroyed()) {
    LOGT("Trying to focus after puppet widget got destroyed.");
    return;
  }

  // Ensure that opening the virtual keyboard is allowed for this specific
  // InputContext depending on the content.ime.strict.policy pref
  if (aContext.mIMEState.mEnabled != IMEState::DISABLED &&
      aContext.mIMEState.mEnabled != IMEState::PLUGIN &&
      Preferences::GetBool("content.ime.strict_policy", false) &&
      !aAction.ContentGotFocusByTrustedCause() &&
      !aAction.UserMightRequestOpenVKB()) {
    return;
  }

  IMEState::Enabled enabled = aContext.mIMEState.mEnabled;

  // Only show the virtual keyboard for plugins if mOpen is set appropriately.
  // This avoids showing it whenever a plugin is focused. Bug 747492
  if (aContext.mIMEState.mEnabled == IMEState::PLUGIN &&
      aContext.mIMEState.mOpen != IMEState::OPEN) {
      enabled = IMEState::DISABLED;
  }

  mInputContext = aContext;
  mInputContext.mIMEState.mEnabled = enabled;

  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    view->SetInputContext(
      static_cast<int32_t>(aContext.mIMEState.mEnabled),
      static_cast<int32_t>(aContext.mIMEState.mOpen),
      aContext.mHTMLInputType,
      aContext.mHTMLInputInputmode,
      aContext.mActionHint,
      static_cast<int32_t>(aAction.mCause),
      static_cast<int32_t>(aAction.mFocusChange));
  }
}

NS_IMETHODIMP_(InputContext)
EmbedLitePuppetWidget::GetInputContext()
{
  LOGT();
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();

  if (view) {
    int32_t enabled = IMEState::DISABLED;
    int32_t open = IMEState::OPEN_STATE_NOT_SUPPORTED;

    view->GetInputContext(&enabled, &open);
    mInputContext.mIMEState.mEnabled = static_cast<IMEState::Enabled>(enabled);
    mInputContext.mIMEState.mOpen = static_cast<IMEState::Open>(open);
  }

  return mInputContext;
}

NS_IMETHODIMP_(NativeIMEContext)
EmbedLitePuppetWidget::GetNativeIMEContext()
{
  LOGT();
  return mNativeIMEContext;
}

nsIMEUpdatePreference
EmbedLitePuppetWidget::GetIMEUpdatePreference()
{
    LOGT();
#ifdef MOZ_CROSS_PROCESS_IME
  // e10s requires IME content cache in in the TabParent for handling query
  // content event only with the parent process.  Therefore, this process
  // needs to receive a lot of information from the focused editor to sent
  // the latest content to the parent process.
  if (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
    // But if a plugin has focus, we cannot receive text nor selection change
    // in the plugin.  Therefore, PuppetWidget needs to receive only position
    // change event for updating the editor rect cache.
    return nsIMEUpdatePreference(mIMEPreferenceOfParent.mWantUpdates |
                                 nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE);
  }
  return nsIMEUpdatePreference(mIMEPreferenceOfParent.mWantUpdates |
                               nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
                               nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE |
                               nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE );
#else
  // B2G doesn't handle IME as widget-level.
  return nsIMEUpdatePreference();
#endif
}

void
EmbedLitePuppetWidget::RemoveIMEComposition()
{
  LOGT();
  // Remove composition on Gecko side
  if (!mIMEComposing) {
    return;
  }

  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    view->ResetInputState();
  }

  RefPtr<EmbedLitePuppetWidget> kungFuDeathGrip(this);

  WidgetCompositionEvent textEvent(true, eCompositionChange, this);
  textEvent.time = PR_Now() / 1000;
  textEvent.mData = mIMEComposingText;
  nsEventStatus status;
  DispatchEvent(&textEvent, status);

  WidgetCompositionEvent event(true, eCompositionEnd, this);
  event.time = PR_Now() / 1000;
  DispatchEvent(&event, status);
}

GLContext*
EmbedLitePuppetWidget::GetGLContext() const
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

void
EmbedLitePuppetWidget::CreateGLContextEarly(uint32_t aWindowId)
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

LayoutDeviceIntRect EmbedLitePuppetWidget::GetNaturalBounds()
{
  return LayoutDeviceIntRect::FromUnknownRect(mNaturalBounds);
}

bool
EmbedLitePuppetWidget::NeedsPaint()
{
  // Widgets representing EmbedLite view and window don't need to paint anything.
  if (mWindow || mView) {
    return false;
  }
  return nsIWidget::NeedsPaint();
}

LayerManager*
EmbedLitePuppetWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                                       LayersBackend aBackendHint,
                                       LayerManagerPersistence aPersistence,
                                       bool* aAllowRetaining)
{
  if (aAllowRetaining) {
    *aAllowRetaining = true;
  }

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

float
EmbedLitePuppetWidget::GetDPI()
{
  if (mDPI < 0) {
    if (mView) {
      mView->GetDPI(&mDPI);
    } else {
      mDPI = nsBaseWidget::GetDPI();
    }
  }

  return mDPI;
}

bool EmbedLitePuppetWidget::AsyncPanZoomEnabled() const
{
  return true;
}

void EmbedLitePuppetWidget::SetConfirmedTargetAPZC(uint64_t aInputBlockId, const nsTArray<ScrollableLayerGuid> &aTargets) const
{
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  LOGT("view: %p", view, mWindow);
  if (view) {
    view->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void EmbedLitePuppetWidget::UpdateZoomConstraints(const uint32_t &aPresShellId, const FrameMetrics::ViewID &aViewId, const mozilla::Maybe<ZoomConstraints> &aConstraints)
{
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  LOGT("view: %p", view, mWindow);
  if (view) {
    view->UpdateZoomConstraints(aPresShellId,
                                aViewId,
                                aConstraints);
  }
}

CompositorParent*
EmbedLitePuppetWidget::NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight)
{
  LOGT();
  mHasCompositor = true;
  return new EmbedLiteCompositorParent(this, mWindow->GetUniqueID(), true,
                                       aSurfaceWidth, aSurfaceHeight);
}

void EmbedLitePuppetWidget::CreateCompositor()
{
  LOGT();
  // Compositor should be created only for top level widgets, aka windows.
  MOZ_ASSERT(mWindow);
  LayoutDeviceIntRect size = mWindow->GetSize();
  CreateCompositor(size.width, size.height);
}

void EmbedLitePuppetWidget::CreateCompositor(int aWidth, int aHeight)
{
  LOGT();
  nsBaseWidget::CreateCompositor(aWidth, aHeight);
}

void
EmbedLitePuppetWidget::DrawWindowUnderlay(LayerManagerComposite *aManager, LayoutDeviceIntRect aRect)
{
  MOZ_ASSERT(mWindow);
  Unused << aManager;
  Unused << aRect;
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->DrawUnderlay();
  }
}

void
EmbedLitePuppetWidget::DrawWindowOverlay(LayerManagerComposite *aManager, LayoutDeviceIntRect aRect)
{
  MOZ_ASSERT(mWindow);
  Unused << aManager;
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->DrawOverlay(aRect.ToUnknownRect());
  }
}

bool
EmbedLitePuppetWidget::PreRender(LayerManagerComposite *aManager)
{
  MOZ_ASSERT(mWindow);
  Unused << aManager;
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
EmbedLitePuppetWidget::PostRender(LayerManagerComposite *aManager)
{
  MOZ_ASSERT(mWindow);
  Unused << aManager;
  EmbedLiteWindow* window = EmbedLiteApp::GetInstance()->GetWindowByID(mWindow->GetUniqueID());
  if (window) {
    window->GetListener()->CompositingFinished();
  }
}

void
EmbedLitePuppetWidget::AddObserver(EmbedLitePuppetWidgetObserver* obs)
{
  mObservers.AppendElement(obs);
}

void
EmbedLitePuppetWidget::RemoveObserver(EmbedLitePuppetWidgetObserver* obs)
{
  mObservers.RemoveElement(obs);
}

void
EmbedLitePuppetWidget::DumpWidgetTree()
{
  printf_stderr("EmbedLite Puppet Widget Tree:\n");
  DumpWidgetTree(gTopLevelWindows);
}

void
EmbedLitePuppetWidget::DumpWidgetTree(const nsTArray<EmbedLitePuppetWidget*>& widgets, int indent)
{
  for (uint32_t i = 0; i < widgets.Length(); ++i) {
    EmbedLitePuppetWidget *w = widgets[i];
    LogWidget(w, i, indent);
    DumpWidgetTree(w->mChildren, indent + 2);
  }
}

void
EmbedLitePuppetWidget::LogWidget(EmbedLitePuppetWidget *widget, int index, int indent)
{
  char spaces[] = "                    ";
  spaces[indent < 20 ? indent : 20] = 0;
  printf_stderr("%s [% 2d] [%p] size: [(%d, %d), (%3d, %3d)], margins: [%d, %d, %d, %d], "
                "visible: %d, type: %d, rotation: %d, observers: %zu\n",
                spaces, index, widget,
                widget->mBounds.x, widget->mBounds.y,
                widget->mBounds.width, widget->mBounds.height,
                widget->mMargins.top, widget->mMargins.right,
                widget->mMargins.bottom, widget->mMargins.left,
                widget->mVisible, widget->mWindowType,
                widget->mRotation * 90, widget->mObservers.Length());
}

bool
EmbedLitePuppetWidget::DoSendContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid &aGuid, uint64_t aInputBlockId, bool aPreventDefault)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    view->DoSendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
    return true;
  }
  return false;
}

bool
EmbedLitePuppetWidget::DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    return view->DoSendSetAllowedTouchBehavior(aInputBlockId, aFlags);
  }

  return false;
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget()
  : EmbedLitePuppetWidget(nullptr, nullptr)
{
}

EmbedLiteViewChildIface*
EmbedLitePuppetWidget::GetEmbedLiteChildView() const
{
  if (mView) {
    return mView;
  }
  if (mParent) {
    return mParent->GetEmbedLiteChildView();
  }
  return nullptr;
}

void EmbedLitePuppetWidget::ConfigureAPZCTreeManager()
{
  LOGT("Do nothing - APZEventState configured in EmbedLiteViewBaseChild");
}

void EmbedLitePuppetWidget::ConfigureAPZControllerThread()
{
  LOGT("Do nothing - APZController thread configured in EmbedLiteViewBaseParent");
}

already_AddRefed<GeckoContentController>
EmbedLitePuppetWidget::CreateRootContentController()
{
  return nullptr;
}

}  // namespace widget
}  // namespace mozilla
