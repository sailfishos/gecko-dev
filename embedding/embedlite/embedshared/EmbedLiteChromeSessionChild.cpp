/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteChromeSessionChild.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <utility>

#include "EmbedLiteChromeContentEventOrder.h"
#include "EmbedLiteWindowChild.h"
#include "EmbedLiteAppService.h"
#include "nsWindow.h"

#include "InputData.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/PresShell.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Unused.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsIAppWindow.h"
#include "nsIBaseWindow.h"
#include "nsIChannel.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIOpenWindowInfo.h"
#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsITransportSecurityInfo.h"
#include "nsFrameLoaderOwner.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindowOuter.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace embedlite {

using namespace mozilla::dom;

namespace {

constexpr uint32_t kProgressRetryDelayMs = 16;
constexpr uint8_t kMaxProgressRetryAttempts = 60;
constexpr uint32_t kMaxRestoredTabs = 256;
constexpr uint32_t kMaxRestoredLocationLength = 1024 * 1024;
constexpr uint32_t kMaxRestoredTitleLength = 16 * 1024;

bool ParseUint64String(const nsAString& aText, uint64_t* aResult)
{
  MOZ_ASSERT(aResult);
  if (aText.IsEmpty() || aText[0] == '0') {
    return false;
  }
  uint64_t value = 0;
  for (uint32_t index = 0; index < aText.Length(); ++index) {
    const char16_t character = aText[index];
    if (character < '0' || character > '9') {
      return false;
    }
    const uint64_t digit = character - '0';
    if (value > (UINT64_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  *aResult = value;
  return true;
}

class PermitUnloadResult final
{
public:
  NS_INLINE_DECL_REFCOUNTING(PermitUnloadResult)

  bool done = false;
  bool permit = false;

private:
  ~PermitUnloadResult() = default;
};

class PermitUnloadPromiseHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  explicit PermitUnloadPromiseHandler(std::function<void(bool)>&& aCallback)
    : mCallback(std::move(aCallback))
  {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aError) override
  {
    Unused << aCx;
    Unused << aError;
    Complete(aValue.isBoolean() && aValue.toBoolean());
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aError) override
  {
    Unused << aCx;
    Unused << aValue;
    Unused << aError;
    Complete(false);
  }

private:
  ~PermitUnloadPromiseHandler() = default;

  void Complete(bool aPermit)
  {
    if (mCallback) {
      auto callback = std::move(mCallback);
      callback(aPermit);
    }
  }

  std::function<void(bool)> mCallback;
};

NS_IMPL_ISUPPORTS0(PermitUnloadPromiseHandler)

} // namespace

class EmbedLiteChromeTabProgressListener final : public nsIWebProgressListener,
                                                  public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  EmbedLiteChromeTabProgressListener(
      EmbedLiteChromeSessionChild* aOwner, nsIWebProgress* aSource)
    : mOwner(aOwner)
    , mSource(aSource)
  {
    MOZ_RELEASE_ASSERT(mOwner);
    MOZ_RELEASE_ASSERT(mSource);
  }

  void Detach()
  {
    mOwner = nullptr;
    mSource = nullptr;
  }

private:
  ~EmbedLiteChromeTabProgressListener() = default;

  EmbedLiteChromeSessionChild* mOwner;
  nsCOMPtr<nsIWebProgress> mSource;
};

NS_IMPL_ISUPPORTS(EmbedLiteChromeTabProgressListener,
                  nsIWebProgressListener,
                  nsISupportsWeakReference)

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnStateChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    uint32_t aStateFlags, nsresult aStatus)
{
  return mOwner
    ? mOwner->OnStateChange(
        aWebProgress ? aWebProgress : mSource.get(), aRequest,
        aStateFlags, aStatus) : NS_OK;
}

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnProgressChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
    int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
  return mOwner
    ? mOwner->OnProgressChange(
        aWebProgress ? aWebProgress : mSource.get(), aRequest,
        aCurSelfProgress, aMaxSelfProgress,
        aCurTotalProgress, aMaxTotalProgress) : NS_OK;
}

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnLocationChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsIURI* aLocation,
    uint32_t aFlags)
{
  return mOwner
    ? mOwner->OnLocationChange(
        aWebProgress ? aWebProgress : mSource.get(), aRequest,
        aLocation, aFlags) : NS_OK;
}

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnStatusChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsresult aStatus,
    const char16_t* aMessage)
{
  return mOwner
    ? mOwner->OnStatusChange(
        aWebProgress ? aWebProgress : mSource.get(), aRequest,
        aStatus, aMessage) : NS_OK;
}

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnSecurityChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, uint32_t aState)
{
  return mOwner
    ? mOwner->OnSecurityChange(
        aWebProgress ? aWebProgress : mSource.get(), aRequest, aState) : NS_OK;
}

NS_IMETHODIMP EmbedLiteChromeTabProgressListener::OnContentBlockingEvent(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, uint32_t aEvent)
{
  return mOwner
    ? mOwner->OnContentBlockingEvent(
        aWebProgress ? aWebProgress : mSource.get(), aRequest, aEvent) : NS_OK;
}

class EmbedLiteBrowserDOMWindow final : public nsIBrowserDOMWindow
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBROWSERDOMWINDOW

  explicit EmbedLiteBrowserDOMWindow(EmbedLiteChromeSessionChild* aOwner)
    : mOwner(aOwner)
  {
    MOZ_RELEASE_ASSERT(mOwner);
  }

  void Detach() { mOwner = nullptr; }

private:
  ~EmbedLiteBrowserDOMWindow() = default;

  nsresult OpenInFrame(nsIURI* aURI, nsIOpenURIInFrameParams* aParams,
                       int16_t aWhere, int32_t aFlags,
                       const nsAString& aName, bool aSkipLoad,
                       Element** aResult);

  EmbedLiteChromeSessionChild* mOwner;
};

NS_IMPL_ISUPPORTS(EmbedLiteBrowserDOMWindow, nsIBrowserDOMWindow)

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::CreateContentWindow(
    nsIURI* aURI, nsIOpenWindowInfo* aOpenWindowInfo, int16_t aWhere,
    int32_t aFlags, nsIPrincipal* aTriggeringPrincipal,
    nsIContentSecurityPolicy* aCsp, BrowsingContext** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_AVAILABLE);
  return mOwner->OpenTabFromBrowserDOMWindow(
    aURI, aOpenWindowInfo, nullptr, aTriggeringPrincipal, aCsp,
    EmptyString(), aWhere, aFlags, true, nullptr, aResult);
}

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::OpenURI(
    nsIURI* aURI, nsIOpenWindowInfo* aOpenWindowInfo, int16_t aWhere,
    int32_t aFlags, nsIPrincipal* aTriggeringPrincipal,
    nsIContentSecurityPolicy* aCsp, BrowsingContext** aResult)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aTriggeringPrincipal);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (aFlags & nsIBrowserDOMWindow::OPEN_NO_REFERRER) {
    referrerInfo = new ReferrerInfo(
      nullptr, ReferrerPolicy::_empty, false);
  } else if (aOpenWindowInfo) {
    if (RefPtr<BrowsingContext> parent = aOpenWindowInfo->GetParent()) {
      if (Document* document = parent->GetDocument()) {
        referrerInfo = new ReferrerInfo(*document);
      }
    }
  }
  return mOwner->OpenTabFromBrowserDOMWindow(
    aURI, aOpenWindowInfo, referrerInfo, aTriggeringPrincipal, aCsp,
    EmptyString(), aWhere, aFlags, false, nullptr, aResult);
}

nsresult EmbedLiteBrowserDOMWindow::OpenInFrame(
    nsIURI* aURI, nsIOpenURIInFrameParams* aParams, int16_t aWhere,
    int32_t aFlags, const nsAString& aName, bool aSkipLoad,
    Element** aResult)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIOpenWindowInfo> openWindowInfo;
  nsresult rv = aParams->GetOpenWindowInfo(
    getter_AddRefs(openWindowInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  auto cancelOpenWindowInfo = MakeScopeExit([&]() {
    if (openWindowInfo) {
      Unused << openWindowInfo->Cancel();
    }
  });

  if (aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB &&
      aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB_BACKGROUND &&
      aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB_FOREGROUND &&
      aWhere != nsIBrowserDOMWindow::OPEN_PRINT_BROWSER) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aWhere == nsIBrowserDOMWindow::OPEN_PRINT_BROWSER) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool isPrivate = false;
  rv = aParams->GetIsPrivate(&isPrivate);
  NS_ENSURE_SUCCESS(rv, rv);
  Document* chromeDocument = mOwner->mTabContainer
    ? mOwner->mTabContainer->OwnerDoc() : nullptr;
  if (!chromeDocument ||
      isPrivate != chromeDocument->IsInPrivateBrowsing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  rv = aParams->GetReferrerInfo(getter_AddRefs(referrerInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  rv = aParams->GetTriggeringPrincipal(
    getter_AddRefs(triggeringPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(triggeringPrincipal, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aParams->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFlags & nsIBrowserDOMWindow::OPEN_NO_REFERRER) {
    referrerInfo = new ReferrerInfo(
      nullptr, ReferrerPolicy::_empty, false);
  }
  cancelOpenWindowInfo.release();
  return mOwner->OpenTabFromBrowserDOMWindow(
    aURI, openWindowInfo, referrerInfo, triggeringPrincipal, csp, aName,
    aWhere, aFlags, aSkipLoad, aResult, nullptr);
}

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::CreateContentWindowInFrame(
    nsIURI* aURI, nsIOpenURIInFrameParams* aParams, int16_t aWhere,
    int32_t aFlags, const nsAString& aName, Element** aResult)
{
  return OpenInFrame(
    aURI, aParams, aWhere, aFlags, aName, true, aResult);
}

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::OpenURIInFrame(
    nsIURI* aURI, nsIOpenURIInFrameParams* aParams, int16_t aWhere,
    int32_t aFlags, const nsAString& aName, Element** aResult)
{
  NS_ENSURE_ARG(aURI);
  return OpenInFrame(
    aURI, aParams, aWhere, aFlags, aName, false, aResult);
}

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::CanClose(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  RefPtr<EmbedLiteChromeSessionChild> owner = mOwner;
  *aResult = !owner || owner->CanCloseTabs();
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteBrowserDOMWindow::GetTabCount(uint32_t* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mOwner ? mOwner->mTabs.Length() : 0;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(EmbedLiteChromeSessionChild,
                  nsIObserver,
                  nsIDOMEventListener,
                  nsIWebProgressListener,
                  nsISupportsWeakReference)

EmbedLiteChromeSessionChild::TabRecord::TabRecord(
    uint64_t aId, uint64_t aPersistentId)
  : id(aId)
  , persistentId(aPersistentId)
  , locationRevision(0)
  , endpointId(0)
  , selectedHistoryIndex(0)
  , progressListenerRegistered(false)
  , progressRetryPending(false)
  , progressRetryAttempts(0)
  , contentBridgeReady(false)
  , loading(false)
  , closing(false)
  , discarded(false)
  , restoring(false)
  , awaitingDocumentLocation(false)
  , canGoBack(false)
  , canGoForward(false)
  , progress(0)
  , current(0)
  , total(0)
  , securityState(0)
  , dynamicToolbarHeight(0)
  , hasHttpUserAgent(false)
  , hasDynamicToolbarHeight(false)
  , timeoutsSuspended(false)
  , throttlePainting(false)
  , fullscreen(false)
  , firstPaint(false)
  , firstPaintX(0)
  , firstPaintY(0)
  , scrollWidth(0)
  , scrollHeight(0)
  , scrollX(0)
  , scrollY(0)
  , viewportX(0)
  , viewportY(0)
  , viewportWidth(0)
  , viewportHeight(0)
{
  MOZ_RELEASE_ASSERT(id);
}

EmbedLiteChromeSessionChild::EmbedLiteChromeSessionChild(
    EmbedLiteWindowChild* aWindow)
  : mWindow(aWindow)
  , mAppWindow(nullptr)
  , mNextTabId(1)
  , mSelectedTabId(0)
  , mPendingSelectedTabId(0)
  , mTabRevision(0)
  , mContentRevision(0)
  , mNextBeforeUnloadPromptId(1)
  , mObservingWindowVisible(false)
  , mInitializationRetryPending(false)
  , mInitializationRetryAttempts(0)
  , mInitializationCompletionPending(false)
  , mInitializationCompletionSuccess(false)
  , mTabSnapshotPending(false)
  , mCheckingCanClose(false)
  , mRestoreTabsReceived(false)
  , mInitializationFinished(false)
  , mShuttingDown(false)
  , mReady(false)
  , mActive(true)
  , mFocused(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWindow);
  // Hosted tabs do not emit the legacy embedliteviewcreated notification,
  // so install the compatibility frame script explicitly on every current
  // and later materialized browser. A matching public registration remains
  // idempotent in LoadFrameScript().
  MOZ_ALWAYS_TRUE(mContentRegistrations.AddFrameScript(
    "chrome://embedlite/content/embedhelper.js"_ns));
}

EmbedLiteChromeSessionChild::~EmbedLiteChromeSessionChild()
{
  MOZ_ASSERT(!mWindow);
  MOZ_ASSERT(!mAppWindow);
  MOZ_ASSERT(!mBrowserDOMWindow);
  MOZ_ASSERT(!mTabContainer);
  MOZ_ASSERT(mTabs.IsEmpty());
}

nsresult
EmbedLiteChromeSessionChild::Start(
    nsIAppWindow* aAppWindow, const nsACString& aInitialContentURI)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aAppWindow);
  MOZ_ASSERT(!mAppWindow);

  nsCOMPtr<nsIObserverService> observerService =
    services::GetObserverService();
  NS_ENSURE_TRUE(observerService, NS_ERROR_NOT_AVAILABLE);

  nsresult rv = observerService->AddObserver(
    this, "xul-window-visible", true);
  NS_ENSURE_SUCCESS(rv, rv);

  mAppWindow = aAppWindow;
  mInitialContentURI = aInitialContentURI;
  mObservingWindowVisible = true;
  nsCOMPtr<mozIDOMWindowProxy> chromeDOMWindow = do_GetInterface(mAppWindow);
  if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
    service->RegisterChromeWindow(
      mWindow->GetUniqueID(), chromeDOMWindow, this);
  }
  return NS_OK;
}

void
EmbedLiteChromeSessionChild::RemoveObserver()
{
  if (!mObservingWindowVisible) {
    return;
  }

  if (nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService()) {
    Unused << observerService->RemoveObserver(this, "xul-window-visible");
  }
  mObservingWindowVisible = false;
}

void EmbedLiteChromeSessionChild::AddBrowserEventListeners(TabRecord& aTab)
{
  MOZ_ASSERT(aTab.browser);

  Unused << aTab.browser->AddSystemEventListener(
    u"DOMTitleChanged"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"pagetitlechanged"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"DidChangeBrowserRemoteness"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"XULFrameLoaderCreated"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"DOMWindowClose"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"EmbedLiteChromeContentState"_ns, this, false);
  Unused << aTab.browser->AddSystemEventListener(
    u"EmbedLiteChromeContentMessage"_ns, this, false);
}

void EmbedLiteChromeSessionChild::RemoveBrowserEventListeners(TabRecord& aTab)
{
  if (!aTab.browser) {
    return;
  }

  aTab.browser->RemoveSystemEventListener(
    u"DOMTitleChanged"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"pagetitlechanged"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"DidChangeBrowserRemoteness"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"XULFrameLoaderCreated"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"DOMWindowClose"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"EmbedLiteChromeContentState"_ns, this, false);
  aTab.browser->RemoveSystemEventListener(
    u"EmbedLiteChromeContentMessage"_ns, this, false);
}

bool EmbedLiteChromeSessionChild::DispatchContentCommand(
    TabRecord& aTab, const nsAString& aCommand, const nsAString& aData)
{
  if (!aTab.browser || aTab.discarded || aTab.restoring || mShuttingDown) {
    return false;
  }
  ErrorResult error;
  aTab.browser->SetAttribute(
    u"data-embedlite-command"_ns, aCommand, error);
  if (error.Failed()) {
    error.SuppressException();
    return false;
  }
  aTab.browser->SetAttribute(
    u"data-embedlite-command-data"_ns, aData, error);
  if (error.Failed()) {
    error.SuppressException();
    return false;
  }
  RefPtr<Event> event = NS_NewDOMEvent(aTab.browser, nullptr, nullptr);
  if (!event) {
    return false;
  }
  event->InitEvent(u"EmbedLiteChromeContentCommand"_ns, true, false);
  const bool dispatched = aTab.browser->DispatchEvent(
    *event, CallerType::NonSystem, error);
  const bool dispatchFailed = error.Failed();
  error.SuppressException();
  aTab.browser->RemoveAttribute(u"data-embedlite-command"_ns, error);
  error.SuppressException();
  aTab.browser->RemoveAttribute(u"data-embedlite-command-data"_ns, error);
  error.SuppressException();
  aTab.browser->RemoveAttribute(u"data-embedlite-command-name"_ns, error);
  error.SuppressException();
  return !dispatchFailed && dispatched;
}

void EmbedLiteChromeSessionChild::ReplayContentRegistrations(TabRecord& aTab)
{
  if (aTab.endpointId) {
    nsAutoString endpointData;
    endpointData.AppendLiteral(u"{\"endpointId\":");
    endpointData.AppendInt(aTab.endpointId);
    endpointData.Append('}');
    DispatchContentCommand(aTab, u"set-endpoint"_ns, endpointData);
  }
  for (const nsCString& script : mContentRegistrations.FrameScripts()) {
    DispatchContentCommand(aTab, u"load-script"_ns,
                           NS_ConvertUTF8toUTF16(script));
  }
  for (const nsCString& name : mContentRegistrations.MessageListeners()) {
    ErrorResult error;
    aTab.browser->SetAttribute(u"data-embedlite-command-name"_ns,
                               NS_ConvertUTF8toUTF16(name), error);
    if (!error.Failed()) {
      DispatchContentCommand(aTab, u"add-listener"_ns);
    } else {
      error.SuppressException();
    }
  }
  if (aTab.hasHttpUserAgent) {
    ErrorResult error;
    aTab.browser->SetAttribute(u"data-embedlite-command-name"_ns,
                               aTab.httpUserAgent, error);
    if (!error.Failed()) {
      DispatchContentCommand(aTab, u"set-user-agent"_ns, u"{}"_ns);
    } else {
      error.SuppressException();
    }
  }
  if (aTab.hasDynamicToolbarHeight) {
    nsAutoCString json;
    json.AppendPrintf("{\"height\":%d}", aTab.dynamicToolbarHeight);
    DispatchContentCommand(aTab, u"set-toolbar-height"_ns,
                           NS_ConvertUTF8toUTF16(json));
  }
  if (aTab.timeoutsSuspended) {
    DispatchContentCommand(aTab, u"suspend-timeouts"_ns, u"{}"_ns);
  }
}

void EmbedLiteChromeSessionChild::BeginDocumentNavigation(TabRecord& aTab)
{
  if (!++aTab.locationRevision) {
    ++aTab.locationRevision;
  }
  aTab.securityStatus.Truncate();
  aTab.securityState = 0;
  aTab.fullscreen = false;
  aTab.firstPaint = false;
  aTab.firstPaintX = 0;
  aTab.firstPaintY = 0;
  aTab.scrollWidth = 0;
  aTab.scrollHeight = 0;
  aTab.scrollX = 0;
  aTab.scrollY = 0;
  aTab.viewportX = 0;
  aTab.viewportY = 0;
  aTab.viewportWidth = 0;
  aTab.viewportHeight = 0;
  ScheduleTabSnapshot();
  SendContentState(aTab);
}

void EmbedLiteChromeSessionChild::SendContentState(TabRecord& aTab)
{
  if (!mWindow || !mReady || aTab.id != mSelectedTabId ||
      aTab.discarded || aTab.restoring ||
      aTab.securityStatus.Length() > kMaxContentDataLength) {
    return;
  }
  // Navigation and selection updates must reach the embedder before content
  // state carrying their locationRevision. The already-posted snapshot task
  // may send a harmless later duplicate revision.
  if (mTabSnapshotPending) {
    SendTabSnapshot();
  }
  if (!++mContentRevision) {
    ++mContentRevision;
  }
  EmbedLiteChromeContentStateData state;
  state.tabId() = aTab.id;
  state.persistentId() = aTab.persistentId;
  state.revision() = mContentRevision;
  state.locationRevision() = aTab.locationRevision;
  state.securityStatus() = aTab.securityStatus;
  state.securityState() = aTab.securityState;
  state.fullscreen() = aTab.fullscreen;
  state.firstPaint() = aTab.firstPaint;
  state.firstPaintX() = aTab.firstPaintX;
  state.firstPaintY() = aTab.firstPaintY;
  state.scrollWidth() = aTab.scrollWidth;
  state.scrollHeight() = aTab.scrollHeight;
  state.scrollX() = aTab.scrollX;
  state.scrollY() = aTab.scrollY;
  state.viewportX() = aTab.viewportX;
  state.viewportY() = aTab.viewportY;
  state.viewportWidth() = aTab.viewportWidth;
  state.viewportHeight() = aTab.viewportHeight;
  Unused << mWindow->SendOnContentStateChanged(state);
}

void EmbedLiteChromeSessionChild::RemoveProgressListener(TabRecord& aTab)
{
  if (aTab.webProgress && aTab.progressListenerRegistered) {
    Unused << aTab.webProgress->RemoveProgressListener(
      aTab.progressListener);
  }
  if (aTab.progressListener) {
    aTab.progressListener->Detach();
  }
  aTab.progressListenerRegistered = false;
  aTab.progressListener = nullptr;
  aTab.webProgress = nullptr;
}

void
EmbedLiteChromeSessionChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  if (mShuttingDown) {
    return;
  }
  mShuttingDown = true;
  RemoveObserver();
  CancelBeforeUnloadPrompts();

  mFocused = false;
  ApplyFocusState();

  nsCOMPtr<mozIDOMWindowProxy> chromeDOMWindow;
  if (mAppWindow) {
    chromeDOMWindow = do_GetInterface(mAppWindow);
  }
  if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
    service->UnregisterChromeWindow(
      mWindow->GetUniqueID(), chromeDOMWindow, this);
  }
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(chromeDOMWindow);
  if (outer && mBrowserDOMWindow &&
      nsGlobalWindowOuter::Cast(outer)->GetBrowserDOMWindow() ==
        mBrowserDOMWindow) {
    nsGlobalWindowOuter::Cast(outer)->SetBrowserDOMWindowOuter(nullptr);
  }
  if (mBrowserDOMWindow) {
    static_cast<EmbedLiteBrowserDOMWindow*>(mBrowserDOMWindow.get())
      ->Detach();
    mBrowserDOMWindow = nullptr;
  }

  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (tab->endpointId) {
      if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
        service->UnregisterChromeTab(tab->endpointId);
      }
      tab->endpointId = 0;
    }
    RemoveProgressListener(*tab);
    RemoveBrowserEventListeners(*tab);
    if (tab->browser) {
      tab->browser->Remove();
    }
  }
  mTabs.Clear();
  mSelectedTabId = 0;
  mPendingSelectedTabId = 0;
  mTabContainer = nullptr;
  mInitialContentURI.Truncate();
  mReady = false;
  mAppWindow = nullptr;
  mWindow = nullptr;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const char16_t* aData)
{
  Unused << aData;
  if (!mAppWindow || std::strcmp(aTopic, "xul-window-visible")) {
    return NS_OK;
  }

  nsCOMPtr<nsIAppWindow> appWindow = do_QueryInterface(aSubject);
  if (appWindow != mAppWindow) {
    return NS_OK;
  }

  RefPtr<EmbedLiteChromeSessionChild> kungFuDeathGrip(this);
  RemoveObserver();
  nsresult rv = BrowserBecameVisible();
  if (NS_SUCCEEDED(rv)) {
    ScheduleInitializationCompletion(true);
  } else if (rv != NS_ERROR_NOT_AVAILABLE) {
    ScheduleInitializationCompletion(false);
  } else {
    ScheduleInitializationRetry();
  }
  return NS_OK;
}

void
EmbedLiteChromeSessionChild::ScheduleInitializationRetry()
{
  if (!mWindow || mInitializationFinished || mReady ||
      mInitializationRetryPending) {
    return;
  }

  if (mInitializationRetryAttempts >= kMaxProgressRetryAttempts) {
    ScheduleInitializationCompletion(false);
    return;
  }

  mInitializationRetryPending = true;
  ++mInitializationRetryAttempts;
  nsresult rv = NS_DelayedDispatchToCurrentThread(
    NS_NewRunnableFunction(
      "EmbedLiteChromeSessionChild::RetryInitialization",
      [self = RefPtr<EmbedLiteChromeSessionChild>(this)]() {
        self->mInitializationRetryPending = false;
        if (!self->mWindow || self->mInitializationFinished || self->mReady) {
          return;
        }

        nsresult retryRv = NS_OK;
        if (TabRecord* selected = self->SelectedTab()) {
          if (selected->discarded) {
            retryRv = self->MaterializeTab(*selected);
          }
        } else if (!self->mRestoreTabsReceived &&
                   !self->mInitialContentURI.IsEmpty()) {
          TabRecord* initialTab = nullptr;
          retryRv = self->CreateTab(
            nullptr, EmptyString(), false, true, 0, &initialTab);
        }
        if (NS_SUCCEEDED(retryRv)) {
          retryRv = self->TryCompleteInitialization();
        }
        if (NS_SUCCEEDED(retryRv)) {
          self->ScheduleInitializationCompletion(true);
        } else if (retryRv == NS_ERROR_NOT_AVAILABLE) {
          self->ScheduleInitializationRetry();
        } else {
          self->ScheduleInitializationCompletion(false);
        }
      }),
    kProgressRetryDelayMs);
  if (NS_FAILED(rv)) {
    mInitializationRetryPending = false;
    ScheduleInitializationCompletion(false);
  }
}

void
EmbedLiteChromeSessionChild::ScheduleInitializationCompletion(bool aSuccess)
{
  if (!mWindow || mInitializationFinished) {
    return;
  }

  if (mInitializationCompletionPending) {
    mInitializationCompletionSuccess |= aSuccess;
    return;
  }

  mInitializationCompletionPending = true;
  mInitializationCompletionSuccess = aSuccess;
  nsresult rv = NS_DispatchToCurrentThread(NS_NewRunnableFunction(
    "EmbedLiteChromeSessionChild::CompleteInitialization",
    [self = RefPtr<EmbedLiteChromeSessionChild>(this)]() {
      self->CompleteInitialization(
        self->mInitializationCompletionSuccess || self->mReady);
    }));
  if (NS_FAILED(rv)) {
    mInitializationCompletionPending = false;
  }
}

nsresult
EmbedLiteChromeSessionChild::BrowserBecameVisible()
{
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mAppWindow);

  nsCOMPtr<mozIDOMWindowProxy> chromeDOMWindow = do_GetInterface(mAppWindow);
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(chromeDOMWindow);
  RefPtr<Document> document = outer ? outer->GetExtantDoc() : nullptr;
  NS_ENSURE_TRUE(document, NS_ERROR_UNEXPECTED);

  nsresult rv = InstallBrowserDOMWindow();
  NS_ENSURE_SUCCESS(rv, rv);

  mTabContainer = document->GetElementById(u"browser-stack"_ns);
  NS_ENSURE_TRUE(mTabContainer, NS_ERROR_UNEXPECTED);
  nsWindow* rootWidget = mWindow->GetWidget();
  NS_ENSURE_TRUE(rootWidget, NS_ERROR_UNEXPECTED);
  rootWidget->InitializeChromeInput();

  if (TabRecord* selected = SelectedTab()) {
    if (selected->discarded) {
      rv = MaterializeTab(*selected);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else if (!mInitialContentURI.IsEmpty()) {
    TabRecord* initialTab = nullptr;
    rv = CreateTab(
      nullptr, EmptyString(), false, true, 0, &initialTab);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_RELEASE_ASSERT(initialTab);
  }

  return TryCompleteInitialization();
}

nsresult EmbedLiteChromeSessionChild::InstallBrowserDOMWindow()
{
  MOZ_ASSERT(mAppWindow);
  MOZ_ASSERT(!mBrowserDOMWindow);

  nsCOMPtr<mozIDOMWindowProxy> chromeDOMWindow = do_GetInterface(mAppWindow);
  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(chromeDOMWindow);
  NS_ENSURE_TRUE(outer, NS_ERROR_UNEXPECTED);

  mBrowserDOMWindow = new EmbedLiteBrowserDOMWindow(this);
  nsGlobalWindowOuter::Cast(outer)->SetBrowserDOMWindowOuter(
    mBrowserDOMWindow);
  return NS_OK;
}

nsresult
EmbedLiteChromeSessionChild::TryCompleteInitialization()
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  if (mReady) {
    return NS_OK;
  }

  TabRecord* selected = SelectedTab();
  if (!selected) {
    NS_ENSURE_TRUE(mRestoreTabsReceived || mInitialContentURI.IsEmpty(),
                   NS_ERROR_NOT_AVAILABLE);
  } else {
    const uint64_t selectedTabId = selected->id;
    if (selected->discarded) {
      nsresult rv = MaterializeTab(*selected);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    selected = FindTab(selectedTabId);
    NS_ENSURE_TRUE(selected, NS_ERROR_ABORT);
    nsresult rv = RebindProgressListener(*selected);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mReady = true;
  ApplyActiveState();
  ApplyFocusState();
  return NS_OK;
}

void
EmbedLiteChromeSessionChild::CompleteInitialization(bool aSuccess)
{
  mInitializationCompletionPending = false;
  if (!mWindow || mInitializationFinished) {
    return;
  }
  mInitializationFinished = true;

  if (!aSuccess || !mReady) {
    mWindow->ChromeSessionInitializationFinished(false);
    return;
  }

  nsCString initialContentURI(mInitialContentURI);
  mInitialContentURI.Truncate();
  mWindow->ChromeSessionInitializationFinished(true);
  ScheduleTabSnapshot();
  if (mWindow && mReady && !mRestoreTabsReceived &&
      !initialContentURI.IsEmpty()) {
    Unused << LoadURL(initialContentURI, false);
  }
}

EmbedLiteChromeSessionChild::TabRecord*
EmbedLiteChromeSessionChild::FindTab(uint64_t aTabId) const
{
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (tab->id == aTabId) {
      return tab.get();
    }
  }
  return nullptr;
}

EmbedLiteChromeSessionChild::TabRecord*
EmbedLiteChromeSessionChild::FindTab(Element* aBrowser) const
{
  if (!aBrowser) {
    return nullptr;
  }
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (tab->browser == aBrowser) {
      return tab.get();
    }
  }
  return nullptr;
}

EmbedLiteChromeSessionChild::TabRecord*
EmbedLiteChromeSessionChild::FindTab(nsIWebProgress* aWebProgress) const
{
  if (!aWebProgress) {
    return nullptr;
  }
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (tab->progressListenerRegistered &&
        tab->webProgress == aWebProgress) {
      return tab.get();
    }
  }

  RefPtr<BrowsingContext> progressContext;
  if (NS_FAILED(aWebProgress->GetBrowsingContextXPCOM(
        getter_AddRefs(progressContext))) || !progressContext) {
    return nullptr;
  }
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    BrowsingContext* context = BrowsingContextFor(*tab);
    if (context &&
        context->Canonical() == progressContext->Canonical()) {
      return tab.get();
    }
  }
  return nullptr;
}

EmbedLiteChromeSessionChild::TabRecord*
EmbedLiteChromeSessionChild::FindTab(BrowsingContext* aBrowsingContext) const
{
  if (!aBrowsingContext) {
    return nullptr;
  }
  const uint64_t browserId = aBrowsingContext->Top()->BrowserId();
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    BrowsingContext* context = BrowsingContextFor(*tab);
    if (context && context->Top()->BrowserId() == browserId) {
      return tab.get();
    }
  }
  return nullptr;
}

EmbedLiteChromeSessionChild::TabRecord*
EmbedLiteChromeSessionChild::SelectedTab() const
{
  return FindTab(mSelectedTabId);
}

BrowsingContext* EmbedLiteChromeSessionChild::BrowsingContextFor(
    const TabRecord& aTab) const
{
  if (!aTab.browser) {
    return nullptr;
  }
  RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
    do_QueryObject(aTab.browser);
  return frameLoaderOwner ? frameLoaderOwner->GetBrowsingContext() : nullptr;
}

CanonicalBrowsingContext*
EmbedLiteChromeSessionChild::CurrentBrowsingContext() const
{
  if (TabRecord* selected = SelectedTab()) {
    if (BrowsingContext* browsingContext = BrowsingContextFor(*selected)) {
      return browsingContext->Canonical();
    }
  }
  return nullptr;
}

BrowsingContext* EmbedLiteChromeSessionChild::BrowsingContextForTab(
    uint64_t aTabId) const
{
  TabRecord* tab = FindTab(aTabId);
  return tab && !tab->discarded && !tab->restoring && !tab->closing
    ? BrowsingContextFor(*tab) : nullptr;
}

Element* EmbedLiteChromeSessionChild::BrowserForTab(uint64_t aTabId) const
{
  TabRecord* tab = FindTab(aTabId);
  return tab ? tab->browser.get() : nullptr;
}

nsresult EmbedLiteChromeSessionChild::RebindProgressListener(TabRecord& aTab)
{
  BrowsingContext* context = BrowsingContextFor(aTab);
  CanonicalBrowsingContext* browsingContext =
    context ? context->Canonical() : nullptr;
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIWebProgress> webProgress = browsingContext->GetWebProgress();
  NS_ENSURE_TRUE(webProgress, NS_ERROR_NOT_AVAILABLE);
  if (webProgress == aTab.webProgress &&
      aTab.progressListenerRegistered) {
    return NS_OK;
  }

  RemoveProgressListener(aTab);
  aTab.webProgress = webProgress;

  const uint32_t notifyMask =
    nsIWebProgress::NOTIFY_STATE_NETWORK |
    nsIWebProgress::NOTIFY_LOCATION |
    nsIWebProgress::NOTIFY_PROGRESS |
    nsIWebProgress::NOTIFY_SECURITY;
  RefPtr<EmbedLiteChromeTabProgressListener> listener =
    new EmbedLiteChromeTabProgressListener(this, aTab.webProgress);
  nsresult rv = aTab.webProgress->AddProgressListener(listener, notifyMask);
  if (NS_SUCCEEDED(rv)) {
    aTab.progressListener = listener;
    aTab.progressListenerRegistered = true;
    aTab.progressRetryAttempts = 0;
    if (BrowserParent* browserParent = browsingContext->GetBrowserParent()) {
      browserParent->SetBrowserDOMWindow(mBrowserDOMWindow);
    }
  }
  return rv;
}

void EmbedLiteChromeSessionChild::FinishProgressListenerRebind(
    TabRecord& aTab)
{
  if (aTab.discarded) {
    return;
  }
  ApplyTabActiveState(aTab, aTab.id == mSelectedTabId);
  UpdateLocation(aTab);
  UpdateTitle(aTab);
  if (aTab.id == mSelectedTabId) {
    ApplyFocusState();
    ScheduleUpdate();
  }
}

void EmbedLiteChromeSessionChild::ScheduleProgressListenerRetry(
    uint64_t aTabId)
{
  TabRecord* tab = FindTab(aTabId);
  if (!mWindow || !mReady || !tab || tab->progressRetryPending ||
      tab->progressRetryAttempts >= kMaxProgressRetryAttempts) {
    return;
  }

  tab->progressRetryPending = true;
  ++tab->progressRetryAttempts;
  nsresult rv = NS_DelayedDispatchToCurrentThread(
    NS_NewRunnableFunction(
      "EmbedLiteChromeSessionChild::RetryProgressListener",
      [self = RefPtr<EmbedLiteChromeSessionChild>(this), aTabId]() {
        TabRecord* pending = self->FindTab(aTabId);
        if (!pending) {
          return;
        }
        pending->progressRetryPending = false;
        if (!self->mWindow || !self->mReady || pending->closing) {
          return;
        }
        nsresult retryRv = pending->discarded
          ? self->MaterializeTab(*pending)
          : self->RebindProgressListener(*pending);
        pending = self->FindTab(aTabId);
        if (!pending) {
          return;
        }
        if (NS_SUCCEEDED(retryRv)) {
          if (self->mPendingSelectedTabId == aTabId) {
            self->mPendingSelectedTabId = 0;
            Unused << self->SelectTab(*pending);
          } else {
            self->FinishProgressListenerRebind(*pending);
          }
        } else if (retryRv == NS_ERROR_NOT_AVAILABLE) {
          self->ScheduleProgressListenerRetry(aTabId);
        }
      }),
    kProgressRetryDelayMs);
  if (NS_FAILED(rv)) {
    tab->progressRetryPending = false;
  }
}

nsresult EmbedLiteChromeSessionChild::CreateBrowserForTab(
    TabRecord& aTab, nsIOpenWindowInfo* aOpenWindowInfo,
    const nsAString& aName, bool aInBackground, Element** aBrowser,
    BrowsingContext** aBrowsingContext)
{
  // All browsers are inserted with nodefaultsrc so that the browser DOM
  // window and, when present, nsIOpenWindowInfo are installed before the
  // frame loader performs any navigation. Loads are always dispatched
  // explicitly after the BrowsingContext exists.
  auto cancelOpenWindowInfo = MakeScopeExit([&]() {
    if (aOpenWindowInfo) {
      Unused << aOpenWindowInfo->Cancel();
    }
  });
  NS_ENSURE_TRUE(mTabContainer, NS_ERROR_NOT_INITIALIZED);
  MOZ_RELEASE_ASSERT(!aTab.browser);
  if (aBrowser) {
    *aBrowser = nullptr;
  }
  if (aBrowsingContext) {
    *aBrowsingContext = nullptr;
  }

  Document* document = mTabContainer->OwnerDoc();
  NS_ENSURE_TRUE(document, NS_ERROR_UNEXPECTED);
  RefPtr<Element> browser =
    document->CreateElem(u"browser"_ns, nullptr, kNameSpaceID_XUL);
  NS_ENSURE_TRUE(browser, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = browser->SetAttr(
    kNameSpaceID_None, nsGkAtoms::type, u"content"_ns, false);
  NS_ENSURE_SUCCESS(rv, rv);
  ErrorResult attributeError;
  browser->SetAttribute(
    u"maychangeremoteness"_ns, u"true"_ns, attributeError);
  if (attributeError.Failed()) {
    return attributeError.StealNSResult();
  }
  rv = browser->SetAttr(
    kNameSpaceID_None, nsGkAtoms::manualactiveness, u"true"_ns, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = browser->SetAttr(
    kNameSpaceID_None, nsGkAtoms::flex, u"1"_ns, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = browser->SetAttr(
    kNameSpaceID_None, nsGkAtoms::nodefaultsrc, u"true"_ns, false);
  NS_ENSURE_SUCCESS(rv, rv);

  const bool remote =
    !aOpenWindowInfo || aOpenWindowInfo->GetIsRemote();
  if (remote) {
    rv = browser->SetAttr(
      kNameSpaceID_None, nsGkAtoms::remote, u"true"_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (aOpenWindowInfo) {
    const uint32_t userContextId =
      aOpenWindowInfo->GetOriginAttributes().mUserContextId;
    if (userContextId) {
      nsAutoString value;
      value.AppendInt(userContextId);
      rv = browser->SetAttr(
        kNameSpaceID_None, nsGkAtoms::usercontextid, value, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  if (!aName.IsEmpty()) {
    rv = browser->SetAttr(
      kNameSpaceID_None, nsGkAtoms::name, aName, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (aInBackground) {
    rv = browser->SetAttr(
      kNameSpaceID_None, nsGkAtoms::hidden, u"true"_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  XULFrameElement* xulBrowser = XULFrameElement::FromNode(browser);
  NS_ENSURE_TRUE(xulBrowser, NS_ERROR_UNEXPECTED);
  xulBrowser->SetOpenWindowInfo(aOpenWindowInfo);

  aTab.browser = browser;
  AddBrowserEventListeners(aTab);

  ErrorResult error;
  mTabContainer->AppendChild(*browser, error);
  if (error.Failed()) {
    RemoveBrowserEventListeners(aTab);
    aTab.browser = nullptr;
    return error.StealNSResult();
  }

  RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(browser);
  RefPtr<BrowsingContext> browsingContext =
    frameLoaderOwner ? frameLoaderOwner->GetBrowsingContext() : nullptr;
  if (!browsingContext) {
    RemoveBrowserEventListeners(aTab);
    browser->Remove();
    aTab.browser = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
    aTab.endpointId = service->RegisterChromeTab(
      browsingContext, this, aTab.id);
  }
  if (!aTab.endpointId) {
    RemoveBrowserEventListeners(aTab);
    browser->Remove();
    aTab.browser = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }
  cancelOpenWindowInfo.release();

  rv = RebindProgressListener(aTab);
  if (rv == NS_ERROR_NOT_AVAILABLE && mReady) {
    ScheduleProgressListenerRetry(aTab.id);
  }
  ReplayContentRegistrations(aTab);

  if (aBrowser) {
    browser.forget(aBrowser);
  }
  if (aBrowsingContext) {
    browsingContext.forget(aBrowsingContext);
  }
  return NS_OK;
}

nsresult EmbedLiteChromeSessionChild::CreateTab(
    nsIOpenWindowInfo* aOpenWindowInfo, const nsAString& aName,
    bool aInBackground, bool aSkipLoad, uint64_t aPersistentId,
    TabRecord** aTab, Element** aBrowser,
    BrowsingContext** aBrowsingContext)
{
  Unused << aSkipLoad;
  NS_ENSURE_ARG_POINTER(aTab);
  *aTab = nullptr;

  if (aPersistentId) {
    for (const UniquePtr<TabRecord>& existing : mTabs) {
      NS_ENSURE_TRUE(existing->persistentId != aPersistentId,
                     NS_ERROR_INVALID_ARG);
    }
  }

  uint64_t tabId = mNextTabId++;
  if (!tabId) {
    tabId = mNextTabId++;
  }
  UniquePtr<TabRecord> record =
    MakeUnique<TabRecord>(tabId, aPersistentId);
  TabRecord* tab = record.get();
  mTabs.AppendElement(std::move(record));

  nsresult rv = CreateBrowserForTab(
    *tab, aOpenWindowInfo, aName, aInBackground,
    aBrowser, aBrowsingContext);
  if (NS_FAILED(rv)) {
    MOZ_RELEASE_ASSERT(mTabs.LastElement().get() == tab);
    mTabs.RemoveLastElement();
    return rv;
  }

  if (!aInBackground || !SelectedTab()) {
    SelectTab(*tab);
  } else {
    ApplyTabActiveState(*tab, false);
  }
  UpdateSiblingState();

  *aTab = tab;
  ScheduleTabSnapshot();
  return NS_OK;
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
EmbedLiteChromeSessionChild::RestoreTabHistory(TabRecord& aTab)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTab.restoring);
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  NS_ENSURE_TRUE(aTab.browser && !aTab.history.IsEmpty(),
                 NS_ERROR_NOT_AVAILABLE);

  BrowsingContext* context = BrowsingContextFor(aTab);
  CanonicalBrowsingContext* canonical =
    context ? context->Canonical() : nullptr;
  NS_ENSURE_TRUE(canonical, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsISHistory> sessionHistory = canonical->GetSessionHistory();
  NS_ENSURE_TRUE(sessionHistory, NS_ERROR_NOT_AVAILABLE);

  const int32_t oldCount = sessionHistory->GetCount();
  if (oldCount > 0) {
    nsresult rv = sessionHistory->PurgeHistory(oldCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RefPtr<NullPrincipal> principal =
    NullPrincipal::Create(canonical->OriginAttributesRef());
  NS_ENSURE_TRUE(principal, NS_ERROR_OUT_OF_MEMORY);

  for (const TabHistoryEntry& saved : aTab.history) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), saved.location);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISHEntry> entry;
    rv = sessionHistory->CreateEntry(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = entry->SetURI(uri);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = entry->SetOriginalURI(uri);
    NS_ENSURE_SUCCESS(rv, rv);
    const nsString title = saved.title.IsEmpty()
      ? NS_ConvertUTF8toUTF16(saved.location) : saved.title;
    rv = entry->SetTitle(title);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = entry->SetLoadTypeAsHistory();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = entry->SetHasUserInteraction(true);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = entry->SetTriggeringPrincipal(principal);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = sessionHistory->AddEntry(entry, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = sessionHistory->SetIndex(aTab.selectedHistoryIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  canonical->HistoryCommitIndexAndLength();

  const uint64_t tabId = aTab.id;
  aTab.discarded = false;
  rv = sessionHistory->ReloadCurrentEntry();
  TabRecord* current = self->FindTab(tabId);
  if (!current) {
    return NS_ERROR_ABORT;
  }
  if (NS_FAILED(rv)) {
    current->discarded = true;
    current->restoring = false;
    current->awaitingDocumentLocation = false;
  }
  return rv;
}

nsresult EmbedLiteChromeSessionChild::MaterializeTab(TabRecord& aTab)
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  if (!aTab.discarded) {
    return aTab.browser ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  const uint64_t tabId = aTab.id;
  aTab.restoring = true;
  if (!aTab.browser) {
    nsresult rv = CreateBrowserForTab(
      aTab, nullptr, EmptyString(), true);
    TabRecord* current = self->FindTab(tabId);
    if (NS_FAILED(rv)) {
      if (current) {
        current->restoring = false;
        current->awaitingDocumentLocation = false;
      }
      return rv;
    }
    if (!current) {
      return NS_ERROR_ABORT;
    }
  }

  TabRecord* materialized = self->FindTab(tabId);
  if (!materialized) {
    return NS_ERROR_ABORT;
  }
  nsresult rv = RestoreTabHistory(*materialized);
  TabRecord* current = self->FindTab(tabId);
  if (!current) {
    return NS_ERROR_ABORT;
  }
  if (NS_FAILED(rv)) {
    ApplyTabActiveState(*current, false);
    if (current->endpointId) {
      if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
        service->UnregisterChromeTab(current->endpointId);
      }
      current->endpointId = 0;
    }
    RemoveProgressListener(*current);
    RemoveBrowserEventListeners(*current);
    RefPtr<Element> failedBrowser = current->browser;
    current->browser = nullptr;
    current->discarded = true;
    current->restoring = false;
    current->awaitingDocumentLocation = false;
    current->loading = false;
    current->progress = 0;
    current->current = 0;
    current->total = 0;
    if (failedBrowser) {
      failedBrowser->Remove();
    }
    return rv;
  }

  UpdateSiblingState();
  return NS_OK;
}

void EmbedLiteChromeSessionChild::UpdateSiblingState()
{
  const bool hasSiblings = mTabs.Length() > 1;
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (BrowsingContext* context = BrowsingContextFor(*tab)) {
      MOZ_ALWAYS_SUCCEEDS(context->SetHasSiblings(hasSiblings));
    }
  }
}

bool EmbedLiteChromeSessionChild::LoadTab(
    TabRecord& aTab, const nsACString& aURL, bool aFromExternal,
    nsIReferrerInfo* aReferrerInfo, nsIPrincipal* aTriggeringPrincipal,
    nsIContentSecurityPolicy* aCsp)
{
  BrowsingContext* context = BrowsingContextFor(aTab);
  CanonicalBrowsingContext* browsingContext =
    context ? context->Canonical() : nullptr;
  if (!browsingContext || aURL.IsEmpty()) {
    return false;
  }

  uint32_t flags = nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
  if (Preferences::GetBool("keyword.enabled", true)) {
    flags |= nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
             nsIWebNavigation::LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
  }
  if (aFromExternal) {
    flags |= nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL;
  }

  LoadURIOptions options;
  options.mTriggeringPrincipal = aTriggeringPrincipal
    ? aTriggeringPrincipal : nsContentUtils::GetSystemPrincipal();
  options.mCsp = aCsp;
  options.mReferrerInfo = aReferrerInfo;
  options.mLoadFlags = flags;
  ErrorResult error;
  browsingContext->FixupAndLoadURIString(
    NS_ConvertUTF8toUTF16(aURL), options, error);
  return NS_SUCCEEDED(error.StealNSResult());
}

nsresult EmbedLiteChromeSessionChild::OpenTabFromBrowserDOMWindow(
    nsIURI* aURI, nsIOpenWindowInfo* aOpenWindowInfo,
    nsIReferrerInfo* aReferrerInfo, nsIPrincipal* aTriggeringPrincipal,
    nsIContentSecurityPolicy* aCsp, const nsAString& aName,
    int16_t aWhere, int32_t aFlags, bool aSkipLoad, Element** aBrowser,
    BrowsingContext** aBrowsingContext)
{
  auto cancelOpenWindowInfo = MakeScopeExit([&]() {
    if (aOpenWindowInfo) {
      Unused << aOpenWindowInfo->Cancel();
    }
  });
  if (aBrowser) {
    *aBrowser = nullptr;
  }
  if (aBrowsingContext) {
    *aBrowsingContext = nullptr;
  }

  if (aURI && !aSkipLoad && !aTriggeringPrincipal) {
    return NS_ERROR_INVALID_ARG;
  }
  if ((aFlags & nsIBrowserDOMWindow::OPEN_EXTERNAL) && aOpenWindowInfo) {
    return NS_ERROR_INVALID_ARG;
  }
  if ((aFlags & nsIBrowserDOMWindow::OPEN_EXTERNAL) && aURI &&
      aURI->SchemeIs("chrome")) {
    return NS_ERROR_DOM_BAD_URI;
  }

  if (aWhere == nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW) {
    const char* pref =
      (aFlags & nsIBrowserDOMWindow::OPEN_EXTERNAL) &&
      Preferences::HasUserValue(
        "browser.link.open_newwindow.override.external")
      ? "browser.link.open_newwindow.override.external"
      : "browser.link.open_newwindow";
    aWhere = Preferences::GetInt(
      pref, nsIBrowserDOMWindow::OPEN_NEWTAB);
  }
  if (aWhere == nsIBrowserDOMWindow::OPEN_NEWWINDOW) {
    // Sailfish has one native browser window. Feature-bearing web opens are
    // represented as logical tabs within that AppWindow.
    aWhere = nsIBrowserDOMWindow::OPEN_NEWTAB;
  }
  if (aWhere == nsIBrowserDOMWindow::OPEN_PRINT_BROWSER) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (aWhere == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
    TabRecord* selected = SelectedTab();
    NS_ENSURE_TRUE(selected, NS_ERROR_NOT_AVAILABLE);
    BrowsingContext* context = BrowsingContextFor(*selected);
    NS_ENSURE_TRUE(context, NS_ERROR_NOT_AVAILABLE);
    if (aURI && !aSkipLoad) {
      NS_ENSURE_TRUE(aTriggeringPrincipal, NS_ERROR_INVALID_ARG);
      LoadURIOptions options;
      options.mTriggeringPrincipal = aTriggeringPrincipal;
      options.mCsp = aCsp;
      options.mReferrerInfo = aReferrerInfo;
      if (aFlags & nsIBrowserDOMWindow::OPEN_EXTERNAL) {
        options.mLoadFlags |= nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL;
      } else if (!aTriggeringPrincipal->IsSystemPrincipal()) {
        options.mLoadFlags |= nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD;
      }
      ErrorResult error;
      context->Canonical()->LoadURI(aURI, options, error);
      nsresult rv = error.StealNSResult();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (aBrowser) {
      NS_ADDREF(*aBrowser = selected->browser);
    }
    if (aBrowsingContext) {
      NS_ADDREF(*aBrowsingContext = context);
    }
    cancelOpenWindowInfo.release();
    return NS_OK;
  }

  if (aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB &&
      aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB_BACKGROUND &&
      aWhere != nsIBrowserDOMWindow::OPEN_NEWTAB_FOREGROUND) {
    return NS_ERROR_INVALID_ARG;
  }

  const bool inBackground =
    aWhere == nsIBrowserDOMWindow::OPEN_NEWTAB_BACKGROUND ||
    (aWhere == nsIBrowserDOMWindow::OPEN_NEWTAB &&
     Preferences::GetBool("browser.tabs.loadDivertedInBackground", false));
  TabRecord* tab = nullptr;
  RefPtr<Element> browser;
  RefPtr<BrowsingContext> browsingContext;
  cancelOpenWindowInfo.release();
  nsresult rv = CreateTab(
    aOpenWindowInfo, aName, inBackground, aSkipLoad, 0, &tab,
    getter_AddRefs(browser), getter_AddRefs(browsingContext));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_RELEASE_ASSERT(tab);

  if (aURI && !aSkipLoad) {
    NS_ENSURE_TRUE(aTriggeringPrincipal, NS_ERROR_INVALID_ARG);
    LoadURIOptions options;
    options.mTriggeringPrincipal = aTriggeringPrincipal;
    options.mCsp = aCsp;
    options.mReferrerInfo = aReferrerInfo;
    if (aFlags & nsIBrowserDOMWindow::OPEN_EXTERNAL) {
      options.mLoadFlags |= nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL;
    } else if (!aTriggeringPrincipal->IsSystemPrincipal()) {
      options.mLoadFlags |= nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD;
    }
    if (aOpenWindowInfo) {
      options.mHasValidUserGestureActivation =
        aOpenWindowInfo->GetHasValidUserGestureActivation();
      options.mTextDirectiveUserActivation =
        aOpenWindowInfo->GetTextDirectiveUserActivation();
    }
    ErrorResult error;
    browsingContext->Canonical()->LoadURI(aURI, options, error);
    rv = error.StealNSResult();
    if (NS_FAILED(rv)) {
      RemoveTab(tab->id);
      return rv;
    }
  }

  if (aBrowser) {
    browser.forget(aBrowser);
  }
  if (aBrowsingContext) {
    browsingContext.forget(aBrowsingContext);
  }
  return NS_OK;
}

bool
EmbedLiteChromeSessionChild::LoadURL(
    const nsACString& aURL, bool aFromExternal)
{
  TabRecord* selected = SelectedTab();
  if (!mReady || !selected) {
    return false;
  }
  return LoadTab(*selected, aURL, aFromExternal);
}

bool
EmbedLiteChromeSessionChild::GoBack(
    bool aRequireUserInteraction, bool aUserActivation)
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext) {
    return false;
  }
  Optional<int32_t> noCancelContentJSEpoch;
  browsingContext->GoBack(noCancelContentJSEpoch,
                          aRequireUserInteraction, aUserActivation);
  return true;
}

bool
EmbedLiteChromeSessionChild::GoForward(
    bool aRequireUserInteraction, bool aUserActivation)
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext) {
    return false;
  }
  Optional<int32_t> noCancelContentJSEpoch;
  browsingContext->GoForward(noCancelContentJSEpoch,
                             aRequireUserInteraction, aUserActivation);
  return true;
}

bool
EmbedLiteChromeSessionChild::StopLoad()
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext) {
    return false;
  }
  browsingContext->Stop(nsIWebNavigation::STOP_ALL);
  return true;
}

bool
EmbedLiteChromeSessionChild::Reload(bool aHardReload)
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext) {
    return false;
  }

  uint32_t flags = nsIWebNavigation::LOAD_FLAGS_NONE;
  if (aHardReload) {
    flags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
             nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
  }
  browsingContext->Reload(flags);
  return true;
}

void
EmbedLiteChromeSessionChild::ApplyActiveState()
{
  if (!mReady) {
    return;
  }

  for (const UniquePtr<TabRecord>& tab : mTabs) {
    ApplyTabActiveState(*tab, tab->id == mSelectedTabId);
  }

  if (mWindow && mWindow->GetWidget()) {
    mWindow->GetWidget()->SetActive(mActive);
  }
  if (mActive) {
    ScheduleUpdate();
  }
}

void EmbedLiteChromeSessionChild::ApplyTabActiveState(
    TabRecord& aTab, bool aSelected)
{
  if (!aTab.browser || aTab.discarded) {
    return;
  }
  const bool active = mReady && mActive && aSelected;
  if (aSelected) {
    Unused << aTab.browser->UnsetAttr(
      kNameSpaceID_None, nsGkAtoms::hidden, true);
    Unused << aTab.browser->SetAttr(
      kNameSpaceID_None, nsGkAtoms::primary, u"true"_ns, true);
  } else {
    Unused << aTab.browser->UnsetAttr(
      kNameSpaceID_None, nsGkAtoms::primary, true);
    Unused << aTab.browser->SetAttr(
      kNameSpaceID_None, nsGkAtoms::hidden, u"true"_ns, true);
  }

  BrowsingContext* context = BrowsingContextFor(aTab);
  CanonicalBrowsingContext* browsingContext =
    context ? context->Canonical() : nullptr;
  if (!browsingContext) {
    return;
  }

  ErrorResult error;
  browsingContext->SetIsActive(active, error);
  if (NS_FAILED(error.StealNSResult())) {
    return;
  }
  if (BrowserParent* browserParent = browsingContext->GetBrowserParent()) {
    browserParent->SetRenderLayers(active && !aTab.throttlePainting);
  }
}

void
EmbedLiteChromeSessionChild::ScheduleUpdate()
{
  TabRecord* selected = SelectedTab();
  if (!selected || !selected->browser || !mWindow) {
    return;
  }

  PresShell* presShell = selected->browser->OwnerDoc()->GetPresShell();
  nsIFrame* rootFrame = presShell ? presShell->GetRootFrame() : nullptr;
  if (rootFrame) {
    rootFrame->SchedulePaint();
  }
  if (nsWindow* window = mWindow->GetWidget()) {
    window->ScheduleWebRenderComposite();
  }
}

bool
EmbedLiteChromeSessionChild::SetActive(bool aActive)
{
  if (!mReady) {
    return false;
  }
  mActive = aActive;
  ApplyActiveState();
  return true;
}

void
EmbedLiteChromeSessionChild::ApplyFocusState()
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  nsWindow* rootWidget = mWindow ? mWindow->GetWidget() : nullptr;
  if (!mReady || !rootWidget) {
    return;
  }

  nsCOMPtr<nsIFocusManager> focusManager =
    do_GetService(FOCUSMANAGER_CONTRACTID);
  if (mFocused) {
    // AppWindow activation must precede SetFocus. nsFocusManager otherwise
    // rejects focus in the remote browser because its root is not active.
    if (!rootWidget->SetChromeFocused(true)) {
      return;
    }
    if (!mReady || !mWindow || !mFocused) {
      return;
    }
    TabRecord* selected = SelectedTab();
    if (focusManager && selected && selected->browser) {
      Unused << focusManager->SetFocus(
        selected->browser, nsIFocusManager::FLAG_NOSCROLL);
    }
  } else {
    nsPIDOMWindowOuter* window =
      mTabContainer ? mTabContainer->OwnerDoc()->GetWindow() : nullptr;
    if (focusManager && window) {
      Unused << focusManager->ClearFocus(window);
    }
    // ClearFocus may run script and tear down the hosted window.
    rootWidget = mWindow ? mWindow->GetWidget() : nullptr;
    if (mReady && rootWidget) {
      Unused << rootWidget->SetChromeFocused(false);
    }
  }
}

bool
EmbedLiteChromeSessionChild::SetFocused(bool aFocused)
{
  if (!mReady) {
    return false;
  }
  if (mFocused == aFocused) {
    return true;
  }
  mFocused = aFocused;
  ApplyFocusState();
  return true;
}

bool EmbedLiteChromeSessionChild::SendTextEvent(
    const nsAString& aCommit, const nsAString& aPreEdit,
    int32_t aReplacementStart, int32_t aReplacementLength)
{
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  return mReady && window && window->DispatchChromeTextEvent(
    aCommit, aPreEdit, aReplacementStart, aReplacementLength);
}

bool EmbedLiteChromeSessionChild::SendKeyPress(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  return mReady && window && window->DispatchChromeKeyPress(
    aDomKeyCode, aModifiers, aCharCode);
}

bool EmbedLiteChromeSessionChild::SendKeyRelease(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  return mReady && window && window->DispatchChromeKeyRelease(
    aDomKeyCode, aModifiers, aCharCode);
}

bool
EmbedLiteChromeSessionChild::ReceiveInputEvent(const MultiTouchInput& aEvent)
{
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  if (!mReady || !window) {
    return false;
  }

  WidgetTouchEvent event = aEvent.ToWidgetEvent(window);
  return window->DispatchChromeInputEvent(&event);
}

bool EmbedLiteChromeSessionChild::RestoreTabs(
    const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
    int32_t aSelectedTabIndex)
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  if (!mWindow || mRestoreTabsReceived || !mTabs.IsEmpty() ||
      aTabs.Length() > kMaxRestoredTabs ||
      (aTabs.IsEmpty() && aSelectedTabIndex != -1) ||
      (!aTabs.IsEmpty() &&
       (aSelectedTabIndex < 0 ||
        static_cast<uint32_t>(aSelectedTabIndex) >= aTabs.Length()))) {
    return false;
  }

  int32_t configuredMax =
    Preferences::GetInt("browser.sessionhistory.max_entries", 50);
  const uint32_t maxHistoryEntries =
    configuredMax > 0 ? static_cast<uint32_t>(configuredMax) : 1;
  nsTArray<UniquePtr<TabRecord>> restoredTabs;
  restoredTabs.SetCapacity(aTabs.Length());
  uint64_t nextTabId = mNextTabId;

  for (uint32_t tabIndex = 0; tabIndex < aTabs.Length(); ++tabIndex) {
    const EmbedLiteChromeTabRestoreData& source = aTabs[tabIndex];
    if (!source.persistentId() || source.history().IsEmpty() ||
        source.selectedHistoryIndex() < 0 ||
        static_cast<uint32_t>(source.selectedHistoryIndex()) >=
          source.history().Length()) {
      return false;
    }
    for (uint32_t other = 0; other < tabIndex; ++other) {
      if (aTabs[other].persistentId() == source.persistentId()) {
        return false;
      }
    }

    uint64_t tabId = nextTabId++;
    if (!tabId) {
      tabId = nextTabId++;
    }
    UniquePtr<TabRecord> record =
      MakeUnique<TabRecord>(tabId, source.persistentId());
    record->discarded = true;

    const size_t historyLength = source.history().Length();
    const size_t selectedHistoryIndex =
      static_cast<size_t>(source.selectedHistoryIndex());
    size_t firstHistoryEntry = 0;
    if (historyLength > maxHistoryEntries) {
      firstHistoryEntry = std::min(
        selectedHistoryIndex,
        historyLength - maxHistoryEntries);
    }
    const size_t lastHistoryEntry = std::min(
      historyLength, firstHistoryEntry + maxHistoryEntries);
    record->selectedHistoryIndex = static_cast<uint32_t>(
      selectedHistoryIndex - firstHistoryEntry);

    for (size_t historyIndex = firstHistoryEntry;
         historyIndex < lastHistoryEntry; ++historyIndex) {
      const EmbedLiteChromeHistoryData& sourceEntry =
        source.history()[historyIndex];
      if (sourceEntry.location().IsEmpty() ||
          sourceEntry.location().Length() > kMaxRestoredLocationLength) {
        return false;
      }
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(
        getter_AddRefs(uri), sourceEntry.location());
      if (NS_FAILED(rv) || !uri || uri->SchemeIs("javascript")) {
        return false;
      }
      TabHistoryEntry& entry = *record->history.AppendElement();
      entry.location = sourceEntry.location();
      entry.title = Substring(sourceEntry.title(), 0,
        std::min<size_t>(sourceEntry.title().Length(), kMaxRestoredTitleLength));
    }

    MOZ_RELEASE_ASSERT(
      record->selectedHistoryIndex < record->history.Length());
    const TabHistoryEntry& selectedEntry =
      record->history[record->selectedHistoryIndex];
    record->location = selectedEntry.location;
    record->title = selectedEntry.title;
    record->locationRevision = 1;
    record->canGoBack = record->selectedHistoryIndex > 0;
    record->canGoForward =
      record->selectedHistoryIndex + 1 < record->history.Length();
    restoredTabs.AppendElement(std::move(record));
  }

  mRestoreTabsReceived = true;
  mNextTabId = nextTabId;
  mTabs = std::move(restoredTabs);
  if (!mTabs.IsEmpty()) {
    mSelectedTabId =
      mTabs[static_cast<uint32_t>(aSelectedTabIndex)]->id;
  }

  TabRecord* selected = SelectedTab();
  if (mTabContainer && selected) {
    const uint64_t selectedTabId = selected->id;
    nsresult rv = MaterializeTab(*selected);
    if (rv == NS_ERROR_NOT_AVAILABLE && mReady) {
      ScheduleProgressListenerRetry(selectedTabId);
    }
    selected = FindTab(selectedTabId);
  }
  if (mReady) {
    if (selected && !selected->discarded) {
      Unused << SelectTab(*selected);
    }
    UpdateSiblingState();
    ScheduleTabSnapshot();
  }
  return true;
}

bool EmbedLiteChromeSessionChild::AssociateTab(
    uint64_t aTabId, uint64_t aPersistentId)
{
  TabRecord* tab = FindTab(aTabId);
  if (!mReady || !tab || !aPersistentId || tab->persistentId) {
    return false;
  }
  for (const UniquePtr<TabRecord>& other : mTabs) {
    if (other->persistentId == aPersistentId) {
      return false;
    }
  }
  tab->persistentId = aPersistentId;
  ScheduleTabSnapshot();
  SendContentState(*tab);
  return true;
}

bool EmbedLiteChromeSessionChild::NewTab(
    const nsACString& aURL, uint64_t aPersistentId,
    bool aFromExternal, bool aInBackground)
{
  if (!mReady) {
    return false;
  }

  TabRecord* tab = nullptr;
  nsresult rv = CreateTab(
    nullptr, EmptyString(), aInBackground, true, aPersistentId, &tab);
  if (NS_FAILED(rv) || !tab) {
    return false;
  }

  const nsCString url = aURL.IsEmpty()
    ? nsCString("about:blank") : nsCString(aURL);
  if (!LoadTab(*tab, url, aFromExternal)) {
    RemoveTab(tab->id);
    return false;
  }
  return true;
}

bool EmbedLiteChromeSessionChild::SelectTab(uint64_t aTabId)
{
  TabRecord* tab = FindTab(aTabId);
  return mReady && tab && SelectTab(*tab);
}

bool EmbedLiteChromeSessionChild::SelectTab(TabRecord& aTab)
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  if (aTab.closing) {
    return false;
  }
  if (mPendingSelectedTabId && mPendingSelectedTabId != aTab.id) {
    mPendingSelectedTabId = 0;
  }

  const uint64_t tabId = aTab.id;
  if (aTab.discarded) {
    nsresult rv = MaterializeTab(aTab);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_NOT_AVAILABLE && mReady) {
        mPendingSelectedTabId = tabId;
        ScheduleProgressListenerRetry(tabId);
      }
      return false;
    }
  }
  TabRecord* target = FindTab(tabId);
  if (!target || !target->browser) {
    return false;
  }
  if (mPendingSelectedTabId == tabId) {
    mPendingSelectedTabId = 0;
  }

  if (mSelectedTabId == target->id) {
    ApplyTabActiveState(*target, true);
    ApplyFocusState();
    ScheduleTabSnapshot();
    SendContentState(*target);
    return true;
  }

  if (mFocused && mTabContainer) {
    if (nsCOMPtr<nsIFocusManager> focusManager =
          do_GetService(FOCUSMANAGER_CONTRACTID)) {
      nsPIDOMWindowOuter* window = mTabContainer->OwnerDoc()->GetWindow();
      if (window) {
        Unused << focusManager->ClearFocus(window);
      }
    }
  }

  TabRecord* oldTab = SelectedTab();
  // Establish the replacement as AppWindow's primary browser before
  // removing the old primary, avoiding a transient null primary actor.
  Unused << target->browser->UnsetAttr(
    kNameSpaceID_None, nsGkAtoms::hidden, true);
  Unused << target->browser->SetAttr(
    kNameSpaceID_None, nsGkAtoms::primary, u"true"_ns, true);
  mSelectedTabId = target->id;
  if (oldTab) {
    ApplyTabActiveState(*oldTab, false);
  }
  ApplyTabActiveState(*target, true);
  UpdateSiblingState();

  UpdateLocation(*target);
  UpdateTitle(*target);
  ApplyFocusState();
  if (mReady && mActive) {
    ScheduleUpdate();
  }
  ScheduleTabSnapshot();
  SendContentState(*target);
  return true;
}

bool EmbedLiteChromeSessionChild::LoadFrameScript(const nsACString& aURI)
{
  if (aURI.IsEmpty() || mShuttingDown) {
    return false;
  }
  if (mContentRegistrations.AddFrameScript(aURI)) {
    for (const UniquePtr<TabRecord>& tab : mTabs) {
      if (tab->browser && !tab->discarded) {
        DispatchContentCommand(*tab, u"load-script"_ns,
                               NS_ConvertUTF8toUTF16(aURI));
      }
    }
  }
  return true;
}

bool EmbedLiteChromeSessionChild::AddMessageListener(const nsACString& aName)
{
  if (aName.IsEmpty() || mShuttingDown) {
    return false;
  }
  if (mContentRegistrations.AddMessageListener(aName)) {
    for (const UniquePtr<TabRecord>& tab : mTabs) {
      if (!tab->browser || tab->discarded) {
        continue;
      }
      ErrorResult error;
      tab->browser->SetAttribute(u"data-embedlite-command-name"_ns,
                                 NS_ConvertUTF8toUTF16(aName), error);
      if (!error.Failed()) {
        DispatchContentCommand(*tab, u"add-listener"_ns);
      } else {
        error.SuppressException();
      }
    }
  }
  return true;
}

bool EmbedLiteChromeSessionChild::RemoveMessageListener(
    const nsACString& aName)
{
  if (!mContentRegistrations.RemoveMessageListener(aName)) {
    return true;
  }
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    if (!tab->browser || tab->discarded) {
      continue;
    }
    ErrorResult error;
    tab->browser->SetAttribute(u"data-embedlite-command-name"_ns,
                               NS_ConvertUTF8toUTF16(aName), error);
    if (!error.Failed()) {
      DispatchContentCommand(*tab, u"remove-listener"_ns);
    } else {
      error.SuppressException();
    }
  }
  return true;
}

bool EmbedLiteChromeSessionChild::SendAsyncMessage(
    uint64_t aTabId, const nsAString& aName, const nsAString& aJSON)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || !tab->browser || tab->discarded || tab->restoring) {
    return false;
  }
  ErrorResult error;
  tab->browser->SetAttribute(
    u"data-embedlite-command-name"_ns, aName, error);
  if (error.Failed() ||
      !DispatchContentCommand(*tab, u"send-message"_ns, aJSON)) {
    error.SuppressException();
    return false;
  }
  if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
    nsString json(aJSON);
    service->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aName).get(), json);
  }
  return true;
}

bool EmbedLiteChromeSessionChild::SendContentMessageFromAppService(
    uint64_t aTabId, const nsAString& aName, const nsAString& aJSON)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || !tab->browser || tab->discarded || tab->restoring) {
    return false;
  }
  ErrorResult error;
  tab->browser->SetAttribute(
    u"data-embedlite-command-name"_ns, aName, error);
  if (error.Failed()) {
    error.SuppressException();
    return false;
  }
  return DispatchContentCommand(*tab, u"send-message"_ns, aJSON);
}

uint32_t EmbedLiteChromeSessionChild::SelectedEndpointId() const
{
  TabRecord* tab = SelectedTab();
  return tab && tab->browser && !tab->discarded && !tab->restoring
    ? tab->endpointId : 0;
}

bool EmbedLiteChromeSessionChild::SendContentMessageToEmbedder(
    uint64_t aTabId, const nsAString& aName, const nsAString& aJSON)
{
  TabRecord* tab = FindTab(aTabId);
  if (!mWindow || !tab ||
      !IsChromeContentNotificationBounded(
        aName.Length(), aJSON.Length())) {
    return false;
  }
  return SendAfterPendingChromeTabSnapshot(
    mTabSnapshotPending,
    [this]() { SendTabSnapshot(); },
    [this, tab, &aName, &aJSON]() {
      return mWindow->SendOnContentAsyncMessage(
        tab->id, tab->persistentId, tab->locationRevision,
        nsString(aName), nsString(aJSON));
    });
}

bool EmbedLiteChromeSessionChild::ScrollTo(
    uint64_t aTabId, int32_t aX, int32_t aY)
{
  TabRecord* tab = FindTab(aTabId);
  nsAutoCString json;
  json.AppendPrintf("{\"x\":%d,\"y\":%d}", aX, aY);
  return tab && DispatchContentCommand(
    *tab, u"scroll-to"_ns, NS_ConvertUTF8toUTF16(json));
}

bool EmbedLiteChromeSessionChild::ScrollBy(
    uint64_t aTabId, int32_t aX, int32_t aY)
{
  TabRecord* tab = FindTab(aTabId);
  nsAutoCString json;
  json.AppendPrintf("{\"x\":%d,\"y\":%d}", aX, aY);
  return tab && DispatchContentCommand(
    *tab, u"scroll-by"_ns, NS_ConvertUTF8toUTF16(json));
}

bool EmbedLiteChromeSessionChild::SetHttpUserAgent(
    uint64_t aTabId, const nsAString& aUserAgent)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab) {
    return false;
  }
  ErrorResult error;
  tab->browser->SetAttribute(u"data-embedlite-command-name"_ns,
                             aUserAgent, error);
  if (error.Failed() || !DispatchContentCommand(
        *tab, u"set-user-agent"_ns, u"{}"_ns)) {
    error.SuppressException();
    return false;
  }
  tab->httpUserAgent = aUserAgent;
  tab->hasHttpUserAgent = true;
  return true;
}

bool EmbedLiteChromeSessionChild::SetThrottlePainting(
    uint64_t aTabId, bool aThrottle)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || !tab->browser || tab->discarded || tab->restoring) {
    return false;
  }
  if (tab->throttlePainting == aThrottle) {
    return true;
  }
  tab->throttlePainting = aThrottle;
  ApplyTabActiveState(*tab, tab->id == mSelectedTabId);
  if (!aThrottle && tab->id == mSelectedTabId && mActive) {
    ScheduleUpdate();
  }
  return true;
}

bool EmbedLiteChromeSessionChild::SuspendTimeouts(uint64_t aTabId)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || !DispatchContentCommand(
        *tab, u"suspend-timeouts"_ns, u"{}"_ns)) {
    return false;
  }
  tab->timeoutsSuspended = true;
  return true;
}
bool EmbedLiteChromeSessionChild::ResumeTimeouts(uint64_t aTabId)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || !DispatchContentCommand(
        *tab, u"resume-timeouts"_ns, u"{}"_ns)) {
    return false;
  }
  tab->timeoutsSuspended = false;
  return true;
}

bool EmbedLiteChromeSessionChild::SetDesktopMode(uint64_t aTabId, bool aValue)
{
  TabRecord* tab = FindTab(aTabId);
  BrowsingContext* context = tab ? BrowsingContextFor(*tab) : nullptr;
  if (!context || tab->id != mSelectedTabId || tab->discarded ||
      tab->restoring) {
    return false;
  }
  context = context->Top();
  if (context->ForceDesktopViewport() == aValue) {
    return true;
  }
  return NS_SUCCEEDED(context->SetForceDesktopViewport(aValue));
}

bool EmbedLiteChromeSessionChild::SetMargins(uint64_t aTabId, int32_t aTop,
    int32_t aRight, int32_t aBottom, int32_t aLeft)
{
  TabRecord* tab = FindTab(aTabId);
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  return tab && tab->id == mSelectedTabId && !tab->discarded &&
    !tab->restoring && window && window->SetChromeMargins(
      LayoutDeviceIntMargin(aTop, aRight, aBottom, aLeft));
}
bool EmbedLiteChromeSessionChild::SetSafeAreaInsets(uint64_t aTabId,
    int32_t aTop, int32_t aRight, int32_t aBottom, int32_t aLeft)
{
  TabRecord* tab = FindTab(aTabId);
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  return tab && tab->id == mSelectedTabId && !tab->discarded &&
    !tab->restoring && window && window->SetChromeSafeAreaInsets(
      LayoutDeviceIntMargin(aTop, aRight, aBottom, aLeft));
}
bool EmbedLiteChromeSessionChild::SetDynamicToolbarHeight(
    uint64_t aTabId, int32_t aHeight)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || tab->id != mSelectedTabId || !tab->browser ||
      tab->discarded || tab->restoring || aHeight < 0) {
    return false;
  }
  nsAutoCString json;
  json.AppendPrintf("{\"height\":%d}", aHeight);
  if (!DispatchContentCommand(*tab, u"set-toolbar-height"_ns,
                              NS_ConvertUTF8toUTF16(json))) {
    return false;
  }
  tab->dynamicToolbarHeight = aHeight;
  tab->hasDynamicToolbarHeight = true;
  return true;
}

bool EmbedLiteChromeSessionChild::SendMouseEvent(
    uint64_t aTabId, uint8_t aType, int32_t aX, int32_t aY, uint64_t aTime,
    uint32_t aButton, uint32_t aButtons, uint32_t aModifiers,
    uint32_t aClickCount)
{
  TabRecord* tab = FindTab(aTabId);
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  if (!tab || tab->id != mSelectedTabId || tab->discarded ||
      tab->restoring || !window || aType > 2) {
    return false;
  }
  EventMessage message = aType == 0 ? eMouseMove
    : (aType == 1 ? eMouseDown : eMouseUp);
  WidgetMouseEvent event(true, message, window,
                         WidgetMouseEvent::eReal);
  event.mRefPoint = LayoutDeviceIntPoint(aX, aY);
  Unused << aTime;
  event.mButton = static_cast<int16_t>(aButton);
  event.mButtons = static_cast<int16_t>(aButtons);
  event.mModifiers = static_cast<Modifiers>(aModifiers);
  event.mClickCount = aClickCount;
  return window->DispatchChromeInputEvent(&event);
}

bool EmbedLiteChromeSessionChild::SendWheelEvent(
    uint64_t aTabId, int32_t aX, int32_t aY, uint64_t aTime,
    double aDeltaX, double aDeltaY, uint32_t aDeltaMode, uint32_t aModifiers)
{
  TabRecord* tab = FindTab(aTabId);
  nsWindow* window = mWindow ? mWindow->GetWidget() : nullptr;
  if (!tab || tab->id != mSelectedTabId || tab->discarded ||
      tab->restoring || !window || aDeltaMode > 2) {
    return false;
  }
  WidgetWheelEvent event(true, eWheel, window);
  event.mRefPoint = LayoutDeviceIntPoint(aX, aY);
  Unused << aTime;
  event.mDeltaX = aDeltaX;
  event.mDeltaY = aDeltaY;
  event.mDeltaMode = aDeltaMode;
  event.mModifiers = static_cast<Modifiers>(aModifiers);
  return window->DispatchChromeInputEvent(&event);
}

bool EmbedLiteChromeSessionChild::ZoomToRect(
    uint64_t aTabId, float aX, float aY, float aWidth, float aHeight)
{
  TabRecord* tab = FindTab(aTabId);
  if (!tab || tab->id != mSelectedTabId || !tab->browser ||
      tab->discarded || tab->restoring || !mWindow ||
      !mWindow->GetWidget() || !std::isfinite(aX) ||
      !std::isfinite(aY) || !std::isfinite(aWidth) || aWidth < 0 ||
      !std::isfinite(aHeight) || aHeight < 0) {
    return false;
  }
  // The selected browser's root identifiers are supplied by the frame script
  // in the content-state event, so this remains correct across Fission and
  // remoteness changes.
  nsAutoString presShellValue;
  nsAutoString viewValue;
  tab->browser->GetAttribute(u"data-embedlite-pres-shell-id"_ns,
                             presShellValue);
  tab->browser->GetAttribute(u"data-embedlite-view-id"_ns, viewValue);
  nsresult rv = NS_OK;
  uint32_t presShellId = presShellValue.ToInteger(&rv);
  if (NS_FAILED(rv)) {
    return false;
  }
  uint64_t viewId = 0;
  if (!ParseUint64String(viewValue, &viewId)) {
    return false;
  }
  mWindow->GetWidget()->ZoomToRect(presShellId, viewId,
    CSSRect(aX, aY, aWidth, aHeight), 0);
  return true;
}

bool EmbedLiteChromeSessionChild::CloseTab(uint64_t aTabId)
{
  TabRecord* tab = FindTab(aTabId);
  if (!mReady || mCheckingCanClose || !tab || tab->closing) {
    SendTabCloseResult(aTabId, false);
    return false;
  }

  for (const UniquePtr<TabRecord>& other : mTabs) {
    if (other->closing) {
      SendTabCloseResult(aTabId, false);
      return false;
    }
  }

  tab->closing = true;
  ScheduleTabSnapshot();
  if (tab->discarded || tab->restoring) {
    const bool removed = RemoveTab(aTabId);
    SendTabCloseResult(aTabId, removed);
    return true;
  }
  BrowsingContext* context = BrowsingContextFor(*tab);
  WindowGlobalParent* windowGlobal = context
    ? context->Canonical()->GetCurrentWindowGlobal() : nullptr;
  if (!windowGlobal) {
    const bool removed = RemoveTab(aTabId);
    SendTabCloseResult(aTabId, removed);
    return true;
  }

  ErrorResult error;
  RefPtr<Promise> promise = windowGlobal->PermitUnload(
    PermitUnloadAction::Prompt,
    Preferences::GetUint("dom.beforeunload_timeout_ms", 1000), error);
  if (error.Failed()) {
    error.SuppressException();
    tab->closing = false;
    ScheduleTabSnapshot();
    SendTabCloseResult(aTabId, false);
    return false;
  }
  if (!promise) {
    tab->closing = false;
    ScheduleTabSnapshot();
    SendTabCloseResult(aTabId, false);
    return false;
  }

  RefPtr<PermitUnloadPromiseHandler> handler =
    new PermitUnloadPromiseHandler(
    [self = RefPtr<EmbedLiteChromeSessionChild>(this), aTabId](bool aPermit) {
      TabRecord* pending = self->FindTab(aTabId);
      if (!pending || !pending->closing) {
        self->SendTabCloseResult(aTabId, false);
        return;
      }
      if (!aPermit) {
        pending->closing = false;
        self->ScheduleTabSnapshot();
        self->SendTabCloseResult(aTabId, false);
        return;
      }
      const bool removed = self->RemoveTab(aTabId);
      self->SendTabCloseResult(aTabId, removed);
    });
  promise->AppendNativeHandler(handler);
  return true;
}

void EmbedLiteChromeSessionChild::SendTabCloseResult(
    uint64_t aTabId, bool aClosed)
{
  if (!mWindow || !aTabId) {
    return;
  }
  SendAfterPendingChromeTabSnapshot(
    mTabSnapshotPending,
    [this]() { SendTabSnapshot(); },
    [this, aTabId, aClosed]() {
      Unused << mWindow->SendOnTabCloseResult(aTabId, aClosed);
    });
}

bool EmbedLiteChromeSessionChild::RemoveTab(uint64_t aTabId)
{
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  TabRecord* tab = FindTab(aTabId);
  if (!tab) {
    return false;
  }

  CancelBeforeUnloadPrompts(aTabId);

  if (mTabs.Length() == 1) {
    MOZ_RELEASE_ASSERT(tab->id == mSelectedTabId);
    mSelectedTabId = 0;
  } else if (tab->id == mSelectedTabId) {
    TabRecord* replacement = nullptr;
    for (uint32_t index = 0; index < mTabs.Length(); ++index) {
      if (mTabs[index].get() != tab) {
        continue;
      }
      for (uint32_t candidate = index + 1;
           candidate < mTabs.Length(); ++candidate) {
        if (!mTabs[candidate]->closing) {
          replacement = mTabs[candidate].get();
          break;
        }
      }
      for (uint32_t candidate = index;
           !replacement && candidate > 0; --candidate) {
        if (!mTabs[candidate - 1]->closing) {
          replacement = mTabs[candidate - 1].get();
          break;
        }
      }
      break;
    }
    if (!replacement) {
      if (NS_FAILED(CreateTab(
            nullptr, EmptyString(), false, true, 0, &replacement)) ||
          !replacement) {
        tab->closing = false;
        ScheduleTabSnapshot();
        return false;
      }
      Unused << LoadTab(*replacement, "about:blank"_ns, false);
    }
    if (!SelectTab(*replacement)) {
      tab->closing = false;
      ScheduleTabSnapshot();
      return false;
    }
  }

  tab = FindTab(aTabId);
  if (!tab) {
    return false;
  }
  ApplyTabActiveState(*tab, false);
  if (tab->endpointId) {
    if (EmbedLiteAppService* service = EmbedLiteAppService::AppService()) {
      service->UnregisterChromeTab(tab->endpointId);
    }
    tab->endpointId = 0;
  }
  RemoveProgressListener(*tab);
  RemoveBrowserEventListeners(*tab);
  RefPtr<Element> browser = tab->browser;
  for (uint32_t index = 0; index < mTabs.Length(); ++index) {
    if (mTabs[index].get() == tab) {
      mTabs.RemoveElementAt(index);
      break;
    }
  }
  if (browser) {
    browser->Remove();
  }
  if (mPendingSelectedTabId == aTabId) {
    mPendingSelectedTabId = 0;
  }

  MOZ_RELEASE_ASSERT(mTabs.IsEmpty() == (mSelectedTabId == 0));
  MOZ_RELEASE_ASSERT(mTabs.IsEmpty() || SelectedTab());

  UpdateSiblingState();
  ScheduleTabSnapshot();
  return true;
}

bool EmbedLiteChromeSessionChild::CanCloseTabs()
{
  if (mCheckingCanClose) {
    return false;
  }
  mCheckingCanClose = true;
  auto resetCheckingCanClose =
    MakeScopeExit([&]() { mCheckingCanClose = false; });
  RefPtr<EmbedLiteChromeSessionChild> self(this);
  AutoTArray<uint64_t, 8> tabIds;
  tabIds.SetCapacity(mTabs.Length());
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    tabIds.AppendElement(tab->id);
  }

  for (uint64_t tabId : tabIds) {
    TabRecord* tab = FindTab(tabId);
    if (!tab) {
      continue;
    }
    if (tab->closing) {
      return false;
    }
    if (tab->discarded || tab->restoring) {
      continue;
    }
    BrowsingContext* context = BrowsingContextFor(*tab);
    RefPtr<WindowGlobalParent> windowGlobal = context
      ? context->Canonical()->GetCurrentWindowGlobal() : nullptr;
    if (!windowGlobal) {
      continue;
    }

    ErrorResult error;
    RefPtr<Promise> promise = windowGlobal->PermitUnload(
      PermitUnloadAction::Prompt,
      Preferences::GetUint("dom.beforeunload_timeout_ms", 1000), error);
    if (error.Failed()) {
      error.SuppressException();
      return false;
    }
    if (!promise) {
      return false;
    }

    RefPtr<PermitUnloadResult> result = new PermitUnloadResult();
    RefPtr<PermitUnloadPromiseHandler> handler =
      new PermitUnloadPromiseHandler([result](bool aPermit) {
        result->permit = aPermit;
        result->done = true;
      });
    promise->AppendNativeHandler(handler);
    const bool completed = SpinEventLoopUntil(
      "EmbedLiteChromeSessionChild::CanCloseTabs"_ns,
      [result, self]() { return result->done || !self->mWindow; });
    if (!self->mWindow) {
      return true;
    }
    if (!completed || !result->permit) {
      return false;
    }
  }
  return true;
}

void EmbedLiteChromeSessionChild::UpdateLocation(
    TabRecord& aTab, nsIURI* aLocation)
{
  if (aTab.discarded || aTab.restoring) {
    return;
  }
  BrowsingContext* context = BrowsingContextFor(aTab);
  CanonicalBrowsingContext* browsingContext =
    context ? context->Canonical() : nullptr;
  if (!browsingContext) {
    return;
  }

  nsCOMPtr<nsIURI> location = aLocation;
  if (!location) {
    location = browsingContext->GetCurrentURI();
  }
  nsAutoCString spec;
  if (location) {
    Unused << location->GetSpec(spec);
  }

  bool canGoBack = false;
  bool canGoForward = false;
  if (ChildSHistory* history = browsingContext->GetChildSessionHistory()) {
    const bool requireUserInteraction =
      StaticPrefs::browser_navigation_requireUserInteraction();
    canGoBack = history->CanGo(-1, requireUserInteraction);
    canGoForward = history->CanGo(1, requireUserInteraction);
  }

  bool changed = false;
  bool locationChanged = false;
  if (aTab.location != spec) {
    aTab.location = spec;
    if (!++aTab.locationRevision) {
      ++aTab.locationRevision;
    }
    changed = true;
    locationChanged = true;
  }
  if (aTab.canGoBack != canGoBack ||
      aTab.canGoForward != canGoForward) {
    aTab.canGoBack = canGoBack;
    aTab.canGoForward = canGoForward;
    changed = true;
  }
  if (changed) {
    ScheduleTabSnapshot();
  }
  if (locationChanged) {
    SendContentState(aTab);
  }
}

void EmbedLiteChromeSessionChild::UpdateTitle(TabRecord& aTab)
{
  if (aTab.discarded || aTab.restoring) {
    return;
  }
  nsAutoString title;
  BrowsingContext* context = BrowsingContextFor(aTab);
  if (CanonicalBrowsingContext* browsingContext =
        context ? context->Canonical() : nullptr) {
    if (WindowGlobalParent* windowGlobal =
          browsingContext->GetCurrentWindowGlobal()) {
      windowGlobal->GetDocumentTitle(title);
    } else if (Document* document = browsingContext->GetDocument()) {
      document->GetTitle(title);
    }
  }
  if (aTab.title != title) {
    aTab.title = title;
    ScheduleTabSnapshot();
  }
}

void EmbedLiteChromeSessionChild::ScheduleTabSnapshot()
{
  if (!mWindow || !mReady || !mInitializationFinished ||
      mTabSnapshotPending) {
    return;
  }

  mTabSnapshotPending = true;
  nsresult rv = NS_DispatchToCurrentThread(NS_NewRunnableFunction(
    "EmbedLiteChromeSessionChild::SendTabSnapshot",
    [self = RefPtr<EmbedLiteChromeSessionChild>(this)]() {
      self->SendTabSnapshot();
    }));
  if (NS_FAILED(rv)) {
    mTabSnapshotPending = false;
  }
}

void EmbedLiteChromeSessionChild::SendTabSnapshot()
{
  mTabSnapshotPending = false;
  if (!mWindow || !mReady || !mInitializationFinished) {
    return;
  }

  EmbedLiteChromeSessionData snapshot;
  if (!++mTabRevision) {
    ++mTabRevision;
  }
  snapshot.revision() = mTabRevision;
  snapshot.selectedTabId() = mSelectedTabId;
  snapshot.tabs().SetCapacity(mTabs.Length());
  for (const UniquePtr<TabRecord>& tab : mTabs) {
    EmbedLiteChromeTabData data;
    data.id() = tab->id;
    data.persistentId() = tab->persistentId;
    data.locationRevision() = tab->locationRevision;
    data.location() = tab->location;
    data.title() = tab->title;
    data.loading() = tab->loading;
    data.closing() = tab->closing;
    data.discarded() = tab->discarded;
    data.canGoBack() = tab->canGoBack;
    data.canGoForward() = tab->canGoForward;
    data.progress() = tab->progress;
    data.current() = tab->current;
    data.total() = tab->total;
    snapshot.tabs().AppendElement(std::move(data));
  }
  Unused << mWindow->SendOnTabSnapshot(snapshot);
}

bool EmbedLiteChromeSessionChild::RequestBeforeUnloadPrompt(
    BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  TabRecord* tab = FindTab(aBrowsingContext);
  if (!mWindow || !mReady || !tab || !aPromise) {
    return false;
  }

  uint64_t requestId;
  do {
    requestId = mNextBeforeUnloadPromptId++;
  } while (!requestId || mBeforeUnloadPrompts.count(requestId));

  EmbedLiteChromeBeforeUnloadData prompt;
  prompt.requestId() = requestId;
  prompt.tabId() = tab->id;
  prompt.persistentId() = tab->persistentId;
  prompt.title() = aTitle;
  prompt.text() = aText;
  prompt.leaveLabel() = aLeaveLabel;
  prompt.stayLabel() = aStayLabel;

  mBeforeUnloadPrompts.emplace(
    requestId, PendingBeforeUnloadPrompt{tab->id, aPromise});
  const bool sent = SendAfterPendingChromeTabSnapshot(
    mTabSnapshotPending,
    [this]() { SendTabSnapshot(); },
    [this, &prompt]() {
      return mWindow->SendOnBeforeUnloadPrompt(prompt);
    });
  if (!sent) {
    auto pending = mBeforeUnloadPrompts.find(requestId);
    if (pending != mBeforeUnloadPrompts.end()) {
      pending->second.promise->MaybeResolve(false);
      mBeforeUnloadPrompts.erase(pending);
    }
  }
  return true;
}

void EmbedLiteChromeSessionChild::ResolveBeforeUnloadPrompt(
    uint64_t aRequestId, uint64_t aTabId, bool aPermit)
{
  MOZ_ASSERT(NS_IsMainThread());
  const auto pending = mBeforeUnloadPrompts.find(aRequestId);
  if (pending == mBeforeUnloadPrompts.end()) {
    return;
  }

  const bool valid = pending->second.tabId == aTabId && FindTab(aTabId);
  RefPtr<Promise> promise = pending->second.promise;
  mBeforeUnloadPrompts.erase(pending);
  promise->MaybeResolve(valid && aPermit);
}

void EmbedLiteChromeSessionChild::CancelBeforeUnloadPrompts(uint64_t aTabId)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (auto prompt = mBeforeUnloadPrompts.begin();
       prompt != mBeforeUnloadPrompts.end();) {
    if (!aTabId || prompt->second.tabId == aTabId) {
      prompt->second.promise->MaybeResolve(false);
      prompt = mBeforeUnloadPrompts.erase(prompt);
    } else {
      ++prompt;
    }
  }
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::HandleEvent(Event* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  Element* browser =
    Element::FromEventTargetOrNull(aEvent->GetCurrentTarget());
  TabRecord* tab = FindTab(browser);
  if (!tab) {
    return NS_OK;
  }

  if (type.EqualsLiteral("DidChangeBrowserRemoteness") ||
      type.EqualsLiteral("XULFrameLoaderCreated")) {
    tab->contentBridgeReady = false;
    BrowsingContext* browsingContext = BrowsingContextFor(*tab);
    if (tab->endpointId && browsingContext) {
      EmbedLiteAppService* service = EmbedLiteAppService::AppService();
      if (!service || !service->UpdateChromeTabBrowsingContext(
                        tab->endpointId, browsingContext)) {
        return NS_ERROR_FAILURE;
      }
    }
    if (!mReady) {
      nsresult rv = TryCompleteInitialization();
      if (NS_SUCCEEDED(rv)) {
        ScheduleInitializationCompletion(true);
      } else if (rv != NS_ERROR_NOT_AVAILABLE) {
        ScheduleInitializationCompletion(false);
      } else {
        ScheduleInitializationRetry();
      }
      return NS_OK;
    }
    ReplayContentRegistrations(*tab);
    nsresult rv = RebindProgressListener(*tab);
    if (NS_SUCCEEDED(rv)) {
      FinishProgressListenerRebind(*tab);
    } else if (rv == NS_ERROR_NOT_AVAILABLE) {
      ScheduleProgressListenerRetry(tab->id);
    }
  } else if (type.EqualsLiteral("DOMTitleChanged") ||
             type.EqualsLiteral("pagetitlechanged")) {
    UpdateTitle(*tab);
  } else if (type.EqualsLiteral("DOMWindowClose")) {
    aEvent->PreventDefault();
    if (!tab->restoring) {
      SendAfterPendingChromeTabSnapshot(
        mTabSnapshotPending,
        [this]() { SendTabSnapshot(); },
        [this, tab]() {
          Unused << mWindow->SendOnContentWindowCloseRequested(
            tab->id, tab->persistentId);
        });
      RemoveTab(tab->id);
    }
  } else if (type.EqualsLiteral("EmbedLiteChromeContentState")) {
    auto getAttribute = [browser](const char16_t* aName) {
      nsAutoString value;
      browser->GetAttribute(nsDependentString(aName), value);
      return value;
    };
    uint64_t innerWindowId = 0;
    BrowsingContext* context = BrowsingContextFor(*tab);
    WindowGlobalParent* windowGlobal = context
      ? context->Top()->Canonical()->GetCurrentWindowGlobal() : nullptr;
    if (!ParseUint64String(
          getAttribute(u"data-embedlite-event-innerWindowId"),
          &innerWindowId) || !windowGlobal ||
        windowGlobal->InnerWindowId() != innerWindowId) {
      return NS_OK;
    }
    if (!tab->contentBridgeReady) {
      // browser.js has received a message from its internal frame script for
      // the current window, so both its command listener and this browser's
      // message manager are ready. Replay registrations that may have raced
      // bridge attachment.
      tab->contentBridgeReady = true;
      ReplayContentRegistrations(*tab);
    }
    if (tab->awaitingDocumentLocation) {
      return NS_OK;
    }
    nsresult rv = NS_OK;
    tab->fullscreen = getAttribute(
      u"data-embedlite-event-fullscreen").EqualsLiteral("true");
    tab->firstPaint = getAttribute(
      u"data-embedlite-event-firstPaint").EqualsLiteral("true");
    tab->firstPaintX = getAttribute(
      u"data-embedlite-event-firstPaintX").ToInteger(&rv);
    rv = NS_OK;
    tab->firstPaintY = getAttribute(
      u"data-embedlite-event-firstPaintY").ToInteger(&rv);
    rv = NS_OK;
    tab->scrollWidth = std::max(0, getAttribute(
      u"data-embedlite-event-scrollWidth").ToInteger(&rv));
    rv = NS_OK;
    tab->scrollHeight = std::max(0, getAttribute(
      u"data-embedlite-event-scrollHeight").ToInteger(&rv));
    rv = NS_OK;
    tab->scrollX = getAttribute(
      u"data-embedlite-event-scrollX").ToInteger(&rv);
    rv = NS_OK;
    tab->scrollY = getAttribute(
      u"data-embedlite-event-scrollY").ToInteger(&rv);
    rv = NS_OK;
    tab->viewportX = getAttribute(
      u"data-embedlite-event-viewportX").ToDouble(&rv);
    if (NS_FAILED(rv) || !std::isfinite(tab->viewportX)) {
      tab->viewportX = 0;
    }
    rv = NS_OK;
    tab->viewportY = getAttribute(
      u"data-embedlite-event-viewportY").ToDouble(&rv);
    if (NS_FAILED(rv) || !std::isfinite(tab->viewportY)) {
      tab->viewportY = 0;
    }
    rv = NS_OK;
    tab->viewportWidth = getAttribute(
      u"data-embedlite-event-viewportWidth").ToDouble(&rv);
    if (NS_FAILED(rv) || !std::isfinite(tab->viewportWidth) ||
        tab->viewportWidth < 0) {
      tab->viewportWidth = 0;
    }
    rv = NS_OK;
    tab->viewportHeight = getAttribute(
      u"data-embedlite-event-viewportHeight").ToDouble(&rv);
    if (NS_FAILED(rv) || !std::isfinite(tab->viewportHeight) ||
        tab->viewportHeight < 0) {
      tab->viewportHeight = 0;
    }
    const nsAutoString presShellId = getAttribute(
      u"data-embedlite-event-presShellId");
    const nsAutoString viewId = getAttribute(
      u"data-embedlite-event-viewId");
    ErrorResult error;
    browser->SetAttribute(u"data-embedlite-pres-shell-id"_ns,
                          presShellId, error);
    error.SuppressException();
    browser->SetAttribute(u"data-embedlite-view-id"_ns, viewId, error);
    error.SuppressException();
    SendContentState(*tab);
  } else if (type.EqualsLiteral("EmbedLiteChromeContentMessage")) {
    nsAutoString name;
    nsAutoString data;
    browser->GetAttribute(u"data-embedlite-event-name"_ns, name);
    browser->GetAttribute(u"data-embedlite-event-data"_ns, data);
    if (!name.IsEmpty()) {
      Unused << SendContentMessageToEmbedder(tab->id, name, data);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnStateChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    uint32_t aStateFlags, nsresult aStatus)
{
  Unused << aRequest;
  Unused << aStatus;
  TabRecord* tab = FindTab(aWebProgress);
  if (!tab || tab->discarded ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_WINDOW)) {
    return NS_OK;
  }

  if (aStateFlags & nsIWebProgressListener::STATE_START) {
    BeginDocumentNavigation(*tab);
    tab->awaitingDocumentLocation = true;
    tab->loading = true;
    tab->progress = 0;
    tab->current = 0;
    tab->total = 0;
    if (!tab->restoring) {
      tab->title.Truncate();
      UpdateLocation(*tab);
    }
    ScheduleTabSnapshot();
  } else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {
    tab->awaitingDocumentLocation = false;
    tab->loading = false;
    tab->progress = 100;
    if (tab->restoring) {
      BrowsingContext* context = BrowsingContextFor(*tab);
      nsCOMPtr<nsIURI> currentURI = context
        ? context->Canonical()->GetCurrentURI() : nullptr;
      nsAutoCString currentLocation;
      if (currentURI) {
        Unused << currentURI->GetSpec(currentLocation);
      }
      if (!currentLocation.IsEmpty() &&
          (!currentLocation.EqualsLiteral("about:blank") ||
           tab->location.Equals(currentLocation))) {
        tab->restoring = false;
        ReplayContentRegistrations(*tab);
        if (tab->location != currentLocation) {
          tab->title.Truncate();
        }
      }
    }
    DispatchContentCommand(*tab, u"request-state"_ns, u"{}"_ns);
    UpdateLocation(*tab);
    UpdateTitle(*tab);
    ScheduleTabSnapshot();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnProgressChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
    int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
  Unused << aRequest;
  Unused << aCurSelfProgress;
  Unused << aMaxSelfProgress;
  TabRecord* tab = FindTab(aWebProgress);
  if (!tab || tab->discarded || aCurTotalProgress < 0 ||
      aMaxTotalProgress <= 0 || aCurTotalProgress > aMaxTotalProgress) {
    return NS_OK;
  }

  const int32_t progress = static_cast<int32_t>(
    (static_cast<int64_t>(aCurTotalProgress) * 100) / aMaxTotalProgress);
  if (tab->progress != progress || tab->current != aCurTotalProgress ||
      tab->total != aMaxTotalProgress) {
    tab->progress = progress;
    tab->current = aCurTotalProgress;
    tab->total = aMaxTotalProgress;
    ScheduleTabSnapshot();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnLocationChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    nsIURI* aLocation, uint32_t aFlags)
{
  Unused << aRequest;
  if (TabRecord* tab = FindTab(aWebProgress)) {
    if (tab->discarded) {
      return NS_OK;
    }
    if (tab->restoring) {
      nsAutoCString restoredLocation;
      if (aLocation) {
        Unused << aLocation->GetSpec(restoredLocation);
      }
      if (restoredLocation.IsEmpty() ||
          (restoredLocation.EqualsLiteral("about:blank") &&
           !tab->location.Equals(restoredLocation))) {
        return NS_OK;
      }
      tab->restoring = false;
      ReplayContentRegistrations(*tab);
      if (tab->location != restoredLocation &&
          !(aFlags &
            nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT)) {
        tab->title.Truncate();
      }
      UpdateLocation(*tab, aLocation);
      ScheduleTabSnapshot();
      DispatchContentCommand(*tab, u"request-state"_ns, u"{}"_ns);
      SendContentState(*tab);
      return NS_OK;
    }
    if (!(aFlags &
          nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT)) {
      if (!tab->awaitingDocumentLocation) {
        BeginDocumentNavigation(*tab);
      }
      tab->awaitingDocumentLocation = false;
      DispatchContentCommand(*tab, u"request-state"_ns, u"{}"_ns);
    }
    if (!(aFlags & nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT)) {
      const bool hadTitle = !tab->title.IsEmpty();
      tab->title.Truncate();
      if (hadTitle) {
        ScheduleTabSnapshot();
      }
    }
    UpdateLocation(*tab, aLocation);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnStatusChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    nsresult aStatus, const char16_t* aMessage)
{
  Unused << aWebProgress;
  Unused << aRequest;
  Unused << aStatus;
  Unused << aMessage;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnSecurityChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, uint32_t aState)
{
  Unused << aRequest;
  TabRecord* tab = FindTab(aWebProgress);
  if (!tab || tab->discarded || tab->awaitingDocumentLocation) {
    return NS_OK;
  }

  RefPtr<BrowsingContext> progressContext;
  if (NS_FAILED(aWebProgress->GetBrowsingContextXPCOM(
        getter_AddRefs(progressContext))) || !progressContext) {
    return NS_OK;
  }
  BrowsingContext* tabContext = BrowsingContextFor(*tab);
  if (!tabContext || progressContext->Canonical() !=
                       tabContext->Top()->Canonical()) {
    return NS_OK;
  }

  nsCString serialized;
  nsCOMPtr<mozIDOMWindowProxy> progressWindow;
  Unused << aWebProgress->GetDOMWindow(getter_AddRefs(progressWindow));
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(progressWindow);
  nsCOMPtr<nsIChannel> channel;
  if (docShell) {
    Unused << docShell->GetCurrentDocumentChannel(
      getter_AddRefs(channel));
  }
  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  if (channel) {
    Unused << channel->GetSecurityInfo(getter_AddRefs(securityInfo));
  }
  if (!securityInfo) {
    WindowGlobalParent* global =
      tabContext->Top()->Canonical()->GetCurrentWindowGlobal();
    securityInfo = global ? global->GetSecurityInfo() : nullptr;
  }
  if (securityInfo) {
    Unused << securityInfo->ToString(serialized);
  }
  if (tab->securityState != aState || tab->securityStatus != serialized) {
    tab->securityState = aState;
    tab->securityStatus = serialized;
    SendContentState(*tab);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnContentBlockingEvent(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, uint32_t aEvent)
{
  Unused << aWebProgress;
  Unused << aRequest;
  Unused << aEvent;
  return NS_OK;
}

} // namespace embedlite
} // namespace mozilla
