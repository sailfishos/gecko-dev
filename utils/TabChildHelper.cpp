/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "TabChildHelper.h"
#include "nsIWidget.h"

#include "TabChild.h"
#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteViewThreadChild.h"
#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "nsIDOMDocument.h"
#include "mozilla/EventListenerManager.h"

#include "nsNetUtil.h"
#include "nsIDOMWindowUtils.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMHTMLBodyElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsViewportInfo.h"
#include "nsPIWindowRoot.h"
#include "StructuredCloneUtils.h"
#include "mozilla/Preferences.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "nsLayoutUtils.h"
#include "nsIDocumentInlines.h"
#include "APZCCallbackHelper.h"

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

TabChildHelper::TabChildHelper(EmbedLiteViewChildIface* aView)
  : mView(aView)
  , mHasValidInnerSize(false)
{
  LOGT();

  mScrolling = sDisableViewportHandler == false ? ASYNC_PAN_ZOOM : DEFAULT_SCROLLING;

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
  if (!InitTabChildGlobal()) {
    NS_WARNING("Failed to register child global ontext");
  }
}

TabChildHelper::~TabChildHelper()
{
  LOGT();
  mGlobal = nullptr;

  if (mTabChildGlobal) {
    EventListenerManager* elm = mTabChildGlobal->GetExistingListenerManager();
    if (elm) {
      elm->Disconnect();
    }
    mTabChildGlobal->mTabChild = nullptr;
  }
}

void
TabChildHelper::Disconnect()
{
  LOGT();
  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
    (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }
}

class EmbedUnloadScriptEvent : public nsRunnable
{
public:
  EmbedUnloadScriptEvent(TabChildHelper* aTabChild, TabChildGlobal* aTabChildGlobal)
    : mTabChild(aTabChild), mTabChildGlobal(aTabChildGlobal)
  { }

  NS_IMETHOD Run() {
    LOGT();
    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMEvent(getter_AddRefs(event), mTabChildGlobal, nullptr, nullptr);
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("unload"), false, false);
      event->SetTrusted(true);

      bool dummy;
      mTabChildGlobal->DispatchEvent(event, &dummy);
    }

    return NS_OK;
  }

  nsRefPtr<TabChildHelper> mTabChild;
  TabChildGlobal* mTabChildGlobal;
};

void
TabChildHelper::Unload()
{
  LOGT();
  if (mTabChildGlobal) {
    // Let the frame scripts know the child is being closed
    nsContentUtils::AddScriptRunner(
      new EmbedUnloadScriptEvent(this, mTabChildGlobal)
    );
  }
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);
  observerService->RemoveObserver(this, CANCEL_DEFAULT_PAN_ZOOM);
  observerService->RemoveObserver(this, BROWSER_ZOOM_TO_RECT);
  observerService->RemoveObserver(this, DETECT_SCROLLABLE_SUBFRAME);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChildHelper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(TabChildBase)

NS_IMPL_ADDREF_INHERITED(TabChildHelper, TabChildBase);
NS_IMPL_RELEASE_INHERITED(TabChildHelper, TabChildBase);

bool
TabChildHelper::InitTabChildGlobal()
{
  if (mTabChildGlobal) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, false);
  nsCOMPtr<nsIDOMEventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, false);

  nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
  NS_ENSURE_TRUE(scope, false);

  mTabChildGlobal = scope;

  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(nsIDOMEventTarget*, scope);

  // Not sure if
  NS_ENSURE_TRUE(InitChildGlobalInternal(scopeSupports, nsCString("intProcessEmbedChildGlobal")), false);

  scope->Init();

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  NS_ENSURE_TRUE(root,  false);
  root->SetParentTarget(scope);

  chromeHandler->AddEventListener(NS_LITERAL_STRING("DOMMetaAdded"), this, false);
  chromeHandler->AddEventListener(NS_LITERAL_STRING("scroll"), this, false);

  return true;
}

bool
TabChildHelper::HasValidInnerSize()
{
  return mHasValidInnerSize;
}

NS_IMETHODIMP
TabChildHelper::Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData)
{
  if (!strcmp(aTopic, BROWSER_ZOOM_TO_RECT)) {
    nsCOMPtr<nsIDocument> doc(GetDocument());
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
    nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
    nsCOMPtr<nsIDocument> doc(GetDocument());

    if (SameCOMIdentity(subject, doc)) {
      mozilla::dom::AutoNoJSAPI nojsapi;

      nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

      mContentDocumentIsDisplayed = true;

      if (!sDisableViewportHandler) {
        // Reset CSS viewport and zoom to default on new page, then
        // calculate them properly using the actual metadata from the
        // page.
        SetCSSViewport(kDefaultViewportSize);

        // In some cases before-first-paint gets called before
        // RecvUpdateDimensions is called and therefore before we have an
        // mInnerSize value set. In such cases defer initializing the viewport
        // until we we get an inner size.
        if (HasValidInnerSize()) {
          InitializeRootMetrics();

          utils->SetResolution(mLastRootMetrics.GetPresShellResolution(),
                               mLastRootMetrics.GetPresShellResolution());
          HandlePossibleViewportChange(mInnerSize);
          // Relay frame metrics to subscribed listeners
          mView->RelayFrameMetrics(mLastRootMetrics);
        }
      }

      nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      utils->SetIsFirstPaint(true);
      if (observerService) {
        observerService->NotifyObservers(aSubject, "embedlite-before-first-paint", nullptr);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChildHelper::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  // Should this also handle "MozScrolledAreaChanged".
  if (eventType.EqualsLiteral("DOMMetaAdded")) {
    // This meta data may or may not have been a meta viewport tag. If it was,
    // we should handle it immediately.
    HandlePossibleViewportChange(mInnerSize);
    // Relay frame metrics to subscribed listeners
    mView->RelayFrameMetrics(mLastRootMetrics);
  }

  return NS_OK;
}

bool
TabChildHelper::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
  if (APZCCallbackHelper::HasValidPresShellId(utils, aFrameMetrics)) {
    mLastRootMetrics = ProcessUpdateFrame(aFrameMetrics);
    return true;
  }
  return true;
}

nsIWebNavigation*
TabChildHelper::WebNavigation() const
{
  return mView->WebNavigation();
}

nsIWidget*
TabChildHelper::WebWidget()
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  return nsContentUtils::WidgetForDocument(document);
}

bool
TabChildHelper::DoLoadMessageManagerScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  if (!InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
  {
    return false;
  }

  LoadScriptInternal(aURL, aRunInGlobalScope);
  return true;
}

bool
TabChildHelper::DoSendBlockingMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      const StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal,
                                      InfallibleTArray<nsString>* aJSONRetVal,
                                      bool aIsSync)
{
  if (!mView->HasMessageListener(aMessage)) {
    LOGE("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return false;
  }

  NS_ENSURE_TRUE(InitTabChildGlobal(), false);
  JSContext* cx = mTabChildGlobal->GetJSContextForEventHandlers();
  JSAutoRequest ar(cx);

  // FIXME: Need callback interface for simple JSON to avoid useless conversion here
  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  if (aData.mDataLength &&
      !ReadStructuredClone(cx, aData, &rval)) {
    JS_ClearPendingException(cx);
    return false;
  }

  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(cx, &rval, JS::NullPtr(), JS::NullHandleValue, EmbedLiteJSON::JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  if (aIsSync) {
    return mView->DoSendSyncMessage(nsString(aMessage).get(), json.get(), aJSONRetVal);
  }

  return mView->DoCallRpcMessage(nsString(aMessage).get(), json.get(), aJSONRetVal);
}

bool
TabChildHelper::DoSendAsyncMessage(JSContext* aCx,
                                   const nsAString& aMessage,
                                   const mozilla::dom::StructuredCloneData& aData,
                                   JS::Handle<JSObject *> aCpows,
                                   nsIPrincipal* aPrincipal)
{
  if (!mView->HasMessageListener(aMessage)) {
    LOGW("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return true;
  }

  NS_ENSURE_TRUE(InitTabChildGlobal(), false);
  JSContext* cx = mTabChildGlobal->GetJSContextForEventHandlers();
  JSAutoRequest ar(cx);

  // FIXME: Need callback interface for simple JSON to avoid useless conversion here
  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  if (aData.mDataLength &&
      !ReadStructuredClone(cx, aData, &rval)) {
    JS_ClearPendingException(cx);
    return false;
  }

  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(cx, &rval, JS::NullPtr(), JS::NullHandleValue, EmbedLiteJSON::JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  return mView->DoSendAsyncMessage(nsString(aMessage).get(), json.get());
}

bool
TabChildHelper::CheckPermission(const nsAString& aPermission)
{
  LOGNI("perm: %s", NS_ConvertUTF16toUTF8(aPermission).get());
  return false;
}

bool
TabChildHelper::ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
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
TabChildHelper::GetWidget(nsPoint* aOffset)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
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
TabChildHelper::GetPresContext()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsRefPtr<nsPresContext> presContext;
  docShell->GetPresContext(getter_AddRefs(presContext));
  return presContext;
}

bool
TabChildHelper::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                        const ViewID& aViewId,
                                        const bool& aIsRoot,
                                        const ZoomConstraints& aConstraints)
{
  return mView->UpdateZoomConstraints(aPresShellId,
                                      aViewId,
                                      aIsRoot,
                                      aConstraints);
}

void
TabChildHelper::ReportSizeUpdate(const gfxSize& aSize)
{
  bool initialSizing = !HasValidInnerSize()
                    && (aSize.width != 0 && aSize.height != 0);
  if (initialSizing) {
    mHasValidInnerSize = true;
  }

  ScreenIntSize oldScreenSize(mInnerSize);
  mInnerSize = ScreenIntSize::FromUnknownSize(gfx::IntSize(aSize.width, aSize.height));

  HandlePossibleViewportChange(oldScreenSize);
}
