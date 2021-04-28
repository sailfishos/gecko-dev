/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "nsIWidget.h"

#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteJSON.h"
#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "nsIDOMDocument.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/DocumentInlines.h"

#include "nsNetUtil.h"
#include "nsIDOMWindowUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsViewportInfo.h"
#include "nsPIWindowRoot.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "mozilla/Preferences.h"
#include "nsIFrame.h"
#include "nsView.h"
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

static bool sPostAZPCAsJsonViewport(false);

BrowserChildHelper::BrowserChildHelper(EmbedLiteViewChildIface* aView)
  : mView(aView)
  , mHasValidInnerSize(false)
  , mIPCOpen(false)
  , mShouldSendWebProgressEventsToParent(false)
  , mHasSiblings(false)
{
  LOGT();

//  mScrolling = sDisableViewportHandler == false ? ASYNC_PAN_ZOOM : DEFAULT_SCROLLING;

  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    Preferences::AddBoolVarCache(&sPostAZPCAsJsonViewport, "embedlite.azpc.json.viewport", false);
  }

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
      event->InitEvent(NS_LITERAL_STRING("unload"), false, false);
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

nsIPresShell*
BrowserChildHelper::GetTopLevelPresShell() const {
  if (RefPtr<Document> doc = GetTopLevelDocument()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChildHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowserChildHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChildMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowserChildHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(BrowserChildHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserChildHelper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowserChildHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowserChildHelper)

bool
BrowserChildHelper::InitBrowserChildHelperMessageManager()
{
  if (mBrowserChildMessageManager) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, false);
  RefPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, false);

  RefPtr<BrowserChildHelperMessageManager> scope = mBrowserChildMessageManager =
      new BrowserChildHelperMessageManager(this);

  MOZ_ALWAYS_TRUE(nsMessageManagerScriptExecutor::Init());

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  if (NS_WARN_IF(!root)) {
    mBrowserChildMessageManager = nullptr;
    return false;
  }
  root->SetParentTarget(scope);

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
      mView->ZoomToRect(presShellId, viewId, rect);
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
BrowserChildHelper::HandleEvent(nsIDOMEvent* aEvent)
{
  (void)(aEvent);
  return NS_OK;
}

bool
BrowserChildHelper::UpdateFrame(const FrameMetrics& aFrameMetrics)
{
  return UpdateFrameHandler(aFrameMetrics);
}

nsIWebNavigation*
BrowserChildHelper::WebNavigation() const
{
  return mView->WebNavigation();
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
BrowserChildHelper::DoSendBlockingMessage(JSContext* aCx,
                                          const nsAString& aMessage,
                                          mozilla::dom::ipc::StructuredCloneData& aData,
                                          JS::Handle<JSObject *> aCpows,
                                          nsIPrincipal* aPrincipal,
                                          nsTArray<mozilla::dom::ipc::StructuredCloneData> *aRetVal,
                                          bool aIsSync)
{
  nsCOMPtr<nsIMessageBroadcaster> globalIMessageManager =
          do_GetService("@mozilla.org/globalmessagemanager;1");
  RefPtr<nsFrameMessageManager> globalMessageManager =
          static_cast<nsFrameMessageManager*>(globalIMessageManager.get());

  RefPtr<nsFrameMessageManager> mm =
      mBrowserChildMessageManager->GetMessageManager();

  // We should have a message manager if the global is alive, but it
  // seems sometimes we don't.  Assert in aurora/nightly, but don't
  // crash in release builds.
  MOZ_DIAGNOSTIC_ASSERT(mm);
  if (!mm) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwindow = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(pwindow);
  RefPtr<EmbedFrame> embedFrame = new EmbedFrame();
  embedFrame->mWindow = window;
  embedFrame->mMessageManager = mBrowserChildMessageManager;

  globalMessageManager->ReceiveMessage(embedFrame, nullptr, aMessage, aIsSync, &aData, aRetVal, IgnoreErrors());
  mm->ReceiveMessage(embedFrame, nullptr, aMessage, aIsSync, &aData, aRetVal, IgnoreErrors());

  if (!mView->HasMessageListener(aMessage)) {
    LOGE("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return true;
  }

  NS_ENSURE_TRUE(InitBrowserChildHelperMessageManager(), false);

  // FIXME: Need callback interface for simple JSON to avoid useless conversion here
  JS::RootedValue rval(aCx);
  JS::StructuredCloneScope scope = JS::StructuredCloneScope::SameProcess;

  if (aData.DataLength() > 0 && !JS_ReadStructuredClone(aCx, aData.Data(),
                                                        JS_STRUCTURED_CLONE_VERSION,
                                                        scope,
                                                        &rval,
                                                        JS::CloneDataPolicy(),
                                                        nullptr, nullptr)) {
    JS_ClearPendingException(aCx);
    return false;
  }

  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(aCx, &rval, nullptr, JS::NullHandleValue, EmbedLiteJSON::JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  // FIXME : Return value should be written to nsTArray<StructuredCloneData> *aRetVal
  nsTArray<nsString> jsonRetVal;

  bool retValue = false;

  if (aIsSync) {
    retValue = mView->DoSendSyncMessage(nsString(aMessage).get(), json.get(), &jsonRetVal);
  } else {
    retValue = mView->DoCallRpcMessage(nsString(aMessage).get(), json.get(), &jsonRetVal);
  }

  if (retValue && aRetVal) {
    for (uint32_t i = 0; i < jsonRetVal.Length(); i++) {
      mozilla::dom::ipc::StructuredCloneData* cloneData = aRetVal->AppendElement();

      NS_ConvertUTF16toUTF8 data(jsonRetVal[i]);
      if (!cloneData->CopyExternalData(data.get(), data.Length())) {
        return false;
      }
    }
  }

  return true;
}

nsresult BrowserChildHelper::DoSendAsyncMessage(JSContext* aCx,
                                                const nsAString& aMessage,
                                                mozilla::dom::ipc::StructuredCloneData& aData,
                                                JS::Handle<JSObject *> aCpows,
                                                nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIMessageBroadcaster> globalIMessageManager =
      do_GetService("@mozilla.org/globalmessagemanager;1");
  RefPtr<nsFrameMessageManager> globalMessageManager =
      static_cast<nsFrameMessageManager*>(globalIMessageManager.get());

  RefPtr<nsFrameMessageManager> mm =
      mBrowserChildMessageManager->GetMessageManager();

  // We should have a message manager if the global is alive, but it
  // seems sometimes we don't.  Assert in aurora/nightly, but don't
  // crash in release builds.
  MOZ_DIAGNOSTIC_ASSERT(mm);
  if (!mm) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwindow = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(pwindow);
  RefPtr<EmbedFrame> embedFrame = new EmbedFrame();
  embedFrame->mWindow = window;
  embedFrame->mMessageManager = mBrowserChildMessageManager;

  globalMessageManager->ReceiveMessage(embedFrame, nullptr, aMessage, false, &aData, nullptr, IgnoreErrors());

  mm->ReceiveMessage(embedFrame,
                     nullptr, aMessage, false, &aData, nullptr, IgnoreErrors());

  if (!mView->HasMessageListener(aMessage)) {
    LOGW("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return NS_OK;
  }

  if (!InitBrowserChildHelperMessageManager()) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::RootedValue rval(aCx);
  JS::StructuredCloneScope scope = JS::StructuredCloneScope::SameProcess;

  if (aData.DataLength() > 0 && !JS_ReadStructuredClone(aCx, aData.Data(),
                                                        JS_STRUCTURED_CLONE_VERSION,
                                                        scope,
                                                        &rval,
                                                        JS::CloneDataPolicy(),
                                                        nullptr, nullptr)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString json;
  // Check EmbedLiteJSON::JSONCreator and/or JS_Stringify from Android side
  if (!JS_Stringify(aCx, &rval, nullptr, JS::NullHandleValue, EmbedLiteJSON::JSONCreator, &json))  {
    return NS_ERROR_UNEXPECTED;
  }

  if (json.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mView->DoSendAsyncMessage(nsString(aMessage).get(), json.get())) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

ScreenIntSize
BrowserChildHelper::GetInnerSize()
{
  return mInnerSize;
}

bool
BrowserChildHelper::ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
                                                 WidgetTouchEvent& aEvent)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return false;
  }
  aEvent = aData.ToWidgetTouchEvent(widget);
  return true;
}

nsIWidget*
BrowserChildHelper::GetWidget(nsPoint* aOffset)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsIFrame* frame = presShell->GetRootFrame();
  if (frame) {
    return frame->GetView()->GetWidget();
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

bool
BrowserChildHelper::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                            const ViewID& aViewId,
                                            const Maybe<mozilla::layers::ZoomConstraints> &aConstraints)
{
  LOGT();
  return mView->UpdateZoomConstraints(aPresShellId,
                                      aViewId,
                                      aConstraints);
}

bool
BrowserChildHelper::UpdateFrameHandler(const FrameMetrics& aFrameMetrics) {
  MOZ_ASSERT(aFrameMetrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID);

  if (aFrameMetrics.IsRootContent()) {
    if (nsCOMPtr<nsIPresShell> shell = GetTopLevelPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aFrameMetrics.GetPresShellId() == shell->GetPresShellId()) {
        ProcessUpdateFrame(aFrameMetrics);
        return true;
      }
    }
  } else {
    // aFrameMetrics.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    FrameMetrics newSubFrameMetrics(aFrameMetrics);
    APZCCallbackHelper::UpdateSubFrame(newSubFrameMetrics);
    return true;
  }
  return true;
}

void
BrowserChildHelper::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics) {
  if (!mBrowserChildMessageManager) {
    return;
  }

  FrameMetrics newMetrics = aFrameMetrics;
  APZCCallbackHelper::UpdateRootFrame(newMetrics);
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
  mInnerSize = ViewAs<ScreenPixel>(size, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
}

mozilla::CSSPoint
BrowserChildHelper::ApplyPointTransform(const LayoutDevicePoint& aPoint,
                                        const mozilla::layers::ScrollableLayerGuid& aGuid,
                                        bool *ok)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresContext()->GetPresShell();
  if (!presShell) {
    if (ok)
      *ok = false;

    LOGT("Failed to transform layout device point -- no nsIPresShell");
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
  return APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);
}

uint64_t BrowserChildHelper::ChromeOuterWindowID() const {
  return mView->GetOuterID();
}

// -- nsIBrowserChild --------------

NS_IMETHODIMP
BrowserChildHelper::GetMessageManager(dom::ContentFrameMessageManager** aResult)
{
  if (mBrowserChildMessageManager) {
    NS_ADDREF(*aResult = mBrowserChildMessageManager);
    return NS_OK;
  }
  *aResult = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BrowserChildHelper::GetWebBrowserChrome(nsIWebBrowserChrome3** aWebBrowserChrome)
{
  NS_IF_ADDREF(*aWebBrowserChrome = mWebBrowserChrome);
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::SetWebBrowserChrome(nsIWebBrowserChrome3* aWebBrowserChrome)
{
  mWebBrowserChrome = aWebBrowserChrome;
  return NS_OK;
}

void
BrowserChildHelper::SendRequestFocus(bool aCanFocus, CallerType aCallerType)
{
  LOGNI();
}

NS_IMETHODIMP
BrowserChildHelper::RemoteSizeShellTo(int32_t aWidth, int32_t aHeight,
                                      int32_t aShellItemWidth, int32_t aShellItemHeight)
{
  LOGNI();
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::RemoteDropLinks(
    const nsTArray<RefPtr<nsIDroppedLinkItem>>& aLinks)
{
  LOGNI();
  return NS_OK;
}

NS_IMETHODIMP
BrowserChildHelper::GetTabId(uint64_t* aId)
{
  *aId = mView->GetID();
  return NS_OK;
}

NS_IMETHODIMP BrowserChildHelper::NotifyNavigationFinished() {
  LOGT("NOT YET IMPLEMENTED");
  return NS_OK;
}

NS_IMETHODIMP BrowserChildHelper::BeginSendingWebProgressEventsToParent() {
  mShouldSendWebProgressEventsToParent = true;
  return NS_OK;
}

nsresult BrowserChildHelper::GetHasSiblings(bool* aHasSiblings) {
  *aHasSiblings = mHasSiblings;
  return NS_OK;
}

nsresult BrowserChildHelper::SetHasSiblings(bool aHasSiblings) {
  mHasSiblings = aHasSiblings;
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
  NS_INTERFACE_MAP_ENTRY(dom::ContentFrameMessageManager)
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
  nsCOMPtr<nsIEventTarget> target = EventTargetFor(TaskCategory::Other);
  return target.forget();
}

uint64_t BrowserChildHelperMessageManager::ChromeOuterWindowID() {
  if (!mBrowserChildHelper) {
    return 0;
  }
  return mBrowserChildHelper->ChromeOuterWindowID();
}

nsresult BrowserChildHelperMessageManager::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  return dom::DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget* BrowserChildHelperMessageManager::EventTargetFor(
    TaskCategory aCategory) const {
  return dom::DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread* BrowserChildHelperMessageManager::AbstractMainThreadFor(
    TaskCategory aCategory) {
  return dom::DispatcherTrait::AbstractMainThreadFor(aCategory);
}
