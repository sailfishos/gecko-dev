/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "base/basictypes.h"

#include "gfxPlatform.h"

#include "EmbedLitePuppetWidget.h"
#include "nsIWidgetListener.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"

#include "mozilla/Preferences.h"

#ifdef DEBUG
#include "mozilla/TextComposition.h"
#include "mozilla/IMEStateManager.h"
#endif

#include "EmbedLiteApp.h"
#include "LayerScope.h"
#include "mozilla/Unused.h"
#include "mozilla/BasicEvents.h"

#include <sys/syscall.h>

using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

NS_IMPL_ISUPPORTS_INHERITED(EmbedLitePuppetWidget, PuppetWidgetBase,
                            nsISupportsWeakReference)

static bool
IsPopup(const nsWidgetInitData* aInitData)
{
  return aInitData && aInitData->mWindowType == eWindowType_popup;
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget(EmbedLiteViewChildIface* view)
  : PuppetWidgetBase()
  , mView(view)
  , mIMEComposing(false)
  , mDPI(-1.0)
{
  MOZ_COUNT_CTOR(EmbedLitePuppetWidget);
  LOGT("Puppet: %p, view: %p", this, mView);
}

EmbedLitePuppetWidget::~EmbedLitePuppetWidget()
{
  MOZ_COUNT_DTOR(EmbedLitePuppetWidget);
  LOGT("this: %p", this);
}

const char *EmbedLitePuppetWidget::Type() const
{
  return "EmbedLitePuppetWidget";
}

already_AddRefed<nsIWidget>
EmbedLitePuppetWidget::CreateChild(const LayoutDeviceIntRect &aRect,
                                   nsWidgetInitData* aInitData,
                                   bool              aForceUseIWidgetParent)
{
  if (Destroyed()) {
    return nullptr;
  }

  LOGT();
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = new EmbedLitePuppetWidget(nullptr);
  nsresult rv = widget->Create(isPopup ? nullptr : this, nullptr, aRect, aInitData);
  return NS_FAILED(rv) ? nullptr : widget.forget();
}

void EmbedLitePuppetWidget::Destroy()
{
  PuppetWidgetBase::Destroy();
  mView = nullptr;
}

void
EmbedLitePuppetWidget::Show(bool aState)
{
  if (Destroyed() || !WillShow(aState)) {
    return;
  }

  PuppetWidgetBase::Show(aState);

  // Only propagate visibility changes for widgets backing EmbedLiteView.
  if (mView) {
    for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
      mChildren[i]->Show(aState);
    }
  }
}

void*
EmbedLitePuppetWidget::GetNativeData(uint32_t aDataType)
{
  if (Destroyed()) {
    return nullptr;
  }

  LOGT("t: %p, DataType: %i", this, aDataType);
  switch (aDataType) {
    case NS_NATIVE_SHAREABLE_WINDOW: {
      LOGW("aDataType: %i\n", aDataType);
      return (void*)nullptr;
    }
    case NS_NATIVE_OPENGL_CONTEXT:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_DISPLAY:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_SHELLWIDGET:
    case NS_NATIVE_WIDGET:
      LOGW("EmbedLitePuppetWidget::GetNativeData not implemented for this type");
      break;
    case NS_RAW_NATIVE_IME_CONTEXT:
      return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
    default:
      NS_WARNING("EmbedLitePuppetWidget::GetNativeData called with bad value");
      break;
  }

  return nullptr;
}

nsresult
EmbedLitePuppetWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
  if (Destroyed()) {
    return NS_OK;
  }

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

void
EmbedLitePuppetWidget::SetInputContext(const InputContext& aContext,
                                       const InputContextAction& aAction)
{
  if (Destroyed()) {
    LOGT("Trying to focus after puppet widget got destroyed.");
    return;
  }

  LOGT("IME: SetInputContext: s=0x%X, 0x%X, action=0x%X, 0x%X",
       aContext.mIMEState.mEnabled, aContext.mIMEState.mOpen,
       aAction.mCause, aAction.mFocusChange);

  // Ensure that opening the virtual keyboard is allowed for this specific
  // InputContext depending on the content.ime.strict.policy pref
  if (aContext.mIMEState.mEnabled != IMEEnabled::Disabled &&
      Preferences::GetBool("content.ime.strict_policy", false) &&
      !aAction.ContentGotFocusByTrustedCause() &&
      !aAction.UserMightRequestOpenVKB()) {
    return;
  }

  IMEEnabled enabled = aContext.mIMEState.mEnabled;

  // Only show the virtual keyboard for plugins if mOpen is set appropriately.
  // This avoids showing it whenever a plugin is focused. Bug 747492
  if (aContext.mIMEState.mOpen != IMEState::OPEN) {
      enabled = IMEEnabled::Disabled;
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

InputContext
EmbedLitePuppetWidget::GetInputContext()
{
  LOGT();
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();

  if (view) {
    int32_t enabled = static_cast<int32_t>(IMEEnabled::Disabled);
    int32_t open = IMEState::OPEN_STATE_NOT_SUPPORTED;

    view->GetInputContext(&enabled, &open);
    mInputContext.mIMEState.mEnabled = static_cast<IMEEnabled>(enabled);
    mInputContext.mIMEState.mOpen = static_cast<IMEState::Open>(open);
  }

  return mInputContext;
}

NativeIMEContext
EmbedLitePuppetWidget::GetNativeIMEContext()
{
  LOGT();
  return mNativeIMEContext;
}

void
EmbedLitePuppetWidget::RemoveIMEComposition()
{
  LOGT();
  // Remove composition on Gecko side
  if (Destroyed() || !mIMEComposing) {
    return;
  }

  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    view->ResetInputState();
  }

  RefPtr<EmbedLitePuppetWidget> kungFuDeathGrip(this);

  WidgetCompositionEvent textEvent(true, eCompositionChange, this);
  textEvent.mTime = PR_Now() / 1000;
  textEvent.mData = mIMEComposingText;
  nsEventStatus status;
  DispatchEvent(&textEvent, status);

  WidgetCompositionEvent event(true, eCompositionEnd, this);
  event.mTime = PR_Now() / 1000;
  DispatchEvent(&event, status);
}

EmbedLitePuppetWidget *
EmbedLitePuppetWidget::GetParentPuppetWidget() const
{
  return dynamic_cast<EmbedLitePuppetWidget *>(mParent);
}

bool
EmbedLitePuppetWidget::NeedsPaint()
{
  // Widgets representing EmbedLite view and window don't need to paint anything.
  if (Destroyed() || mView) {
    return false;
  }
  return nsIWidget::NeedsPaint();
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
  LOGT("view: %p", view);
  if (view) {
    view->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void EmbedLitePuppetWidget::UpdateZoomConstraints(const uint32_t &aPresShellId, const ScrollableLayerGuid::ViewID &aViewId, const mozilla::Maybe<ZoomConstraints> &aConstraints)
{
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  LOGT("view: %p", view);
  if (view) {
    view->UpdateZoomConstraints(aPresShellId,
                                aViewId,
                                aConstraints);
  }
}

void EmbedLitePuppetWidget::CreateCompositor()
{
  MOZ_ASSERT(false, "nsWindow will create compositor");
}

void EmbedLitePuppetWidget::CreateCompositor(int aWidth, int aHeight)
{
  (void)aWidth;
  (void)aHeight;
  MOZ_ASSERT(false, "nsWindow will create compositor with size");

}

LayerManager *
EmbedLitePuppetWidget::GetLayerManager(PLayerTransactionChild *aShadowManager, LayersBackend aBackendHint, LayerManagerPersistence aPersistence)
{
  if (!mLayerManager) {
    if (!mShutdownObserver || Destroyed()) {
      // We are shutting down, do not try to re-create a LayerManager
      return nullptr;
    }
  }

  LayerManager *lm = PuppetWidgetBase::GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  if (lm) {
    mLayerManager = lm;
    return mLayerManager;
  }

  if (EmbedLiteApp::GetInstance()->GetType() == EmbedLiteApp::EMBED_INVALID) {
    LOGT("Create Layer Manager for Process View");

    mLayerManager = new ClientLayerManager(this);
    ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder();
    if (!lf->HasShadowManager() && aShadowManager) {
      lf->SetShadowManager(aShadowManager);
    }
    return mLayerManager;
  }

  nsIWidget* topWidget = GetTopLevelWidget();
  if (topWidget && topWidget != this) {
      mLayerManager = topWidget->GetLayerManager(aShadowManager, aBackendHint, aPersistence);
      return mLayerManager;
  }
  else {
      return nullptr;
  }
}

bool
EmbedLitePuppetWidget::DoSendContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault)
{
  if (Destroyed()) {
    return false;
  }

  LOGT("thread id: %ld", syscall(SYS_gettid));
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    view->DoSendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
    return true;
  }
  return false;
}

bool
EmbedLitePuppetWidget::DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags)
{
  if (Destroyed()) {
    return false;
  }

  LOGT("thread id: %ld", syscall(SYS_gettid));
  EmbedLiteViewChildIface* view = GetEmbedLiteChildView();
  if (view) {
    return view->DoSendSetAllowedTouchBehavior(aInputBlockId, aFlags);
  }

  return false;
}

void EmbedLitePuppetWidget::AddObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.AppendElement(aObserver);
}

void EmbedLitePuppetWidget::RemoveObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.RemoveElement(aObserver);
}

EmbedLitePuppetWidget::EmbedLitePuppetWidget()
  : EmbedLitePuppetWidget(nullptr)
{
}

EmbedLiteViewChildIface*
EmbedLitePuppetWidget::GetEmbedLiteChildView() const
{
  if (mView) {
    return mView;
  }

  if (EmbedLitePuppetWidget *parentWidget = GetParentPuppetWidget()) {
    return parentWidget->GetEmbedLiteChildView();
  }
  return nullptr;
}

void EmbedLitePuppetWidget::ConfigureAPZCTreeManager()
{
  LOGT("Do nothing - APZEventState configured in EmbedLiteViewChild");
}

void EmbedLitePuppetWidget::ConfigureAPZControllerThread()
{
  LOGT("Do nothing - APZController thread configured in EmbedLiteViewParent");
}

already_AddRefed<GeckoContentController>
EmbedLitePuppetWidget::CreateRootContentController()
{
  return nullptr;
}

}  // namespace widget
}  // namespace mozilla
