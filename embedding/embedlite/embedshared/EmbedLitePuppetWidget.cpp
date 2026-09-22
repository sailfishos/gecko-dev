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
#include "nsWindow.h"
#include "nsIWidgetListener.h"

#include "mozilla/Preferences.h"

#ifdef DEBUG
#include "mozilla/TextComposition.h"
#include "mozilla/IMEStateManager.h"
#endif

#include "EmbedLiteApp.h"
#include "mozilla/BasicEvents.h"

using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

NS_IMPL_ISUPPORTS_INHERITED(EmbedLitePuppetWidget, PuppetWidgetBase,
                            nsISupportsWeakReference)

EmbedLitePuppetWidget::EmbedLitePuppetWidget()
  : PuppetWidgetBase()
  , mIMEComposing(false)
  , mDPI(-1.0)
{
  mWidgetType = WidgetType::Puppet;
  MOZ_COUNT_CTOR(EmbedLitePuppetWidget);
  LOGT("Puppet: %p", this);
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
EmbedLitePuppetWidget::CreateForChromeHost(nsIWidget* aHost)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHost);

  RefPtr<EmbedLitePuppetWidget> widget =
    new EmbedLitePuppetWidget();
  widget->mPendingChromeHost = aHost;
  nsCOMPtr<nsIWidget> result = widget.forget();
  return result.forget();
}

nsresult
EmbedLitePuppetWidget::Create(nsIWidget* aParent,
                              const LayoutDeviceIntRect& aRect,
                              const widget::InitData& aInitData)
{
  nsCOMPtr<nsIWidget> chromeHost = mPendingChromeHost;
  mPendingChromeHost = nullptr;
  const bool chromeHosted = !!chromeHost;

  if (chromeHost) {
    if (aParent ||
        aInitData.mWindowType == widget::WindowType::Popup) {
      MOZ_ASSERT_UNREACHABLE(
        "A hosted chrome window must be a parentless top-level widget");
      return NS_ERROR_INVALID_ARG;
    }
    aParent = chromeHost;
  }
  nsresult rv = PuppetWidgetBase::Create(aParent, aRect, aInitData);
  if (NS_SUCCEEDED(rv) && chromeHosted) {
    // The host is already visible, but AppWindow still needs its normal
    // hidden-to-visible transition to invalidate and paint the chrome.
    mVisible = false;
    static_cast<nsWindow*>(chromeHost.get())->AttachChromeHostedWidget(this);
  }
  return rv;
}

already_AddRefed<nsIWidget>
EmbedLitePuppetWidget::AllocateChildPuppetWidget(const widget::InitData&)
{
  if (Destroyed()) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = new EmbedLitePuppetWidget();
  return widget.forget();
}

void EmbedLitePuppetWidget::Destroy()
{
  RefPtr<EmbedLitePuppetWidget> self(this);
  RefPtr<nsWindow> chromeHost = dynamic_cast<nsWindow*>(GetParent());
  if (chromeHost) {
    chromeHost->DetachChromeHostedWidget(this);
  }
  PuppetWidgetBase::Destroy();
}

void
EmbedLitePuppetWidget::Show(bool aState)
{
  if (Destroyed() || !WillShow(aState)) {
    return;
  }

  PuppetWidgetBase::Show(aState);

}

void*
EmbedLitePuppetWidget::GetNativeData(uint32_t aDataType)
{
  if (Destroyed()) {
    return nullptr;
  }

  LOGT("t: %p, DataType: %i", this, aDataType);
  switch (aDataType) {
    case NS_NATIVE_OPENGL_CONTEXT:
      return nullptr;
    case NS_NATIVE_WINDOW:
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

nsEventStatus
EmbedLitePuppetWidget::DispatchEvent(WidgetGUIEvent* event)
{
  if (Destroyed()) {
    return nsEventStatus_eIgnore;
  }

  LOGT();
  MOZ_ASSERT(event);
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

  nsEventStatus status = nsIWidget::DispatchEvent(event);

  switch (event->mMessage) {
    case eCompositionStart:
      MOZ_ASSERT(!mIMEComposing);
      mIMEComposing = true;
      break;
    case eCompositionEnd:
    case eCompositionCommit:
    case eCompositionCommitAsIs:
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

  return status;
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

  if (nsWindow* chromeHost = dynamic_cast<nsWindow*>(GetParent())) {
    mInputContext = aContext;
    chromeHost->SetChromeInputContext(aContext, aAction);
  }
}

InputContext
EmbedLitePuppetWidget::GetInputContext()
{
  LOGT();
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

  RefPtr<EmbedLitePuppetWidget> kungFuDeathGrip(this);

  WidgetCompositionEvent textEvent(true, eCompositionChange, this);
  textEvent.mTimeStamp = TimeStamp::Now();
  textEvent.mData = mIMEComposingText;
  DispatchEvent(&textEvent);

  WidgetCompositionEvent event(true, eCompositionEnd, this);
  event.mTimeStamp = TimeStamp::Now();
  DispatchEvent(&event);
}

bool
EmbedLitePuppetWidget::NeedsPaint()
{
  if (Destroyed()) {
    return false;
  }
  return nsIWidget::NeedsPaint();
}

float
EmbedLitePuppetWidget::GetDPI()
{
  if (GetParent()) {
    return PuppetWidgetBase::GetDPI();
  }

  if (mDPI < 0) {
    mDPI = nsIWidget::GetFallbackDPI();
  }

  return mDPI;
}

bool EmbedLitePuppetWidget::AsyncPanZoomEnabled() const
{
  return true;
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

WindowRenderer *
EmbedLitePuppetWidget::GetWindowRenderer()
{
  if (!mWindowRenderer) {
    if (!mShutdownObserver || Destroyed()) {
      // We are shutting down, do not try to re-create a WindowRenderer.
      return nullptr;
    }
  }

  WindowRenderer* windowRenderer = PuppetWidgetBase::GetWindowRenderer();
  if (windowRenderer) {
    return windowRenderer;
  }

  if (nsIWidget* parent = GetParent()) {
    // Borrow the parent renderer for painting, but do not cache it in this
    // child widget. mWindowRenderer is owning, and child-widget teardown must
    // not destroy the root WebRender layer manager.
    return parent->GetWindowRenderer();
  }

  return nullptr;
}

void EmbedLitePuppetWidget::AddObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.AppendElement(aObserver);
}

void EmbedLitePuppetWidget::RemoveObserver(EmbedLitePuppetWidgetObserver *aObserver)
{
  mObservers.RemoveElement(aObserver);
}

void EmbedLitePuppetWidget::NotifyChromeWindowFocusChanged(bool aFocused)
{
  // Top-level activation belongs to the primary listener.  The attached
  // listener handles painting and input events for the hosted view, but does
  // not update the AppWindow's focus-manager state.
  nsIWidgetListener* listener = GetWidgetListener();
  if (!listener) {
    return;
  }

  if (aFocused) {
    listener->WindowActivated();
  } else {
    listener->WindowDeactivated();
  }
}

void EmbedLitePuppetWidget::ConfigureAPZCTreeManager()
{
  LOGT("APZEventState is configured by the hosted root nsWindow");
}

void EmbedLitePuppetWidget::ConfigureAPZControllerThread()
{
  LOGT("APZ controller thread is configured by the hosted root nsWindow");
}

already_AddRefed<layers::GeckoContentController>
EmbedLitePuppetWidget::CreateRootContentController()
{
  return nullptr;
}

}  // namespace widget
}  // namespace mozilla
