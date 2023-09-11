/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewChild.h"
#include "EmbedLiteAppThreadChild.h"
#include "nsWindow.h"

#include "nsIURIMutator.h"
#include "mozilla/Unused.h"
#include "mozilla/TextControlElement.h"

#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsGlobalWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsWebBrowser.h"
#include "nsRefreshDriver.h"
#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsLayoutUtils.h"
#include "nsILoadContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsISelectionController.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
#include "mozilla/layers/APZEventState.h"
#include "APZCCallbackHelper.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/DoubleTapToZoom.h" // for CalculateRectToZoomTo
#include "mozilla/layers/InputAPZContext.h" // for InputAPZContext
#include "nsIFrame.h"                       // for nsIFrame
#include "FrameLayerBuilder.h"              // for FrameLayerbuilder
#include "nsReadableUtils.h"

#include <sys/syscall.h>

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

// This should be removed and used common StartsWith. But that's not available => simpler to copy for now.
template <int N>
static bool StartsWith(const nsACString& string, const char (&prefix)[N]) {
  if (N - 1 > string.Length()) {
    return false;
  }
  return memcmp(string.Data(), prefix, N - 1) == 0;
}

static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool longTap;
} sHandleDefaultAZPC;
static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool doubleTap;
    bool longTap;
} sPostAZPCAsJson;

static bool sAllowKeyWordURL = false;

static void ReadAZPCPrefs()
{
  // TODO: Switch these to use static prefs
  // See https://firefox-source-docs.mozilla.org/modules/libpref/index.html#static-prefs
  // Example: https://phabricator.services.mozilla.com/D40340

  // Init default azpc notifications behavior
  //Preferences::AddBoolVarCache(&sHandleDefaultAZPC.viewport, "embedlite.azpc.handle.viewport", true);
  //Preferences::AddBoolVarCache(&sHandleDefaultAZPC.singleTap, "embedlite.azpc.handle.singletap", false);
  //Preferences::AddBoolVarCache(&sHandleDefaultAZPC.longTap, "embedlite.azpc.handle.longtap", false);
  //Preferences::AddBoolVarCache(&sHandleDefaultAZPC.scroll, "embedlite.azpc.handle.scroll", true);

  //Preferences::AddBoolVarCache(&sPostAZPCAsJson.viewport, "embedlite.azpc.json.viewport", true);
  //Preferences::AddBoolVarCache(&sPostAZPCAsJson.singleTap, "embedlite.azpc.json.singletap", true);
  //Preferences::AddBoolVarCache(&sPostAZPCAsJson.doubleTap, "embedlite.azpc.json.doubletap", false);
  //Preferences::AddBoolVarCache(&sPostAZPCAsJson.longTap, "embedlite.azpc.json.longtap", true);
  //Preferences::AddBoolVarCache(&sPostAZPCAsJson.scroll, "embedlite.azpc.json.scroll", false);

  //Preferences::AddBoolVarCache(&sAllowKeyWordURL, "keyword.enabled", sAllowKeyWordURL);

  sHandleDefaultAZPC.viewport = true; // "embedlite.azpc.handle.viewport"
  sHandleDefaultAZPC.singleTap = false; // "embedlite.azpc.handle.singletap"
  sHandleDefaultAZPC.longTap = false; // "embedlite.azpc.handle.longtap"
  sHandleDefaultAZPC.scroll = true; // "embedlite.azpc.handle.scroll"

  sPostAZPCAsJson.viewport = true; // "embedlite.azpc.json.viewport"
  sPostAZPCAsJson.singleTap = true; // "embedlite.azpc.json.singletap"
  sPostAZPCAsJson.doubleTap = false; // "embedlite.azpc.json.doubletap"
  sPostAZPCAsJson.longTap = true; // "embedlite.azpc.json.longtap"
  sPostAZPCAsJson.scroll = false; // "embedlite.azpc.json.scroll"

  sAllowKeyWordURL = sAllowKeyWordURL; // "keyword.enabled" (intentionally retained for clarity)
}

EmbedLiteViewChild::EmbedLiteViewChild(const uint32_t &aWindowId,
                                       const uint32_t &aId,
                                       const uint32_t &aParentId,
                                       mozilla::dom::BrowsingContext *parentBrowsingContext,
                                       const bool &isPrivateWindow,
                                       const bool &isDesktopMode)
  : mId(aId)
  , mOuterId(0)
  , mWindow(nullptr)
  , mWebBrowser(nullptr)
  , mChrome(nullptr)
  , mDOMWindow(nullptr)
  , mWebNavigation(nullptr)
  , mWindowObserverRegistered(false)
  , mIsFocused(false)
  , mMargins(0, 0, 0, 0)
  , mIMEComposing(false)
  , mPendingTouchPreventedBlockId(0)
  , mInitialized(false)
  , mDestroyAfterInit(false)
{
  LOGT("id:%u, parentID:%u", aId, aParentId);
  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    ReadAZPCPrefs();
  }

  mWindow = EmbedLiteAppChild::GetInstance()->GetWindowByID(aWindowId);
  MOZ_ASSERT(mWindow != nullptr);

  MessageLoop::current()->PostTask(NewRunnableMethod<const uint32_t, mozilla::dom::BrowsingContext*, const bool, const bool>
                                   ("mozilla::embedlite::EmbedLiteViewChild::InitGeckoWindow",
                                    this,
                                    &EmbedLiteViewChild::InitGeckoWindow,
                                    aParentId,
                                    parentBrowsingContext,
                                    isPrivateWindow,
                                    isDesktopMode));
}

NS_IMETHODIMP EmbedLiteViewChild::QueryInterface(REFNSIID aIID, void **aInstancePtr)
{
  LOGT("Implement me");
  return NS_OK;
}

EmbedLiteViewChild::~EmbedLiteViewChild()
{
  LOGT();
  if (mWindowObserverRegistered) {
    GetPuppetWidget()->RemoveObserver(this);
  }
  mWindow = nullptr;
}

EmbedLitePuppetWidget* EmbedLiteViewChild::GetPuppetWidget() const
{
  return static_cast<EmbedLitePuppetWidget*>(mWidget.get());
}

void
EmbedLiteViewChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvDestroy()
{
  if (!mInitialized) {
    mDestroyAfterInit = true;
    return IPC_OK();
  }

  LOGT("destroy");

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(mDOMWindow, "embedliteviewdestroyed", nullptr);
  }

  EmbedLiteAppService::AppService()->UnregisterView(mId);
  if (mWebBrowser) {
    mWebBrowser->Destroy();
  }
  if (mHelper)
    mHelper->Unload();
  if (mChrome)
    mChrome->RemoveEventHandler();
  if (mWidget) {
    mWidget->Destroy();
  }
  mWebBrowser = nullptr;
  mChrome = nullptr;
  mDOMWindow = nullptr;
  mWebNavigation = nullptr;
  Unused << SendDestroyed();
  PEmbedLiteViewChild::Send__delete__(this);
  return IPC_OK();
}

void
EmbedLiteViewChild::InitGeckoWindow(const uint32_t parentId,
                                    mozilla::dom::BrowsingContext *parentBrowsingContext,
                                    const bool isPrivateWindow,
                                    const bool isDesktopMode)
{
  if (!mWindow) {
    LOGT("Init called for already destroyed object");
    return;
  }

  if (mDestroyAfterInit) {
    Unused << RecvDestroy();
    return;
  }

  LOGT("parentID: %u thread id: %ld", parentId, syscall(SYS_gettid));


  mWidget = new EmbedLitePuppetWidget(this);
  LOGT("puppet widget: %p", GetPuppetWidget());
  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.clipSiblings = true;
  widgetInit.mWindowType = eWindowType_child;

  LayoutDeviceIntRect naturalBounds = mWindow->GetWidget()->GetNaturalBounds();
  nsresult rv = mWidget->Create(mWindow->GetWidget(), 0, naturalBounds, &widgetInit);

  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create widget for EmbedLiteView");
    mWidget = nullptr;
    return;
  }

  mChrome = new WebBrowserChrome(this);

  // nsIBrowserChild (BrowserChildHelper) implementation must be available for nsIWebBrowserChrome (WebBrowserChrome)
  // when nsWebBrowser::Create is called so that nsDocShell::SetTreeOwner can read back nsIBrowserChild.
  // nsWebBrowser::Create -> nsDocShell::SetTreeOwner ->
  // nsDocShellTreeOwner::GetInterface (nsIInterfaceRequestor) -> WebBrowserChrome::GetInterface
  mHelper = new BrowserChildHelper(this, mId);
  mChrome->SetBrowserChildHelper(mHelper.get());

  // If this is created with window.open() or otherwise via WindowCreator
  // we'll receive parent BrowsingContext as an argument.
  // Create a BrowsingContext for our windowless browser.
  RefPtr<BrowsingContext> browsingContext = BrowsingContext::CreateDetached(nullptr, parentBrowsingContext, nullptr, EmptyString(), BrowsingContext::Type::Content);
  browsingContext->SetUsePrivateBrowsing(isPrivateWindow); // Needs to be called before attaching
  browsingContext->EnsureAttached();

  CanonicalBrowsingContext *canonicalBrowsingContext = browsingContext->Canonical();
  nsISecureBrowserUI *secureBrowserUI = nullptr;

  // Create instance of nsSecureBrowserUI.
  if (canonicalBrowsingContext) {
    secureBrowserUI = canonicalBrowsingContext->GetSecureBrowserUI();
    Unused << secureBrowserUI;
  }

  if (browsingContext) {
      LOGT("Created browsing context id: %" PRId64 " opener id: %" PRId64 "", browsingContext->Id(), browsingContext->GetOpenerId());
  }

  // nsWebBrowser::Create creates nsDocShell, calls InitWindow for nsIBaseWindow,
  // and finally creates nsIBaseWindow. When browsingContext is passed to
  // nsWebBrowser::Create, typeContentWrapper type is passed to the nsWebBrowser
  // upon instantiation.
  mWebBrowser = nsWebBrowser::Create(mChrome, mWidget, browsingContext,
                                     nullptr);
  mWebBrowser->SetAllowDNSPrefetch(true);

  uint32_t chromeFlags = 0; // View()->GetWindowFlags();

  if (isPrivateWindow || Preferences::GetBool("browser.privatebrowsing.autostart")) {
    chromeFlags = nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW|nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  mChrome->SetChromeFlags(chromeFlags);

  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  if (NS_FAILED(mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow)))) {
    NS_ERROR("Failed to get the content DOM window!");
  }

  mDOMWindow = do_QueryInterface(domWindow);
  if (!mDOMWindow) {
    NS_ERROR("Got stuck with root DOMWindow!");
  }

  mozilla::dom::AutoNoJSAPI nojsapi;

  mWebNavigation = do_QueryInterface(mWebBrowser);
  if (!mWebNavigation) {
    NS_ERROR("Failed to get the web navigation interface.");
  }

  mHelper->SetWebNavigation(mWebNavigation);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNavigation);
  MOZ_ASSERT(docShell);

  docShell->GetOuterWindowID(&mOuterId);

  EmbedLiteAppService::AppService()->RegisterView(mId);

  mChrome->SetWebBrowser(mWebBrowser);
  rv = mWebBrowser->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed!");
  }

  // This will update also nsIBaseWindow size. nsWebBrowser:Create initializes
  // nsIBaseWindow to empty size (w: 0, h: 0).
  LayoutDeviceIntRect bounds = mWindow->GetWidget()->GetBounds();
  WidgetBoundsChanged(bounds);

  MOZ_ASSERT(mWindow->GetWidget());
  GetPuppetWidget()->AddObserver(this);
  mWindowObserverRegistered = true;

  if (mMargins.LeftRight() > 0 || mMargins.TopBottom() > 0) {
    EmbedLitePuppetWidget *widget = GetPuppetWidget();
    widget->SetMargins(mMargins);
    widget->UpdateBounds(true);
  }

  if (!mWindow->GetWidget()->IsFirstViewCreated()) {
    mWindow->GetWidget()->SetActive(true);
    mWindow->GetWidget()->SetFirstViewCreated();
  }

  nsWeakPtr weakPtrThis = do_GetWeakReference(mWidget);  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](uint64_t aInputBlockId, bool aPreventDefault)
      {
        if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(weakPtrThis)) {
          EmbedLitePuppetWidget *puppetWidget = static_cast<EmbedLitePuppetWidget*>(widget.get());
          puppetWidget->DoSendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mWidget, std::move(callback));
  mSetAllowedTouchBehaviorCallback = [weakPtrThis](uint64_t aInputBlockId,
                                                   const nsTArray<mozilla::layers::TouchBehaviorFlags>& aFlags)
  {
    if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(weakPtrThis)) {
      EmbedLitePuppetWidget *puppetWidget = static_cast<EmbedLitePuppetWidget*>(widget.get());
      puppetWidget->DoSendSetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  };

  SetDesktopMode(isDesktopMode);

  OnGeckoWindowInitialized();
  mHelper->OpenIPC();

  mInitialized = true;

  Unused << SendInitialized();

  nsCOMPtr<nsIObserverService> observerService =
          do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
      observerService->NotifyObservers(mDOMWindow, "embedliteviewcreated", nullptr);
  }
}

nsresult
EmbedLiteViewChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outBrowser = mWebBrowser.get());
  return NS_OK;
}

nsresult
EmbedLiteViewChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  if (!mChrome)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outChrome = mChrome.get());
  return NS_OK;
}


/*----------------------------WidgetIface-----------------------------------------------------*/

bool
EmbedLiteViewChild::SetInputContext(const int32_t& IMEEnabled,
                                    const int32_t& IMEOpen,
                                    const nsString& type,
                                    const nsString& inputmode,
                                    const nsString& actionHint,
                                    const int32_t& cause,
                                    const int32_t& focusChange)
{
  return SendSetInputContext(IMEEnabled,
                             IMEOpen,
                             type,
                             inputmode,
                             actionHint,
                             cause,
                             focusChange);
}

bool
EmbedLiteViewChild::GetInputContext(int32_t* IMEEnabled,
                                    int32_t* IMEOpen)
{
  LOGT();
  return SendGetInputContext(IMEEnabled, IMEOpen);
}

void EmbedLiteViewChild::ResetInputState()
{
  LOGT();
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

/*----------------------------WidgetIface-----------------------------------------------------*/

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewChild::ZoomToRect(const uint32_t& aPresShellId,
                               const ViewID& aViewId,
                               const CSSRect& aRect)
{
  return SendZoomToRect(aPresShellId, aViewId, aRect);
}

bool
EmbedLiteViewChild::SetTargetAPZC(uint64_t aInputBlockId, const nsTArray<ScrollableLayerGuid> &aTargets)
{
  LOGT();
  return SendSetTargetAPZC(aInputBlockId, aTargets);
}

bool
EmbedLiteViewChild::GetDPI(float* aDPI)
{
  return SendGetDPI(aDPI);
}

bool
EmbedLiteViewChild::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                          const ViewID& aViewId,
                                          const Maybe<ZoomConstraints> &aConstraints)
{
  LOGT();
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aConstraints);
}

void
EmbedLiteViewChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  LOGT();
}

bool
EmbedLiteViewChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }
  return false;
}

bool
EmbedLiteViewChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
#endif
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, nsTArray<nsString>* aJSONRetVal)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
#endif
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

nsIWidget*
EmbedLiteViewChild::WebWidget()
{
  return mWidget;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvLoadURL(const nsString &url)
{
  LOGT("url:%s", NS_ConvertUTF16toUTF8(url).get());
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  uint32_t flags = 0;
  if (sAllowKeyWordURL) {
    flags |= nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    flags |= nsIWebNavigation::LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
  }
  flags |= nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;

  LoadURIOptions loadURIOptions;
  loadURIOptions.mTriggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  loadURIOptions.mLoadFlags = flags;
  mWebNavigation->LoadURI(url, loadURIOptions);

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvGoBack(const bool& aRequireUserInteraction, const bool& aUserActivation)
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->GoBack(aRequireUserInteraction, aUserActivation);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvGoForward(const bool& aRequireUserInteraction, const bool& aUserActivation)
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->GoForward(aRequireUserInteraction, aUserActivation);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvStopLoad()
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->Stop(nsIWebNavigation::STOP_NETWORK);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvReload(const bool& aHardReload)
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  uint32_t reloadFlags = aHardReload ?
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE :
                         nsIWebNavigation::LOAD_FLAGS_NONE;

  mWebNavigation->Reload(reloadFlags);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScrollTo(const int &x, const int &y)
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *window = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());
  window->ScrollTo(x, y);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScrollBy(const int &x, const int &y)
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *window = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());
  window->ScrollBy(x, y);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetIsActive(const bool &aIsActive)
{
  NS_ENSURE_TRUE(mWebBrowser && mDOMWindow, IPC_OK());
  if (aIsActive) {
    // Ensure that the PresShell exists, otherwise focusing
    // is definitely not going to work. GetPresShell should
    // create a PresShell if one doesn't exist yet.
    RefPtr<PresShell> presShell = mHelper->GetTopLevelPresShell();
    NS_ASSERTION(presShell, "Need a PresShell to activate!");
    if (presShell) {
      mWebBrowser->FocusActivate();
      LOGT("Activate browser");
    }
  } else {
    mWebBrowser->FocusDeactivate();
    LOGT("Deactivate browser");
  }

  EmbedLitePuppetWidget *widget = GetPuppetWidget();
  if (widget) {
    widget->SetActive(aIsActive);
  }

  // Update state via DocShell -> PresShell
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNavigation);
  if (NS_WARN_IF(!docShell)) {
    return IPC_OK();
  }

  docShell->SetIsActive(aIsActive);

  mWidget->Show(aIsActive);
  mHelper->SetParentIsActive(aIsActive);
  mWebBrowser->SetVisibility(aIsActive);

  if (aIsActive) {
    RecvScheduleUpdate();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetIsFocused(const bool &aIsFocused)
{
  LOGT("child focus: %d thread %ld", aIsFocused, syscall(SYS_gettid));
  NS_ENSURE_TRUE(mWebBrowser && mDOMWindow && (mIsFocused != aIsFocused), IPC_OK());

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, IPC_OK());

  nsIWidgetListener* listener = mWidget->GetWidgetListener();
  if (listener) {
    if (aIsFocused) {
      LOGT("Activate browser focus");
      listener->WindowActivated();
    }
    else {
      listener->WindowDeactivated();
    }
  }
  if (!aIsFocused) {
    fm->ClearFocus(mDOMWindow);
    LOGT("Clear browser focus");
  }

  mIsFocused = aIsFocused;

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetDesktopMode(const bool &aDesktopMode)
{
  LOGT("aDesktopMode:%d", aDesktopMode);

  SetDesktopMode(aDesktopMode);

  return IPC_OK();
}

void EmbedLiteViewChild::SetDesktopMode(const bool aDesktopMode)
{
  LOGT("aDesktopMode:%d", aDesktopMode);

  if (!SetDesktopModeInternal(aDesktopMode)) {
    return;
  }

  nsCOMPtr<Document> document = mHelper->GetTopLevelDocument();

  nsIURI* currentURI = document->GetDocumentURI();

  // Only reload the page for http/https schemes
  bool isValidScheme =
      (NS_SUCCEEDED(currentURI->SchemeIs("http", &isValidScheme)) &&
       isValidScheme) ||
      (NS_SUCCEEDED(currentURI->SchemeIs("https", &isValidScheme)) &&
       isValidScheme);

  if (!isValidScheme) {
    return;
  }

  nsAutoCString host;
  nsresult rv = currentURI->GetHost(host);

  if (StartsWith(host, "www.")) {
    host.Cut(0, 4);
  } else if (StartsWith(host, "mobile.")) {
    host.Cut(0, 7);
  } else if (StartsWith(host, "m.")) {
    host.Cut(0, 2);
  }

  // See https://slides.com/valentingosu/threadsafe-uri-austin-2017
  nsCOMPtr<nsIURI> clonedURI;
  rv = NS_MutateURI(currentURI).SetHost(host).Finalize(clonedURI);
  NS_ENSURE_SUCCESS(rv, );

  nsAutoCString url;
  clonedURI->GetSpec(url);

  LoadURIOptions loadURIOptions;
  loadURIOptions.mTriggeringPrincipal = nsContentUtils::GetSystemPrincipal();

  // We need LOAD_FLAGS_BYPASS_CACHE here since we're changing the User-Agent
  // string, and servers typically don't use the Vary: User-Agent header, so
  // not doing this means that we'd get some of the previously cached content.
  loadURIOptions.mLoadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                              nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY;

  NS_ENSURE_TRUE(mWebNavigation, );

  mWebNavigation->LoadURI(NS_ConvertUTF8toUTF16(url), loadURIOptions);
}

bool EmbedLiteViewChild::SetDesktopModeInternal(const bool aDesktopMode) {
  NS_ENSURE_TRUE(mDOMWindow, false);

  if (mDOMWindow->IsDesktopModeViewport() == aDesktopMode) {
    return false;
  }

  mDOMWindow->SetDesktopModeViewport(aDesktopMode);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(mDOMWindow, "embedliteviewdesktopmodechanged", mDOMWindow->IsDesktopModeViewport() ? u"true" :  u"false");
    return true;
  }
  return false;
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetThrottlePainting(const bool &aThrottle)
{
  LOGT("aThrottle:%d", aThrottle);
  nsPresContext* presContext = mHelper->GetPresContext();
  NS_ENSURE_TRUE(presContext, IPC_OK());
  presContext->RefreshDriver()->SetThrottled(aThrottle);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetDynamicToolbarHeight(const int &aHeight)
{
  mHelper->DynamicToolbarMaxHeightChanged(aHeight);
  Unused << SendDynamicToolbarHeightChanged(aHeight);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetMargins(const int &aTop, const int &aRight,
                                                           const int &aBottom, const int &aLeft)
{
  mMargins = LayoutDeviceIntMargin(aTop, aRight, aBottom, aLeft);
  if (mWidget) {
    EmbedLitePuppetWidget *widget = GetPuppetWidget();
    widget->SetMargins(mMargins);
    widget->UpdateBounds(true);

    RefPtr<Document> document = mHelper->GetTopLevelDocument();

    if (document) {
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      nsCOMPtr<nsIContent> focusedContent = nsFocusManager::GetFocusedDescendant(
            document->GetWindow(), nsFocusManager::eIncludeAllDescendants,
            getter_AddRefs(focusedWindow));
      if (focusedContent) {
        RefPtr<TextControlElement> textControlElement = TextControlElement::FromNodeOrNull(focusedContent);
        if (textControlElement) {
          nsISelectionController *selectionController = textControlElement->GetSelectionController();
          if (selectionController) {
            selectionController->ScrollSelectionIntoView(
                  nsISelectionController::SELECTION_NORMAL,
                  nsISelectionController::SELECTION_WHOLE_SELECTION,
                  nsISelectionController::SCROLL_CENTER_VERTICALLY
                  );
          }
        }
      }
    }
  }

  Unused << SendMarginsChanged(aTop, aRight, aBottom, aLeft);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScheduleUpdate()
{
  // Same that there is in nsPresShell.cpp:10670
  RefPtr<PresShell> ps = mHelper->GetPresShell();
  if (ps && mWidget->IsVisible()) {
    if (nsIFrame* root = ps->GetRootFrame()) {
      FrameLayerBuilder::InvalidateAllLayersForFrame(nsLayoutUtils::GetDisplayRootFrame(root));
      root->SchedulePaint();
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetHttpUserAgent(const nsString& aHttpUserAgent)
{
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (observerService) {
      LOGT("Setting user agent: %s", NS_ConvertUTF16toUTF8(aHttpUserAgent).get());
      observerService->NotifyObservers(mDOMWindow,
                                       "embedliteviewhttpuseragentchanged",
                                       aHttpUserAgent.get());
    }

    return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSuspendTimeouts()
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *pwindow = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());

  if (pwindow && !pwindow->IsFrozen()) {
    pwindow->Freeze();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvResumeTimeouts()
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *pwindow = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());

  if (pwindow && pwindow->IsFrozen()) {
    pwindow->Thaw();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvLoadFrameScript(const nsString &uri)
{
  if (mHelper) {
    mHelper->DoLoadMessageManagerScript(uri, true);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAsyncMessage(const nsString &aMessage,
                                                             const nsString &aData)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
#endif
  EmbedLiteAppService::AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return IPC_OK();
}


mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAsyncMessage(const nsAString &aMessage,
                                                             const nsAString &aData)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
#endif
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAddMessageListener(const nsCString &name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvRemoveMessageListener(const nsCString &name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAddMessageListeners(nsTArray<nsString> &&messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvRemoveMessageListeners(nsTArray<nsString> &&messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleScrollEvent(const gfxRect &contentRect,
                                                                  const gfxSize &scrollSize)
{
  mozilla::CSSRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  mozilla::CSSSize size(scrollSize.width, scrollSize.height);

  if (sPostAZPCAsJson.scroll) {
    nsString data;
    data.AppendPrintf("{ \"contentRect\" : { \"x\" : ");
    data.AppendFloat(contentRect.x);
    data.AppendPrintf(", \"y\" : ");
    data.AppendFloat(contentRect.y);
    data.AppendPrintf(", \"width\" : ");
    data.AppendFloat(contentRect.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(contentRect.height);
    data.AppendPrintf("}, \"scrollSize\" : { \"width\" : ");
    data.AppendFloat(scrollSize.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(scrollSize.height);
    data.AppendPrintf(" }}");
    mHelper->DispatchMessageManagerMessage(u"AZPC:ScrollDOMEvent"_ns, data);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvUpdateFrame(const RepaintRequest &aRequest)
{
  LOGT();
  NS_ENSURE_TRUE(mWebBrowser, IPC_OK());

  if (sHandleDefaultAZPC.viewport) {
    mHelper->UpdateFrame(aRequest);
  }

  return IPC_OK();
}

void
EmbedLiteViewChild::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (aPoint) {
    event.mRefPoint.x = aPoint->x;
    event.mRefPoint.y = aPoint->y;
  } else {
    event.mRefPoint.x = 0;
    event.mRefPoint.y = 0;
  }

  event.mTime = PR_Now() / 1000;
}

// Returns true if the element is interested in double click events
static bool ElementSupportsDoubleClick(Element *element)
{
  nsAutoString attribute;
  element->GetAttribute(u"ondblclick"_ns, attribute);
  if (!attribute.IsEmpty()) {
    // Element has a dblclick attribute
    return true;
  }

  element->GetAttribute(u"jsaction"_ns, attribute);
  if (!attribute.IsEmpty()) {
    nsAutoString::const_iterator start, end;
    attribute.BeginReading(start);
    attribute.EndReading(end);
    if (CaseInsensitiveFindInReadable(u"dblclick"_ns, start, end)) {
      // Element has a jsaction double click handler
      // See JB#56716 and https://github.com/google/jsaction
      return true;
    }
  }

  return false;
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleDoubleTap(const LayoutDevicePoint &aPoint,
                                                                const Modifiers &aModifiers,
                                                                const ScrollableLayerGuid &aGuid,
                                                                const uint64_t &aInputBlockId)
{
  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, aInputBlockId, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  nsIContent* content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
  NS_ENSURE_TRUE(content, IPC_OK());

  PresShell* presShell = APZCCallbackHelper::GetRootContentDocumentPresShellForContent(content);
  NS_ENSURE_TRUE(presShell, IPC_OK());

  RefPtr<Document> document = presShell->GetDocument();
  NS_ENSURE_TRUE(document && !document->Fullscreen(), IPC_OK());

  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);

  // Check whether the element is interested in double clicks
  bool doubleclick = false;
  if (sPostAZPCAsJson.doubleTap) {
    WidgetMouseEvent hittest(true, eMouseHitTest, widget, WidgetMouseEvent::eReal);
    hittest.mRefPoint = LayoutDeviceIntPoint::Truncate(aPoint);
    hittest.mIgnoreRootScrollFrame = false;
    hittest.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
    widget->DispatchInputEvent(&hittest);

    if (EventTarget* target = hittest.GetDOMEventTarget()) {
      if (nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target)) {
        // Check if the element or any parent element has a double click handler
        for (Element* element = targetContent->GetAsElementOrParentElement();
             element && !doubleclick; element = element->GetParentElement()) {
          doubleclick = ElementSupportsDoubleClick(element);
        }
      }
    }
  }

  if (nsLayoutUtils::AllowZoomingForDocument(document) && !doubleclick) {
    // Zoom in to/out from the double tapped element
    CSSToLayoutDeviceScale scale(
        presShell->GetPresContext()->CSSToDevPixelScale());
    CSSPoint point = aPoint / scale;

    CSSRect zoomToRect = CalculateRectToZoomTo(document, point);
    uint32_t presShellId;
    ViewID viewId;
    if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
        document->GetDocumentElement(), &presShellId, &viewId)) {
      ZoomToRect(presShellId, viewId, zoomToRect);
    }
  } else {
    // Pass the double tap on to the element
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(u"Gesture:DoubleTap"_ns, data);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleSingleTap(const LayoutDevicePoint &aPoint,
                                                                const Modifiers &aModifiers,
                                                                const ScrollableLayerGuid &aGuid,
                                                                const uint64_t &aInputBlockId)
{
  if (mIMEComposing) {
    // If we are in the middle of compositing we must finish it, before it is too late.
    // this way we can get focus and actual compositing node working properly in future composition
    nsPoint offset;
    nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
    WidgetCompositionEvent event(true, eCompositionEnd, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
    mIMEComposing = false;
  }

  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, aInputBlockId, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  // IPDL doesn't hold a strong reference to protocols as they're not required
  // to be refcounted. This function can run script, which may trigger a nested
  // event loop, which may release this, so we hold a strong reference here.
  RefPtr<EmbedLiteViewChild> kungFuDeathGrip(this);
  RefPtr<PresShell> presShell = mHelper->GetTopLevelPresShell();
  if (!presShell) {
    return IPC_OK();
  }
  if (!presShell->GetPresContext()) {
    return IPC_OK();
  }

  CSSToLayoutDeviceScale scale = mWidget->GetDefaultScale();

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(u"Gesture:SingleTap"_ns, data);
  }

  if (sHandleDefaultAZPC.singleTap) {
    // ProcessSingleTap multiplies cssPoint by scale.
    if (mHelper->mBrowserChildMessageManager) {
      mAPZEventState->ProcessSingleTap(cssPoint, scale, aModifiers, 1);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleLongTap(const LayoutDevicePoint &aPoint,
                                                              const ScrollableLayerGuid &aGuid,
                                                              const uint64_t &aInputBlockId)
{
  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, aInputBlockId, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  if (sPostAZPCAsJson.longTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(u"Gesture:LongTap"_ns, data);
  }

  bool eventHandled = false;
  if (sHandleDefaultAZPC.longTap) {
    eventHandled = RecvMouseEvent(u"contextmenu"_ns, cssPoint.x, cssPoint.y,
                   2 /* Right button */,
                   1 /* Click count */,
                   0 /* Modifiers */,
                   false /* Ignore root scroll frame */);
  }

  SendContentReceivedInputBlock(aInputBlockId, eventHandled);

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleTextEvent(const nsString &commit,
                                                                const nsString &preEdit,
                                                                const int32_t &replacementStart,
                                                                const int32_t &replacementLength)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  const InputContext& ctx = mWidget->GetInputContext();

#if EMBEDLITE_LOG_SENSITIVE
  LOGF("ctx.mIMEState.mEnabled:%i, com:%s, pre:%s, replStart:%i, replLength:%i\n",
       ctx.mIMEState.mEnabled, NS_ConvertUTF16toUTF8(commit).get(), NS_ConvertUTF16toUTF8(preEdit).get(),
       replacementStart, replacementLength);
#endif
  NS_ENSURE_TRUE(widget && (ctx.mIMEState.mEnabled != IMEEnabled::Disabled), IPC_OK());

  if (replacementLength > 0) {
    nsEventStatus status;
    WidgetQueryContentEvent selection(true, eQuerySelectedText, widget);
    widget->DispatchEvent(&selection, status);

    if (selection.mSucceeded) {
      // Set selection to delete
      WidgetSelectionEvent selectionEvent(true, eSetSelection, widget);
      selectionEvent.mOffset = selection.mReply.mOffset + replacementStart;
      selectionEvent.mLength = replacementLength;
      selectionEvent.mReversed = false;
      selectionEvent.mExpandToClusterBoundary = false;
      widget->DispatchEvent(&selectionEvent, status);

      if (selectionEvent.mSucceeded) {
        // Delete the selection
        WidgetContentCommandEvent deleteCommandEvent(true, eContentCommandDelete, widget);
        widget->DispatchEvent(&deleteCommandEvent, status);
      }
    }
  }

  // probably logic here is over engineered, but clean enough
  bool prevIsComposition = mIMEComposing;
  bool StartComposite = !prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool ChangeComposite = prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool EndComposite = prevIsComposition && preEdit.IsEmpty();
  mIMEComposing = ChangeComposite || StartComposite;
  nsString pushStr = preEdit.IsEmpty() ? commit : preEdit;
  if (!commit.IsEmpty() && !EndComposite) {
    StartComposite = ChangeComposite = EndComposite = true;
  }

  if (StartComposite) {
    WidgetCompositionEvent event(true, eCompositionStart, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  if (StartComposite || ChangeComposite || EndComposite) {

    {
      WidgetCompositionEvent event(true, eCompositionChange, widget);
      InitEvent(event, nullptr);
      event.mData = pushStr;

      if (!EndComposite) {
        // Because we're leaving the composition open, we need to
        // include proper text ranges to make the editor happy.
        TextRange range;
        range.mStartOffset = 0;
        range.mEndOffset = event.mData.Length();
        range.mRangeType = TextRangeType::eRawClause;
        event.mRanges = new TextRangeArray();
        event.mRanges->AppendElement(range);
      }
      APZCCallbackHelper::DispatchWidgetEvent(event);
    }

    RefPtr<PresShell> ps = mHelper->GetPresShell();
    NS_ENSURE_TRUE(ps, IPC_OK());

    nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
    nsIContent *mTarget = DOMFocusManager->GetFocusedElement();

    InternalEditorInputEvent inputEvent(true, eEditorInput, widget);
    inputEvent.mTime = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIMEComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, eCompositionEnd, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  return IPC_OK();
}

static KeyNameIndex getKeyNameIndexByDomKeyCode(int domKeyCode)
{
#define KEY(key_, _codeNameIdx, _keyCode, _modifier)
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode) \
  if (domKeyCode == _keyCode) return KEY_NAME_INDEX_##keyNameIdx_;
#include "KeyCodeConsensus_En_US.h"
  return KEY_NAME_INDEX_USE_STRING;
#undef CONTROL
#undef KEY
}

static CodeNameIndex getCodeNameIndexByCharCode(int charCode)
{
  switch(charCode) {
#define KEY(key_, _codeNameIdx, _keyCode, _modifier) \
  case key_[0]: return CODE_NAME_INDEX_##_codeNameIdx;
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode)
#include "KeyCodeConsensus_En_US.h"
    default: return CODE_NAME_INDEX_UNKNOWN;
#undef CONTROL
#undef KEY
  }
}

static Modifiers getModifiersByCharCode(int charCode)
{
  switch(charCode) {
#define KEY(key_, _codeNameIdx, _keyCode, _modifier) \
  case key_[0]: return _modifier;
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode)
#include "KeyCodeConsensus_En_US.h"
    default: return 0;
#undef CONTROL
#undef KEY
  }
}

nsresult EmbedLiteViewChild::DispatchKeyPressEvent(nsIWidget *widget, const EventMessage &message, const int &domKeyCode, const int &gmodifiers, const int &charCode)
{
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  EventMessage msg = message;
  WidgetKeyboardEvent event(true, msg, widget);
  event.mModifiers = Modifiers(gmodifiers) | getModifiersByCharCode(charCode);
  event.mKeyCode = charCode ? 0 : domKeyCode;
  event.mCharCode = charCode;
  event.mLocation = eKeyLocationStandard;
  event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  event.mTime = PR_IntervalNow();
  event.mKeyNameIndex = getKeyNameIndexByDomKeyCode(domKeyCode);
  event.mKeyValue.Assign(charCode);
  event.mCodeNameIndex = getCodeNameIndexByCharCode(charCode);

  if (domKeyCode == dom::KeyboardEvent_Binding::DOM_VK_RETURN) {
    // Needed for multiline editing
    event.mKeyNameIndex = KEY_NAME_INDEX_Enter;
  }
  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleKeyPressEvent(const int &domKeyCode,
                                                                    const int &gmodifiers,
                                                                    const int &charCode)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  NS_ENSURE_TRUE(widget, IPC_OK());
  // Initial key down event
  NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyDown, domKeyCode, gmodifiers, charCode), IPC_OK());
  if (domKeyCode != dom::KeyboardEvent_Binding::DOM_VK_SHIFT &&
      domKeyCode != dom::KeyboardEvent_Binding::DOM_VK_META &&
      domKeyCode != dom::KeyboardEvent_Binding::DOM_VK_CONTROL &&
      domKeyCode != dom::KeyboardEvent_Binding::DOM_VK_ALT) {
          // Key press event
          NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyPress, domKeyCode, gmodifiers, charCode), IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleKeyReleaseEvent(const int &domKeyCode,
                                                                      const int &gmodifiers,
                                                                      const int &charCode)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  NS_ENSURE_TRUE(widget, IPC_OK());
  // Key up event
  NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyUp, domKeyCode, gmodifiers, charCode), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvMouseEvent(const nsString &aType,
                                                           const float &aX,
                                                           const float &aY,
                                                           const int32_t &aButton,
                                                           const int32_t &aClickCount,
                                                           const int32_t &aModifiers,
                                                           const bool &aIgnoreRootScrollFrame)
{
  NS_ENSURE_TRUE(mWebBrowser, IPC_OK());

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = nsGlobalWindowOuter::Cast(window)->WindowUtils();
  NS_ENSURE_TRUE(utils, IPC_OK());

  bool ignored = false;
  uint8_t argc = 6;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame,
                        0.0, MouseEvent_Binding::MOZ_SOURCE_TOUCH,
                        false, false, 0, 0, argc, &ignored);

  return IPC_OK();
}

bool
EmbedLiteViewChild::ContentReceivedInputBlock(const uint64_t &aInputBlockId, const bool &aPreventDefault)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return DoSendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewChild::DoSendContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return SendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewChild::DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return SendSetAllowedTouchBehavior(aInputBlockId, aFlags);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid,
                                                                    const mozilla::MultiTouchInput& aInput,
                                                                    const uint64_t& aInputBlockId,
                                                                    const nsEventStatus& aApzResponse)
{
  LOGT("thread: %ld", syscall(SYS_gettid));
  MOZ_ASSERT(NS_IsMainThread());

  bool res = false;
  WidgetTouchEvent localEvent = mHelper->ConvertMutiTouchInputToEvent(aInput, res);

  if (!res) {
    return IPC_OK();
  }

  UserActivity();

  // Stash the guid in InputAPZContext so that when the visual-to-layout
  // transform is applied to the event's coordinates, we use the right transform
  // based on the scroll frame being targeted.
  // The other values don't really matter.
  InputAPZContext context(aGuid, aInputBlockId, aApzResponse);

  if (localEvent.mMessage == eTouchStart && mWidget->AsyncPanZoomEnabled()) {
    nsCOMPtr<Document> document = mHelper->GetTopLevelDocument();
    if (StaticPrefs::layout_css_touch_action_enabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
          mWidget, document, localEvent, aInputBlockId,
          mSetAllowedTouchBehaviorCallback);
    }

    APZCCallbackHelper::SendSetTargetAPZCNotification(mWidget, document,
        localEvent, aGuid.mLayersId, aInputBlockId);
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  // We shouldn't have any e10s platforms that have touch events enabled
  // without APZ.
  MOZ_ASSERT(mWidget->AsyncPanZoomEnabled());

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
      aApzResponse, status);
  return IPC_OK();
}



mozilla::ipc::IPCResult EmbedLiteViewChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid,
                                                                        const mozilla::MultiTouchInput& aData,
                                                                        const uint64_t& aInputBlockId,
                                                                        const nsEventStatus& aApzResponse)
{
  LOGT();
  return RecvInputDataTouchEvent(aGuid, aData, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvNotifyAPZStateChange(const ViewID &aViewId, const APZStateChange &aChange, const int &aArg)
{
  LOGT("thread: %ld", syscall(SYS_gettid));
  mAPZEventState->ProcessAPZStateChange(aViewId, aChange, aArg);
  if (aChange == APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvNotifyFlushComplete()
{
  RefPtr<PresShell> ps = mHelper->GetPresShell();
  NS_ENSURE_TRUE(ps, IPC_OK());
  APZCCallbackHelper::NotifyFlushComplete(ps);
  return IPC_OK();
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward, bool aIsSameDocument)
{
  Unused << aIsSameDocument;
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnWindowCloseRequested()
{
  if (SendOnWindowCloseRequested()) {
    // The event listeners can indirectly hold a reference to nsWebBrowser.
    // Since the listener removal process is not synchronous we have to make
    // sure that we start the process before ::RecvDestroy is called. Otherwise
    // NULLing mWebBrowser in the function won't actually remove it and the current
    // implementation relies on that. The nsWebBrowser object needs to be removed in
    // order for all EmbedLitePuppetWidgets we created to also be released. The puppet
    // widget implementation holds a pointer to EmbedLiteViewThreadParent in mEmbed
    // variable and can use it during shutdown process. This means puppet widgets
    // have to be removed before the view.
    if (mChrome)
      mChrome->RemoveEventHandler();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnFirstPaint(int32_t aX, int32_t aY)
{
  if (mDOMWindow) {
    nsCOMPtr<nsIDocShell> docShell = mDOMWindow->GetDocShell();
    if (docShell) {
      RefPtr<PresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        nscolor bgcolor = presShell->GetCanvasBackground();
        Unused << SendSetBackgroundColor(bgcolor);
      }
    }
  }

  return SendOnFirstPaint(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnHttpUserAgentUsed(const char16_t* aHttpUserAgent)
{
  return SendOnHttpUserAgentUsed(nsDependentString(aHttpUserAgent)) ? NS_OK : NS_ERROR_FAILURE;
}

bool
EmbedLiteViewChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::ScrollableLayerGuid::ViewID *aViewIdOut)
{
  nsCOMPtr<Document> doc(mHelper->GetTopLevelDocument());
  return APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

float
EmbedLiteViewChild::GetPresShellResolution() const
{
  nsCOMPtr<Document> document(mHelper->GetTopLevelDocument());
  RefPtr<PresShell> presShell = document->GetPresShell();
  if (!presShell) {
    return 1.0f;
  }
  return presShell->GetResolution();
}

void
EmbedLiteViewChild::WidgetBoundsChanged(const LayoutDeviceIntRect &aSize)
{
  LOGT("sz[%d,%d]", aSize.width, aSize.height);
  MOZ_ASSERT(mHelper && mWebBrowser);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetPositionAndSize(0, 0, aSize.width, aSize.height, true);

  mHelper->ReportSizeUpdate(aSize);
}

void
EmbedLiteViewChild::UserActivity()
{
  LOGT();
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/useridleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetScreenProperties(const int &aDepth, const float &aDensity, const float &aDpi)
{
  mWindow->SetScreenProperties(aDepth, aDensity, aDpi);
  return IPC_OK();
}

} // namespace embedlite
} // namespace mozilla

