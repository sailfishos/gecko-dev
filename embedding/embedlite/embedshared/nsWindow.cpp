/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include <stdint.h>

#include "nsWindow.h"
#include "EmbedLiteHostedWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteApp.h"

#include "base/basictypes.h"

#include "mozilla/Hal.h"
#include "mozilla/GlobalKeyListener.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/ChromeProcessController.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/PresShell.h"

using namespace mozilla::layers;
using namespace mozilla::widget;
//using namespace mozilla::ipc;

namespace mozilla {
namespace embedlite {

class EmbedLiteChromeInputTransactionListener final
  : public TextEventDispatcherListener
{
public:
  NS_DECL_ISUPPORTS

  EmbedLiteChromeInputTransactionListener() = default;

  NS_IMETHOD NotifyIME(TextEventDispatcher* aDispatcher,
                       const IMENotification& aNotification) override
  {
    if (!aDispatcher) {
      return NS_ERROR_INVALID_ARG;
    }

    switch (aNotification.mMessage) {
      case REQUEST_TO_COMMIT_COMPOSITION:
      case REQUEST_TO_CANCEL_COMPOSITION: {
        RefPtr<TextEventDispatcher> dispatcher = aDispatcher;
        if (!dispatcher->IsComposing()) {
          return NS_OK;
        }
        nsEventStatus status = nsEventStatus_eIgnore;
        if (aNotification.mMessage == REQUEST_TO_COMMIT_COMPOSITION) {
          return dispatcher->CommitComposition(status, nullptr);
        }
        const nsString empty;
        return dispatcher->CommitComposition(status, &empty);
      }
      default:
        return NS_OK;
    }
  }

  NS_IMETHOD_(IMENotificationRequests)
  GetIMENotificationRequests() override
  {
    return IMENotificationRequests{};
  }

  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher*) override
  {
  }

  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher*, WidgetKeyboardEvent&,
                            uint32_t, void*) override
  {
  }

private:
  ~EmbedLiteChromeInputTransactionListener() = default;
};

NS_IMPL_ISUPPORTS(EmbedLiteChromeInputTransactionListener,
                  TextEventDispatcherListener,
                  nsISupportsWeakReference)

namespace {

KeyNameIndex ChromeKeyNameIndex(int32_t aDomKeyCode)
{
#define KEY(key_, _codeNameIdx, _keyCode, _modifier)
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode) \
  if (aDomKeyCode == _keyCode) {                         \
    return KEY_NAME_INDEX_##keyNameIdx_;                 \
  }
#include "KeyCodeConsensus_En_US.inc"
  return KEY_NAME_INDEX_USE_STRING;
#undef CONTROL
#undef KEY
}

CodeNameIndex ChromeCodeNameIndex(int32_t aCharCode)
{
  switch (aCharCode) {
#define KEY(key_, _codeNameIdx, _keyCode, _modifier) \
    case key_[0]:                                      \
      return CODE_NAME_INDEX_##_codeNameIdx;
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode)
#include "KeyCodeConsensus_En_US.inc"
    default:
      return CODE_NAME_INDEX_UNKNOWN;
#undef CONTROL
#undef KEY
  }
}

Modifiers ChromeKeyModifiers(int32_t aCharCode)
{
  switch (aCharCode) {
#define KEY(key_, _codeNameIdx, _keyCode, _modifier) \
    case key_[0]:                                      \
      return _modifier;
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode)
#include "KeyCodeConsensus_En_US.inc"
    default:
      return 0;
#undef CONTROL
#undef KEY
  }
}

void InitializeChromeKeyEvent(WidgetKeyboardEvent& aEvent,
                              int32_t aDomKeyCode, int32_t aModifiers,
                              int32_t aCharCode)
{
  aEvent.mModifiers =
    Modifiers(aModifiers) | ChromeKeyModifiers(aCharCode);
  aEvent.mKeyCode = aCharCode ? 0 : aDomKeyCode;
  aEvent.mCharCode = aCharCode;
  aEvent.mLocation = eKeyLocationStandard;
  aEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
  aEvent.mTimeStamp = TimeStamp::Now();
  aEvent.mKeyNameIndex = ChromeKeyNameIndex(aDomKeyCode);
  aEvent.mKeyValue.Assign(static_cast<char16_t>(aCharCode));
  aEvent.mCodeNameIndex = ChromeCodeNameIndex(aCharCode);
  if (aDomKeyCode == dom::KeyboardEvent_Binding::DOM_VK_RETURN) {
    aEvent.mKeyNameIndex = KEY_NAME_INDEX_Enter;
  }
}

} // namespace

NS_IMPL_ISUPPORTS_INHERITED(nsWindow, PuppetWidgetBase,
                            nsISupportsWeakReference)

AutoEmbedLiteChromeWindowHost*
AutoEmbedLiteChromeWindowHost::sPendingHost = nullptr;

AutoEmbedLiteChromeWindowHost::AutoEmbedLiteChromeWindowHost(nsWindow* aHost)
  : mConsumed(false)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aHost && !sPendingHost) {
    mHost = aHost;
    sPendingHost = this;
  }
}

AutoEmbedLiteChromeWindowHost::~AutoEmbedLiteChromeWindowHost()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sPendingHost == this) {
    sPendingHost = nullptr;
  }
}

bool
AutoEmbedLiteChromeWindowHost::IsValid() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mHost && !mHost->Destroyed() && sPendingHost == this;
}

already_AddRefed<nsIWidget>
AutoEmbedLiteChromeWindowHost::ConsumePending()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sPendingHost) {
    return nullptr;
  }
  return sPendingHost->Consume();
}

already_AddRefed<nsIWidget>
AutoEmbedLiteChromeWindowHost::Consume()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsValid()) {
    return nullptr;
  }

  nsCOMPtr<nsIWidget> widget =
    EmbedLitePuppetWidget::CreateForChromeHost(mHost);
  if (!widget) {
    return nullptr;
  }

  mConsumed = true;
  mHost = nullptr;
  sPendingHost = nullptr;
  return widget.forget();
}

nsWindow::nsWindow(EmbedLiteHostedWindow *window)
  : PuppetWidgetBase()
  , mFirstViewCreated(false)
  , mChromeInputReady(false)
  , mChromeWindowFocused(false)
  , mWindow(window)
  , mChromeHostedWidget(nullptr)
{
  LOGT("nsWindow: %p window: %p", this, mWindow);
}

nsresult
nsWindow::Create(nsIWidget *aParent, const LayoutDeviceIntRect &aRect,
                 const widget::InitData& aInitData)
{
  LOGT();
  (void) PuppetWidgetBase::Create(aParent, aRect, aInitData);
  gfxPlatform::GetPlatform();

#if DEBUG
  DumpWidgetTree();
#endif

  return NS_OK;
}

void
nsWindow::Destroy()
{
  RefPtr<nsWindow> self(this);
  EndChromeInputTransaction();
  mChromeHostedWidget = nullptr;
  mChromeInputReady = false;
  mChromeWindowFocused = false;
  mWindow = nullptr;

  PuppetWidgetBase::Destroy();

  Shutdown();
#if DEBUG
  DumpWidgetTree();
#endif
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
nsWindow::Resize(const DesktopSize& aSize, bool aRepaint)
{
  PuppetWidgetBase::Resize(aSize, aRepaint);
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

float
nsWindow::GetDPI()
{
  return mWindow ? mWindow->GetDPI() : PuppetWidgetBase::GetDPI();
}

double
nsWindow::GetDefaultScaleInternal()
{
  return mWindow ? mWindow->GetDensity()
                 : PuppetWidgetBase::GetDefaultScaleInternal();
}

void
nsWindow::BackingScaleFactorChanged()
{
  NotifyBackingScaleFactorChanged();
  NotifyAPZOfDPIChange();
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
  nsIWidget::CreateCompositor(aWidth, aHeight);
}

void *
nsWindow::GetNativeData(uint32_t aDataType)
{
  LOGT("t:%p, DataType: %i", this, aDataType);
  switch (aDataType) {
    case NS_NATIVE_OPENGL_CONTEXT:
      return nullptr;
    case NS_NATIVE_WINDOW:
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

WindowRenderer *
nsWindow::GetWindowRenderer()
{
  LOGC("EmbedLiteLayerManager", "lm: %p", mWindowRenderer.get());

  if (!mWindowRenderer) {
    if (!mShutdownObserver) {
      // We are shutting down, do not try to re-create a WindowRenderer
      return nullptr;
    }
  }

  WindowRenderer* windowRenderer = PuppetWidgetBase::GetWindowRenderer();
  LOGC("EmbedLiteWindowRenderer", "lm: %p this: %p", windowRenderer, this);

  if (windowRenderer) {
    mWindowRenderer = windowRenderer;
    return mWindowRenderer;
  }

  if (mWindow && ShouldUseOffMainThreadCompositing()) {
    CreateCompositor();
    LOGC("EmbedLiteWindowRenderer", "Created compositor, lm: %p", mWindowRenderer.get());
    if (mWindowRenderer) {
      return mWindowRenderer;
    }
  }
  return mWindowRenderer;
}

void
nsWindow::ScheduleWebRenderComposite()
{
  WindowRenderer *renderer = GetWindowRenderer();
  WebRenderLayerManager *wrRenderer =
    renderer ? renderer->AsWebRender() : nullptr;
  if (wrRenderer && !wrRenderer->IsDestroyed() && wrRenderer->WrBridge()) {
    wrRenderer->ScheduleComposite(wr::RenderReasons::WIDGET);
  }
}

bool
nsWindow::PreRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  (void) aContext;
  if (!IsVisible() || !mActive) {
    return false;
  }

  if (GetCompositorBridgeParent()) {
    return true;
  }

  return mWindow->GetListener()->PreRender();
}

void
nsWindow::PostRender(mozilla::widget::WidgetRenderingContext *aContext)
{
  MOZ_ASSERT(mWindow);
  (void) aContext;

  if (GetCompositorBridgeParent()) {
    static_cast<EmbedLiteCompositorBridgeParent*>(
      GetCompositorBridgeParent())->WebRenderComposited();
  } else if (mWindow) {
    mWindow->GetListener()->CompositingFinished();
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

layers::LayersId nsWindow::GetRootLayerId() const
{
  return mCompositorSession ? mCompositorSession->RootLayerTreeId() : layers::LayersId{0};
}

RefPtr<mozilla::layers::IAPZCTreeManager> nsWindow::GetAPZCTreeManager()
{
  if (mCompositorSession) {
    return mAPZC;
  }

  return nullptr;
}

void
nsWindow::AttachChromeHostedWidget(EmbedLitePuppetWidget* aWidget)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWidget);
  MOZ_ASSERT(!mChromeHostedWidget || mChromeHostedWidget == aWidget);

  mChromeHostedWidget = aWidget;
  mChromeWindowFocused = false;
}

void
nsWindow::DetachChromeHostedWidget(EmbedLitePuppetWidget* aWidget)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mChromeHostedWidget != aWidget) {
    return;
  }

  RefPtr<nsWindow> self(this);
  EndChromeInputTransaction();
  mChromeInputReady = false;
  mChromeWindowFocused = false;
  mChromeHostedWidget = nullptr;
  ReleaseContentController();
}

void
nsWindow::InitializeChromeInput()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mChromeHostedWidget) {
    return;
  }

  mChromeInputReady = true;
  if (mAPZC && mCompositorSession) {
    ConfigureChromeAPZ();
  }
}

bool
nsWindow::DispatchChromeInputEvent(WidgetInputEvent* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (Destroyed() || !mChromeInputReady || !mChromeHostedWidget ||
      !mAPZC || !mAPZEventState || !aEvent) {
    return false;
  }

  aEvent->mWidget = this;
  (void) nsIWidget::DispatchInputEvent(aEvent);
  return true;
}

bool
nsWindow::SetChromeMargins(const LayoutDeviceIntMargin& aMargins)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed()) {
    return false;
  }
  widget->SetMargins(aMargins);
  widget->UpdateBounds(true);
  return true;
}

bool
nsWindow::SetChromeSafeAreaInsets(const LayoutDeviceIntMargin& aInsets)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed()) {
    return false;
  }
  widget->SetSafeAreaInsets(aInsets);
  return true;
}

bool
nsWindow::DispatchChromeTextEvent(
    const nsAString& aCommit, const nsAString& aPreEdit,
    int32_t aReplacementStart, int32_t aReplacementLength)
{
  return DispatchChromeTextEventInternal(
    aCommit, aPreEdit, aReplacementStart, 0, aReplacementLength, false);
}

bool
nsWindow::DispatchChromeTextEventAtOffset(
    const nsAString& aCommit, const nsAString& aPreEdit,
    uint32_t aReplacementOffset, int32_t aReplacementLength)
{
  return DispatchChromeTextEventInternal(
    aCommit, aPreEdit, 0, aReplacementOffset, aReplacementLength, true);
}

bool
nsWindow::DispatchChromeTextEventInternal(
    const nsAString& aCommit, const nsAString& aPreEdit,
    int32_t aReplacementStart, uint32_t aReplacementOffset,
    int32_t aReplacementLength, bool aUseReplacementOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsWindow> self(this);
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed() ||
      aReplacementLength < 0 ||
      widget->GetInputContext().mIMEState.mEnabled == IMEEnabled::Disabled) {
    return false;
  }

  if (!mChromeInputTransactionListener) {
    mChromeInputTransactionListener =
      new EmbedLiteChromeInputTransactionListener;
  }
  RefPtr<TextEventDispatcher> dispatcher = widget->GetTextEventDispatcher();
  if (!dispatcher || NS_FAILED(dispatcher->BeginInputTransaction(
                       mChromeInputTransactionListener))) {
    return false;
  }

  auto stillAttached = [&]() {
    return !Destroyed() && mChromeInputReady &&
      mChromeHostedWidget == widget && !widget->Destroyed();
  };

  if (aReplacementLength > 0) {
    uint32_t replacementOffset = aReplacementOffset;
    if (!aUseReplacementOffset) {
      WidgetQueryContentEvent querySelection(
        true, eQuerySelectedText, widget);
      widget->DispatchEvent(&querySelection);
      if (!querySelection.FoundSelection() || !stillAttached()) {
        return false;
      }

      const int64_t relativeOffset =
        static_cast<int64_t>(querySelection.mReply->StartOffset()) +
        aReplacementStart;
      if (relativeOffset < 0 || relativeOffset > UINT32_MAX) {
        return false;
      }
      replacementOffset = static_cast<uint32_t>(relativeOffset);
    }
    if (static_cast<uint64_t>(aReplacementLength) >
        UINT32_MAX - static_cast<uint64_t>(replacementOffset)) {
      return false;
    }

    WidgetSelectionEvent selection(true, eSetSelection, widget);
    selection.mOffset = replacementOffset;
    selection.mLength = static_cast<uint32_t>(aReplacementLength);
    selection.mReversed = false;
    selection.mExpandToClusterBoundary = false;
    widget->DispatchEvent(&selection);
    if (!selection.mSucceeded || !stillAttached()) {
      return false;
    }

    WidgetContentCommandEvent deleteSelection(
      true, eContentCommandDelete, widget);
    widget->DispatchEvent(&deleteSelection);
    if (!deleteSelection.mSucceeded || !stillAttached()) {
      return false;
    }
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  if (!aCommit.IsEmpty()) {
    if (NS_FAILED(dispatcher->CommitComposition(status, &aCommit)) ||
        !stillAttached()) {
      return false;
    }
  }

  if (!aPreEdit.IsEmpty()) {
    if (!dispatcher->IsComposing()) {
      status = nsEventStatus_eIgnore;
      if (NS_FAILED(dispatcher->StartComposition(status)) ||
          !dispatcher->IsComposing() || !stillAttached()) {
        return false;
      }
    }
    if (NS_FAILED(dispatcher->SetPendingCompositionString(aPreEdit)) ||
        NS_FAILED(dispatcher->AppendClauseToPendingComposition(
          aPreEdit.Length(), TextRangeType::eRawClause)) ||
        NS_FAILED(dispatcher->SetCaretInPendingComposition(
          aPreEdit.Length(), 0))) {
      dispatcher->ClearPendingComposition();
      return false;
    }
    status = nsEventStatus_eIgnore;
    return NS_SUCCEEDED(dispatcher->FlushPendingComposition(status)) &&
      stillAttached();
  }

  if (aCommit.IsEmpty() && dispatcher->IsComposing()) {
    const nsString empty;
    status = nsEventStatus_eIgnore;
    return NS_SUCCEEDED(dispatcher->CommitComposition(status, &empty)) &&
      stillAttached();
  }

  return true;
}

bool
nsWindow::DispatchChromeKeyPress(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsWindow> self(this);
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed() ||
      aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return false;
  }

  if (!mChromeInputTransactionListener) {
    mChromeInputTransactionListener =
      new EmbedLiteChromeInputTransactionListener;
  }
  RefPtr<TextEventDispatcher> dispatcher = widget->GetTextEventDispatcher();
  if (!dispatcher || NS_FAILED(dispatcher->BeginInputTransaction(
                       mChromeInputTransactionListener))) {
    return false;
  }

  WidgetKeyboardEvent event(true, eKeyDown, widget);
  InitializeChromeKeyEvent(event, aDomKeyCode, aModifiers, aCharCode);
  nsEventStatus status = nsEventStatus_eIgnore;
  if (!dispatcher->DispatchKeyboardEvent(eKeyDown, event, status)) {
    return false;
  }
  if (Destroyed() || !mChromeInputReady || mChromeHostedWidget != widget ||
      widget->Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
    return true;
  }

  (void) dispatcher->MaybeDispatchKeypressEvents(event, status);
  return true;
}

bool
nsWindow::DispatchChromeKeyRelease(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsWindow> self(this);
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed() ||
      aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return false;
  }

  if (!mChromeInputTransactionListener) {
    mChromeInputTransactionListener =
      new EmbedLiteChromeInputTransactionListener;
  }
  RefPtr<TextEventDispatcher> dispatcher = widget->GetTextEventDispatcher();
  if (!dispatcher || NS_FAILED(dispatcher->BeginInputTransaction(
                       mChromeInputTransactionListener))) {
    return false;
  }

  WidgetKeyboardEvent event(true, eKeyUp, widget);
  InitializeChromeKeyEvent(event, aDomKeyCode, aModifiers, aCharCode);
  nsEventStatus status = nsEventStatus_eIgnore;
  return dispatcher->DispatchKeyboardEvent(eKeyUp, event, status);
}

bool nsWindow::SetChromeFocused(bool aFocused)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsWindow> self(this);
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  if (Destroyed() || !mChromeInputReady || !widget || widget->Destroyed()) {
    return false;
  }
  if (mChromeWindowFocused == aFocused) {
    return true;
  }

  mChromeWindowFocused = aFocused;
  widget->NotifyChromeWindowFocusChanged(aFocused);
  return true;
}

void nsWindow::SetChromeInputContext(
    const InputContext& aContext, const InputContextAction& aAction)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (Destroyed() || !mChromeHostedWidget || !mWindow) {
    return;
  }
  mWindow->ChromeInputContextChanged(aContext, aAction);
}

void nsWindow::EndChromeInputTransaction()
{
  RefPtr<EmbedLitePuppetWidget> widget = mChromeHostedWidget;
  RefPtr<EmbedLiteChromeInputTransactionListener> listener =
    mChromeInputTransactionListener;
  mChromeInputTransactionListener = nullptr;
  if (!widget || !listener || widget->Destroyed()) {
    return;
  }

  RefPtr<TextEventDispatcher> dispatcher = widget->GetTextEventDispatcher();
  if (!dispatcher || dispatcher->IsDispatchingEvent()) {
    return;
  }
  if (dispatcher->IsComposing()) {
    const nsString empty;
    nsEventStatus status = nsEventStatus_eIgnore;
    (void) dispatcher->CommitComposition(status, &empty);
  }
  if (!widget->Destroyed() && !dispatcher->IsComposing() &&
      !dispatcher->IsDispatchingEvent()) {
    dispatcher->EndInputTransaction(listener);
  }
}

nsWindow::~nsWindow()
{
  LOGT("this: %p", this);
}

void
nsWindow::ConfigureAPZCTreeManager()
{
  if (mChromeInputReady && mChromeHostedWidget) {
    ConfigureChromeAPZ();
    return;
  }
  LOGT("Chrome APZ waits for the hosted AppWindow widget");
}

void
nsWindow::ConfigureAPZControllerThread()
{
  LOGT("APZ controller thread is configured by EmbedLiteWindowParent");
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController()
{
  return nullptr;
}

void
nsWindow::ConfigureChromeAPZ()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAPZC);
  MOZ_ASSERT(mCompositorSession);
  MOZ_ASSERT(mChromeHostedWidget);
  if (mRootContentController) {
    return;
  }

  mAPZC->SetDPI(GetDPI());
  if (StaticPrefs::apz_keyboard_enabled_AtStartup()) {
    KeyboardMap map =
      RootWindowGlobalKeyListener::CollectKeyboardShortcuts();
    mAPZC->SetKeyboardMap(map);
  }

  ContentReceivedInputBlockCallback callback(
    [treeManager = RefPtr{mAPZC.get()}](uint64_t aInputBlockId,
                                        bool aPreventDefault) {
      MOZ_ASSERT(NS_IsMainThread());
      treeManager->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
    });

  ReleaseContentController();
  mAPZEventState =
    new APZEventState(mChromeHostedWidget, std::move(callback));
  mRootContentController = new ChromeProcessController(
    mChromeHostedWidget, mAPZEventState, mAPZC);
  mCompositorSession->SetContentController(mRootContentController);

  if (StaticPrefs::dom_w3c_touch_events_enabled()) {
    RegisterTouchWindow();
  }
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
  : nsWindow(nullptr)
{
}

nsEventStatus
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent *aEvent)
{
  if (aEvent && aEvent->AsInputEvent()) {
    if (mChromeInputReady && mChromeHostedWidget) {
      aEvent->mWidget = mChromeHostedWidget;
      return mChromeHostedWidget->DispatchEvent(aEvent);
    }

    if (!mChromeHostedWidget && mAPZEventState &&
        InputAPZContext::GetInputBlockId()) {
      InputAPZContext::SetDropped();
      return nsEventStatus_eIgnore;
    }
  }
  return nsIWidget::DispatchEvent(aEvent);
}

}  // namespace embedlite
}  // namespace mozilla

already_AddRefed<nsIWidget>
nsIWidget::CreateTopLevelWindow()
{
  nsCOMPtr<nsIWidget> window =
    mozilla::embedlite::AutoEmbedLiteChromeWindowHost::ConsumePending();
  if (window) {
    return window.forget();
  }

  window = new mozilla::embedlite::nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget>
nsIWidget::CreateChildWindow()
{
  nsCOMPtr<nsIWidget> window = new mozilla::embedlite::nsWindow();
  return window.forget();
}
