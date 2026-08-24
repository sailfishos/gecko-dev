/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "nsPIDOMWindowInlines.h"

#include "nsIWidget.h"

#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteMessageSerialization.h"
#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "mozilla/EventListenerManager.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/layers/InputAPZContext.h"

#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/SameProcessMessageQueue.h"

#include "nsNetUtil.h"
#include "nsIDOMWindowUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/ViewportUtils.h"
#include "nsIDocShell.h"
#include "nsViewportInfo.h"
#include "nsPIWindowRoot.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "mozilla/Preferences.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "APZCCallbackHelper.h"
#include "EmbedFrame.h"

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";
static const char CANCEL_DEFAULT_PAN_ZOOM[] = "cancel-default-pan-zoom";
static const char BROWSER_ZOOM_TO_RECT[] = "browser-zoom-to-rect";
static const char DETECT_SCROLLABLE_SUBFRAME[] = "detect-scrollable-subframe";
static bool sDisableViewportHandler = getenv("NO_VIEWPORT") != 0;

using namespace mozilla;
using namespace mozilla::embedlite;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::dom;
using namespace mozilla::widget;

static const CSSSize kDefaultViewportSize(980, 480);

class AsyncMessageToEmbedLiteParent final
    : public nsSameProcessAsyncMessageBase,
      public SameProcessMessageQueue::Runnable {
 public:
  AsyncMessageToEmbedLiteParent(BrowserChildHelper* aHelper,
                                EmbedFrame* aTarget,
                                ChromeMessageSender* aManager)
      : mHelper(aHelper), mTarget(aTarget), mManager(aManager) {}

  nsresult HandleMessage() override {
    ReceiveMessage(static_cast<EventTarget*>(mTarget.get()), nullptr,
                   mManager);
    return NS_OK;
  }

 private:
  RefPtr<BrowserChildHelper> mHelper;
  RefPtr<EmbedFrame> mTarget;
  RefPtr<ChromeMessageSender> mManager;
};

class AsyncMessageToEmbedLiteChild final
    : public nsSameProcessAsyncMessageBase,
      public SameProcessMessageQueue::Runnable {
 public:
  AsyncMessageToEmbedLiteChild(BrowserChildHelperMessageManager* aTarget,
                               nsFrameMessageManager* aManager)
      : mTarget(aTarget), mManager(aManager) {}

  nsresult HandleMessage() override {
    ReceiveMessage(static_cast<EventTarget*>(mTarget.get()), nullptr,
                   mManager);
    return NS_OK;
  }

 private:
  RefPtr<BrowserChildHelperMessageManager> mTarget;
  RefPtr<nsFrameMessageManager> mManager;
};

namespace mozilla::embedlite {

class BrowserChildHelperChromeMessageManagerCallback final
    : public dom::ipc::MessageManagerCallback {
 public:
  explicit BrowserChildHelperChromeMessageManagerCallback(
      BrowserChildHelper* aHelper)
      : mHelper(aHelper) {}

  void Disconnect() { mHelper = nullptr; }

  bool DoLoadMessageManagerScript(const nsAString& aURL,
                                  bool aRunInGlobalScope) override {
    return mHelper &&
           mHelper->DoLoadMessageManagerScript(aURL, aRunInGlobalScope);
  }

  nsresult DoSendAsyncMessage(
      const nsAString& aMessage,
      NotNull<dom::ipc::StructuredCloneData*> aData) override {
    return mHelper ? mHelper->SendAsyncMessageToChild(aMessage, aData)
                   : NS_ERROR_NOT_AVAILABLE;
  }

  bool WantsMessageReceived() const override { return true; }

  void DoReceiveMessage(
      JSContext* aCx, const nsAString& aMessage, bool aIsSync,
      JS::Handle<JS::Value> aData, bool aHasTransferables,
      nsTArray<NotNull<RefPtr<dom::ipc::StructuredCloneData>>>* aRetVal)
      override {
    if (mHelper) {
      mHelper->ForwardMessageToEmbedLite(
          aCx, aMessage, aIsSync, aData, aHasTransferables, aRetVal);
    }
  }

 private:
  BrowserChildHelper* mHelper;
};

}  // namespace mozilla::embedlite

BrowserChildHelper::BrowserChildHelper(EmbedLiteViewChildIface *aView, uint32_t aId)
  : mView(aView)
  , mWebNavigation(nullptr)
  , mId(aId)
  , mHasValidInnerSize(false)
  , mIPCOpen(false)
  , mShouldSendWebProgressEventsToParent(false)
  , mHasSiblings(false)
  , mDynamicToolbarMaxHeight(0)

{
  LOGT();

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->AddObserver(this,
                                 BEFORE_FIRST_PAINT,
                                 false);
    observerService->AddObserver(this,
                                 CANCEL_DEFAULT_PAN_ZOOM,
                                 false);
    observerService->AddObserver(this,
                                 BROWSER_ZOOM_TO_RECT,
                                 false);
    observerService->AddObserver(this,
                                 DETECT_SCROLLABLE_SUBFRAME,
                                 false);
  }
  if (!InitBrowserChildHelperMessageManager()) {
    NS_WARNING("Failed to register child global ontext");
  }
}

BrowserChildHelper::~BrowserChildHelper()
{
  LOGT();

  if (mChromeMessageManager) {
    mChromeMessageManager->Disconnect();
  }
  if (mChromeMessageManagerCallback) {
    mChromeMessageManagerCallback->Disconnect();
  }

  if (mBrowserChildMessageManager) {
    EventListenerManager* elm = mBrowserChildMessageManager->GetExistingListenerManager();
    if (elm) {
      elm->Disconnect();
    }
    mBrowserChildMessageManager->mBrowserChildHelper = nullptr;
  }
}

void
BrowserChildHelper::Disconnect()
{
  LOGT();
  mIPCOpen = false;
  if (mChromeMessageManager) {
    mChromeMessageManager->Disconnect();
  }
  if (mChromeMessageManagerCallback) {
    mChromeMessageManagerCallback->Disconnect();
  }
  if (mBrowserChildMessageManager) {
    // We should have a message manager if the global is alive, but it
    // seems sometimes we don't.  Assert in aurora/nightly, but don't
    // crash in release builds.
    MOZ_DIAGNOSTIC_ASSERT(mBrowserChildMessageManager->GetMessageManager());
    if (mBrowserChildMessageManager->GetMessageManager()) {
      // The messageManager relays messages via the BrowserChild which
      // no longer exists.
      mBrowserChildMessageManager->DisconnectMessageManager();
    }
  }
}

class EmbedUnloadScriptEvent : public mozilla::Runnable
{
public:
  explicit EmbedUnloadScriptEvent(BrowserChildHelper* aBrowserChild, BrowserChildHelperMessageManager* aBrowserChildMessageManager)
    : mozilla::Runnable("BrowserChildHelper::EmbedUnloadScriptEvent")
    , mBrowserChild(aBrowserChild)
    , mBrowserChildMessageManager(aBrowserChildMessageManager)
  { }

  NS_IMETHOD Run() {
    LOGT();
    RefPtr<Event> event = NS_NewDOMEvent(mBrowserChildMessageManager, nullptr, nullptr);
    if (event) {
      event->InitEvent(nsLiteralString(u"unload"_ns), false, false);
      event->SetTrusted(true);

      mBrowserChildMessageManager->DispatchEvent(*event);
    }

    return NS_OK;
  }

  RefPtr<BrowserChildHelper> mBrowserChild;
  BrowserChildHelperMessageManager* mBrowserChildMessageManager;
};

void
BrowserChildHelper::Unload()
{
  LOGT();
  if (mBrowserChildMessageManager) {
    // Let the frame scripts know the child is being closed
    nsContentUtils::AddScriptRunner(
      new EmbedUnloadScriptEvent(this, mBrowserChildMessageManager)
    );
  }

  mView = nullptr;

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);
  observerService->RemoveObserver(this, CANCEL_DEFAULT_PAN_ZOOM);
  observerService->RemoveObserver(this, BROWSER_ZOOM_TO_RECT);
  observerService->RemoveObserver(this, DETECT_SCROLLABLE_SUBFRAME);
}

already_AddRefed<Document>
BrowserChildHelper::GetTopLevelDocument() const {
  nsCOMPtr<Document> doc;
  WebNavigation()->GetDocument(getter_AddRefs(doc));
  return doc.forget();
}

PresShell*
BrowserChildHelper::GetTopLevelPresShell() const {
  if (RefPtr<Document> doc = GetTopLevelDocument()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

void BrowserChildHelper::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                                       const nsAString& aJSONData) {
  if (!InitBrowserChildHelperMessageManager()) {
    return;
  }

  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> json(cx, JS::NullValue());
  if (!JS_ParseJSON(cx,
                    static_cast<const char16_t*>(aJSONData.BeginReading()),
                    aJSONData.Length(), &json)) {
    JS_ClearPendingException(cx);
    json.setNull();
  }

  auto data = MakeNotNull<RefPtr<dom::ipc::StructuredCloneData>>(
      JS::StructuredCloneScope::DifferentProcess,
      StructuredCloneHolder::TransferringNotSupported);
  ErrorResult rv;
  data->Write(cx, json, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return;
  }

  (void)mChromeMessageManager->DispatchAsyncMessageInternal(cx, aMessageName,
                                                            data);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChildHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowserChildHelper)
  if (tmp->mChromeMessageManager) {
    tmp->mChromeMessageManager->Disconnect();
  }
  if (tmp->mChromeMessageManagerCallback) {
    tmp->mChromeMessageManagerCallback->Disconnect();
  }
  tmp->mChromeMessageManagerCallback = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChildMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEmbedFrame)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowserChildHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChildMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEmbedFrame)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(BrowserChildHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserChildHelper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBrowserChild)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowserChildHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowserChildHelper)

bool
BrowserChildHelper::InitBrowserChildHelperMessageManager()
{
  mShouldSendWebProgressEventsToParent = true;

  if (mBrowserChildMessageManager && mChromeMessageManager && mEmbedFrame) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, false);
  RefPtr<EventTarget> chromeHandler(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, false);

  RefPtr<BrowserChildHelperMessageManager> scope =
      new BrowserChildHelperMessageManager(this);

  MOZ_ALWAYS_TRUE(nsMessageManagerScriptExecutor::Init());

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  if (NS_WARN_IF(!root)) {
    return false;
  }

  RefPtr<ChromeMessageBroadcaster> globalMessageManager =
      nsFrameMessageManager::GetGlobalMessageManager();
  NS_ENSURE_TRUE(globalMessageManager, false);

  auto callback = MakeUnique<BrowserChildHelperChromeMessageManagerCallback>(
      this);
  RefPtr<ChromeMessageSender> chromeMessageManager =
      new ChromeMessageSender(globalMessageManager);

  RefPtr<EmbedFrame> embedFrame = new EmbedFrame();
  embedFrame->mMessageManager = chromeMessageManager;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    embedFrame->mWindow = docShell->GetBrowsingContext();
  }

  mBrowserChildMessageManager = scope;
  mChromeMessageManagerCallback = std::move(callback);
  mChromeMessageManager = chromeMessageManager;
  mEmbedFrame = embedFrame;

  root->SetParentTarget(scope);

  RefPtr<JSActorService> wasvc = JSActorService::GetSingleton();
  wasvc->RegisterChromeEventTarget(scope);

  chromeMessageManager->InitWithCallback(
      mChromeMessageManagerCallback.get());

  return true;
}

bool
BrowserChildHelper::HasValidInnerSize()
{
  return mHasValidInnerSize;
}

NS_IMETHODIMP
BrowserChildHelper::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  if (!strcmp(aTopic, BROWSER_ZOOM_TO_RECT)) {
    nsCOMPtr<Document> doc(GetTopLevelDocument());
    uint32_t presShellId;
    ViewID viewId;
    if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(),
                                                         &presShellId, &viewId)) {
      CSSRect rect;
      sscanf(NS_ConvertUTF16toUTF8(aData).get(),
             "{\"x\":%f,\"y\":%f,\"w\":%f,\"h\":%f}",
             &rect.x, &rect.y, &rect.width, &rect.height);
      mView->ZoomToRect(presShellId, viewId, ZoomTarget{rect});
    }
  } else if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    nsCOMPtr<Document> subject(do_QueryInterface(aSubject));
    nsCOMPtr<Document> doc(GetTopLevelDocument());

    if (subject == doc && doc->IsTopLevelContentDocument()) {
      RefPtr<PresShell> presShell = doc->GetPresShell();
      if (presShell) {
        presShell->SetIsFirstPaint(true);
      }

      APZCCallbackHelper::InitializeRootDisplayport(presShell);

      nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      if (observerService) {
        observerService->NotifyObservers(aSubject, "embedlite-before-first-paint", nullptr);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::HandleEvent(Event *aEvent)
{
  (void)(aEvent);
  return NS_OK;
}

bool
BrowserChildHelper::UpdateFrame(const RepaintRequest &aRequest)
{
  return UpdateFrameHandler(aRequest);
}

void BrowserChildHelper::DynamicToolbarMaxHeightChanged(const ScreenIntCoord &aHeight)
{
  mDynamicToolbarMaxHeight = aHeight;

  RefPtr<Document> document = GetTopLevelDocument();
  if (!document) {
    return;
  }

  if (RefPtr<nsPresContext> presContext = document->GetPresContext()) {
    presContext->SetDynamicToolbarMaxHeight(aHeight);
    presContext->UpdateDynamicToolbarOffset(0);
  }
}

nsIWebNavigation*
BrowserChildHelper::WebNavigation() const
{
  return mWebNavigation.get();
}

nsIWidget*
BrowserChildHelper::WebWidget()
{
  nsCOMPtr<Document> document = GetTopLevelDocument();
  return nsContentUtils::WidgetForDocument(document);
}

bool
BrowserChildHelper::DoLoadMessageManagerScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  if (!InitBrowserChildHelperMessageManager())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
  {
    return false;
  }

  JS::Rooted<JSObject*> messageManager(RootingCx(),
                                       mBrowserChildMessageManager->GetOrCreateWrapper());
  if (!messageManager) {
    // This can happen if we're half-destroyed. It's not a fatal error.
    return true;
  }

  LoadScriptInternal(messageManager, aURL, !aRunInGlobalScope);
  return true;
}

bool
BrowserChildHelper::DoSendBlockingMessage(const nsAString& aMessage,
    NotNull<dom::ipc::StructuredCloneData*> aData,
    nsTArray<NotNull<RefPtr<dom::ipc::StructuredCloneData>>>* aRetVal)
{
  if (!mView) {
    return false;
  }

  NS_ENSURE_TRUE(InitBrowserChildHelperMessageManager(), false);

  SameProcessMessageQueue::Get()->Flush();
  if (mChromeMessageManager) {
    mChromeMessageManager->ReceiveMessage(
        static_cast<EventTarget*>(mEmbedFrame.get()), nullptr, aMessage, true,
        aData, aRetVal);
  }

  return true;
}

nsresult BrowserChildHelper::DoSendAsyncMessage(const nsAString& aMessage,
    NotNull<dom::ipc::StructuredCloneData*> aData)
{
  if (!mView) {
    return NS_ERROR_FAILURE;
  }

  if (!InitBrowserChildHelperMessageManager()) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<AsyncMessageToEmbedLiteParent> event =
      new AsyncMessageToEmbedLiteParent(this, mEmbedFrame,
                                        mChromeMessageManager);
  nsresult rv = event->Init(aMessage, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  SameProcessMessageQueue::Get()->Push(event);
  return NS_OK;
}

nsresult BrowserChildHelper::SendAsyncMessageToChild(
    const nsAString& aMessage,
    NotNull<dom::ipc::StructuredCloneData*> aData) {
  if (!mBrowserChildMessageManager) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<nsFrameMessageManager> manager =
      mBrowserChildMessageManager->GetMessageManager();
  if (!manager) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<AsyncMessageToEmbedLiteChild> event =
      new AsyncMessageToEmbedLiteChild(mBrowserChildMessageManager, manager);
  nsresult rv = event->Init(aMessage, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  SameProcessMessageQueue::Get()->Push(event);
  return NS_OK;
}

void BrowserChildHelper::ForwardMessageToEmbedLite(
    JSContext* aCx, const nsAString& aMessage, bool aIsSync,
    JS::Handle<JS::Value> aData, bool aHasTransferables,
    nsTArray<NotNull<RefPtr<dom::ipc::StructuredCloneData>>>* aRetVal) {
  if (!mView || !mView->HasMessageListener(aMessage)) {
    return;
  }

  if (aHasTransferables) {
    LOGW("Skipping EmbedLite JSON projection for transferable message: %s\n",
         NS_ConvertUTF16toUTF8(aMessage).get());
    return;
  }

  nsAutoString json;
  if (!ConvertMessageToJSON(aCx, aData, aHasTransferables, json)) {
    LOGW("Skipping unsupported EmbedLite JSON message: %s\n",
         NS_ConvertUTF16toUTF8(aMessage).get());
    return;
  }

  if (!aIsSync) {
    if (!mView->DoSendAsyncMessage(nsString(aMessage).get(), json.get())) {
      LOGW("Failed to send EmbedLite JSON message: %s\n",
           NS_ConvertUTF16toUTF8(aMessage).get());
    }
    return;
  }

  nsTArray<nsString> jsonReplies;
  if (!mView->DoSendSyncMessage(nsString(aMessage).get(), json.get(),
                                &jsonReplies) ||
      !aRetVal) {
    return;
  }

  AppendJSONReplies(aCx, jsonReplies, *aRetVal);
}

ScreenIntSize
BrowserChildHelper::GetInnerSize()
{
  return mInnerSize;
}

WidgetTouchEvent BrowserChildHelper::ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput &aData,
                                                                  bool &aRes)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    aRes = false;
    return WidgetTouchEvent();
  }

  aRes = true;
  return aData.ToWidgetEvent(widget);
}

nsIWidget*
BrowserChildHelper::GetWidget(nsPoint* aOffset)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsIFrame* frame = presShell->GetRootFrame();
  if (frame) {
    return frame->GetNearestWidget();
  }

  return nullptr;
}

nsPresContext*
BrowserChildHelper::GetPresContext()
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  return docShell->GetPresContext();
}


mozilla::PresShell*
BrowserChildHelper::GetPresShell()
{
  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, nullptr);
  return presContext->GetPresShell();
}

bool
BrowserChildHelper::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                            const ViewID& aViewId,
                                            const Maybe<mozilla::layers::ZoomConstraints> &aConstraints)
{
  LOGT();
  return mView && mView->UpdateZoomConstraints(aPresShellId,
                                               aViewId,
                                               aConstraints);
}

bool
BrowserChildHelper::UpdateFrameHandler(const RepaintRequest &aRequest) {
  MOZ_ASSERT(aRequest.GetScrollId() != ScrollableLayerGuid::NULL_SCROLL_ID);

  if (aRequest.IsRootContent()) {
    if (RefPtr<PresShell> presShell = GetTopLevelPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aRequest.GetPresShellId() == presShell->GetPresShellId()) {
        ProcessUpdateFrame(aRequest);
        return true;
      }
    }
  } else {
    // aRequest.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    APZCCallbackHelper::UpdateSubFrame(aRequest);
    return true;
  }
  return true;
}

void
BrowserChildHelper::ProcessUpdateFrame(const RepaintRequest &aRequest) {
  if (!mBrowserChildMessageManager) {
    return;
  }

  APZCCallbackHelper::UpdateRootFrame(aRequest);
}

void
BrowserChildHelper::ReportSizeUpdate(const LayoutDeviceIntRect &aRect)
{
  bool initialSizing = !HasValidInnerSize()
                    && (aRect.width != 0 && aRect.height != 0);
  if (initialSizing) {
    mHasValidInnerSize = true;
  }

  LayoutDeviceIntSize size = aRect.Size();
  mInnerSize = ViewAs<ScreenPixel>(
      size, PixelCastJustification::LayoutDeviceIsScreenForBounds);
}

CSSPoint BrowserChildHelper::GetVisualToLayoutTransformedPoint(
    const CSSPoint &aInput, const mozilla::layers::ScrollableLayerGuid::ViewID &aScrollId) {
  nsIContent* content =
    aScrollId == ScrollableLayerGuid::NULL_SCROLL_ID
      ? nullptr : nsLayoutUtils::FindContentFor(aScrollId);
  return mozilla::ViewportUtils::GetVisualToLayoutTransform(content)
    .TransformPoint(aInput);
}

mozilla::CSSPoint
BrowserChildHelper::ApplyPointTransform(const LayoutDevicePoint& aPoint,
                                        const mozilla::layers::ScrollableLayerGuid& aGuid,
                                        uint64_t aInputBlockId,
                                        bool *ok)
{
  RefPtr<PresShell> presShell = GetTopLevelPresShell();
  if (!presShell) {
    if (ok)
      *ok = false;

    LOGT("Failed to transform layout device point -- no PresShell");
    return mozilla::CSSPoint(0.0f, 0.0f);
  }

  if (!presShell->GetPresContext()) {
    if (ok)
      *ok = false;

    LOGT("Failed to transform layout device point -- no nsPresContext");
    return mozilla::CSSPoint(0.0f, 0.0f);
  }

  if (ok)
    *ok = true;

  mozilla::CSSToLayoutDeviceScale scale = presShell->GetPresContext()->CSSToDevPixelScale();

  CSSPoint point = aPoint / scale;

  // Stash the guid in InputAPZContext so that when the visual-to-layout
  // transform is applied to the event's coordinates, we use the right transform
  // based on the scroll frame being targeted.
  // The other values don't really matter.
  InputAPZContext context(aGuid, aInputBlockId, nsEventStatus_eSentinel);
  return GetVisualToLayoutTransformedPoint(point, aGuid.mScrollId);
}

void BrowserChildHelper::SetWebNavigation(nsIWebNavigation *aWebNavigation) {
  mWebNavigation = aWebNavigation;
}

// -- nsIBrowserChild --------------

NS_IMETHODIMP
BrowserChildHelper::GetMessageManager(dom::ContentFrameMessageManager** aResult)
{
  if (mBrowserChildMessageManager) {
    *aResult = mBrowserChildMessageManager;
    NS_ADDREF(*aResult);
    return NS_OK;
  }
  *aResult = nullptr;
  return NS_ERROR_FAILURE;
}

void
BrowserChildHelper::SendRequestFocus(bool aCanFocus, CallerType aCallerType)
{
  LOGNI();
}

NS_IMETHODIMP
BrowserChildHelper::RemoteDropLinks(
    const nsTArray<RefPtr<nsIDroppedLinkItem>>& aLinks)
{
  LOGNI();
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::ContentTransformsReceived(JSContext* aCx,
                                              dom::Promise** aPromise)
{
  LOGNI();
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::GetTabId(uint64_t* aId)
{
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::GetChromeOuterWindowID(uint64_t* aId) {
  nsCOMPtr<nsIDocShell> window = do_GetInterface(WebNavigation());
  if (window) {
    window->GetOuterWindowID(aId);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserChildHelper::NotifyNavigationFinished() {
  LOGT("NOT YET IMPLEMENTED");
  return NS_OK;
}

// -- end of nsIBrowserChild -------

BrowserChildHelperMessageManager::BrowserChildHelperMessageManager(
    BrowserChildHelper* aBrowserChildHelper)
    : dom::ContentFrameMessageManager(new nsFrameMessageManager(aBrowserChildHelper)),
      mBrowserChildHelper(aBrowserChildHelper) {}

BrowserChildHelperMessageManager::~BrowserChildHelperMessageManager() = default;

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChildHelperMessageManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BrowserChildHelperMessageManager,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChildHelper);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BrowserChildHelperMessageManager,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChildHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserChildHelperMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BrowserChildHelperMessageManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BrowserChildHelperMessageManager, DOMEventTargetHelper)

JSObject* BrowserChildHelperMessageManager::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ContentFrameMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

void BrowserChildHelperMessageManager::MarkForCC() {
  if (mBrowserChildHelper) {
    mBrowserChildHelper->MarkScopesForCC();
  }
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }
  MessageManagerGlobal::MarkForCC();
}

dom::Nullable<dom::WindowProxyHolder> BrowserChildHelperMessageManager::GetContent(
    ErrorResult& aError) {
  nsCOMPtr<nsIDocShell> docShell = GetDocShell(aError);
  if (!docShell) {
    return nullptr;
  }
  return dom::WindowProxyHolder(docShell->GetBrowsingContext());
}

already_AddRefed<nsIDocShell> BrowserChildHelperMessageManager::GetDocShell(
    ErrorResult& aError) {
  if (!mBrowserChildHelper) {
    aError.Throw(NS_ERROR_NULL_POINTER);
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> window =
      do_GetInterface(mBrowserChildHelper->WebNavigation());
  return window.forget();
}

already_AddRefed<nsIEventTarget>
BrowserChildHelperMessageManager::GetTabEventTarget() {
  return do_AddRef(GetMainThreadSerialEventTarget());
}

nsresult BrowserChildHelperMessageManager::Dispatch(
    already_AddRefed<nsIRunnable>&& aRunnable) const {
  return SchedulerGroup::Dispatch(std::move(aRunnable));
}
