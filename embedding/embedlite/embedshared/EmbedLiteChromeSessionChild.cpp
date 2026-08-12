/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteChromeSessionChild.h"

#include <cstring>

#include "EmbedLiteWindowChild.h"
#include "nsWindow.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsIAppWindow.h"
#include "nsIBaseWindow.h"
#include "nsIFocusManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsFrameLoaderOwner.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace embedlite {

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(EmbedLiteChromeSessionChild,
                  nsIObserver,
                  nsIDOMEventListener,
                  nsIWebProgressListener,
                  nsISupportsWeakReference)

EmbedLiteChromeSessionChild::EmbedLiteChromeSessionChild(
    EmbedLiteWindowChild* aWindow)
  : mWindow(aWindow)
  , mAppWindow(nullptr)
  , mObservingWindowVisible(false)
  , mProgressListenerRegistered(false)
  , mInitializationRetryPending(false)
  , mInitializationCompletionPending(false)
  , mInitializationFinished(false)
  , mReady(false)
  , mActive(true)
  , mFocused(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWindow);
}

EmbedLiteChromeSessionChild::~EmbedLiteChromeSessionChild()
{
  MOZ_ASSERT(!mWindow);
  MOZ_ASSERT(!mAppWindow);
  MOZ_ASSERT(!mBrowser);
  MOZ_ASSERT(!mWebProgress);
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

void
EmbedLiteChromeSessionChild::RemoveBrowserEventListeners()
{
  if (!mBrowser) {
    return;
  }

  mBrowser->RemoveSystemEventListener(u"DOMTitleChanged"_ns, this, false);
  mBrowser->RemoveSystemEventListener(u"pagetitlechanged"_ns, this, false);
  mBrowser->RemoveSystemEventListener(
    u"DidChangeBrowserRemoteness"_ns, this, false);
  mBrowser->RemoveSystemEventListener(
    u"XULFrameLoaderCreated"_ns, this, false);
}

void
EmbedLiteChromeSessionChild::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  RemoveObserver();
  RemoveBrowserEventListeners();

  if (mWebProgress && mProgressListenerRegistered) {
    Unused << mWebProgress->RemoveProgressListener(this);
  }
  mProgressListenerRegistered = false;
  mWebProgress = nullptr;
  mBrowser = nullptr;
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

  mInitializationRetryPending = true;
  nsresult rv = NS_DispatchToCurrentThread(NS_NewRunnableFunction(
    "EmbedLiteChromeSessionChild::RetryInitialization",
    [self = RefPtr<EmbedLiteChromeSessionChild>(this)]() {
      self->mInitializationRetryPending = false;
      if (!self->mWindow || self->mInitializationFinished || self->mReady) {
        return;
      }
      nsresult retryRv = self->TryCompleteInitialization();
      if (NS_SUCCEEDED(retryRv)) {
        self->ScheduleInitializationCompletion(true);
      } else if (retryRv != NS_ERROR_NOT_AVAILABLE) {
        self->ScheduleInitializationCompletion(false);
      }
    }));
  if (NS_FAILED(rv)) {
    mInitializationRetryPending = false;
  }
}

void
EmbedLiteChromeSessionChild::ScheduleInitializationCompletion(bool aSuccess)
{
  if (!mWindow || mInitializationFinished ||
      mInitializationCompletionPending) {
    return;
  }

  mInitializationCompletionPending = true;
  nsresult rv = NS_DispatchToCurrentThread(NS_NewRunnableFunction(
    "EmbedLiteChromeSessionChild::CompleteInitialization",
    [self = RefPtr<EmbedLiteChromeSessionChild>(this), aSuccess]() {
      self->CompleteInitialization(aSuccess);
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

  mBrowser = document->GetElementById(u"content"_ns);
  NS_ENSURE_TRUE(mBrowser, NS_ERROR_UNEXPECTED);

  nsresult rv = mBrowser->AddSystemEventListener(
    u"DOMTitleChanged"_ns, this, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mBrowser->AddSystemEventListener(
    u"pagetitlechanged"_ns, this, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mBrowser->AddSystemEventListener(
    u"DidChangeBrowserRemoteness"_ns, this, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mBrowser->AddSystemEventListener(
    u"XULFrameLoaderCreated"_ns, this, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return TryCompleteInitialization();
}

nsresult
EmbedLiteChromeSessionChild::TryCompleteInitialization()
{
  if (mReady) {
    return NS_OK;
  }

  nsresult rv = RebindProgressListener();
  NS_ENSURE_SUCCESS(rv, rv);

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
  if (mWindow && mReady && !initialContentURI.IsEmpty()) {
    Unused << LoadURL(initialContentURI, false);
  }
}

CanonicalBrowsingContext*
EmbedLiteChromeSessionChild::CurrentBrowsingContext() const
{
  if (mBrowser) {
    RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(mBrowser);
    if (frameLoaderOwner) {
      if (BrowsingContext* browsingContext =
            frameLoaderOwner->GetBrowsingContext()) {
        return browsingContext->Canonical();
      }
    }
  }

  if (!mAppWindow) {
    return nullptr;
  }

  RefPtr<BrowsingContext> browsingContext;
  if (NS_FAILED(mAppWindow->GetPrimaryContentBrowsingContext(
        getter_AddRefs(browsingContext))) || !browsingContext) {
    return nullptr;
  }
  return browsingContext->Canonical();
}

bool
EmbedLiteChromeSessionChild::IsCurrentWebProgress(
    nsIWebProgress* aWebProgress, bool aAllowNull) const
{
  CanonicalBrowsingContext* current = CurrentBrowsingContext();
  if (!mReady || !mProgressListenerRegistered || !current) {
    return false;
  }
  if (!aWebProgress) {
    return aAllowNull;
  }

  RefPtr<BrowsingContext> progressContext;
  if (NS_FAILED(aWebProgress->GetBrowsingContextXPCOM(
        getter_AddRefs(progressContext))) || !progressContext) {
    return false;
  }
  return progressContext->Canonical() == current;
}

nsresult
EmbedLiteChromeSessionChild::RebindProgressListener()
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIWebProgress> webProgress = browsingContext->GetWebProgress();
  NS_ENSURE_TRUE(webProgress, NS_ERROR_NOT_AVAILABLE);
  if (webProgress == mWebProgress && mProgressListenerRegistered) {
    return NS_OK;
  }

  if (mWebProgress && mProgressListenerRegistered) {
    Unused << mWebProgress->RemoveProgressListener(this);
  }
  mProgressListenerRegistered = false;
  mWebProgress = webProgress;

  const uint32_t notifyMask =
    nsIWebProgress::NOTIFY_STATE_DOCUMENT |
    nsIWebProgress::NOTIFY_LOCATION |
    nsIWebProgress::NOTIFY_PROGRESS;
  nsresult rv = mWebProgress->AddProgressListener(this, notifyMask);
  if (NS_SUCCEEDED(rv)) {
    mProgressListenerRegistered = true;
  }
  return rv;
}

bool
EmbedLiteChromeSessionChild::LoadURL(
    const nsACString& aURL, bool aFromExternal)
{
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext || aURL.IsEmpty()) {
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
  options.mTriggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  options.mLoadFlags = flags;
  ErrorResult error;
  browsingContext->FixupAndLoadURIString(
    NS_ConvertUTF8toUTF16(aURL), options, error);
  return NS_SUCCEEDED(error.StealNSResult());
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
  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
  if (!mReady || !browsingContext) {
    return;
  }

  ErrorResult error;
  browsingContext->SetIsActive(mActive, error);
  if (NS_FAILED(error.StealNSResult())) {
    return;
  }
  if (BrowserParent* browserParent = browsingContext->GetBrowserParent()) {
    browserParent->SetRenderLayers(mActive);
  }
  if (mWindow && mWindow->GetWidget()) {
    mWindow->GetWidget()->SetActive(mActive);
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
  if (!mReady || !mBrowser) {
    return;
  }

  nsCOMPtr<nsIFocusManager> focusManager =
    do_GetService(FOCUSMANAGER_CONTRACTID);
  if (!focusManager) {
    return;
  }

  if (mFocused) {
    Unused << focusManager->SetFocus(
      mBrowser, nsIFocusManager::FLAG_NOSCROLL);
  } else {
    nsPIDOMWindowOuter* window = mBrowser->OwnerDoc()->GetWindow();
    if (window) {
      Unused << focusManager->ClearFocus(window);
    }
  }
}

bool
EmbedLiteChromeSessionChild::SetFocused(bool aFocused)
{
  if (!mReady) {
    return false;
  }
  mFocused = aFocused;
  ApplyFocusState();
  return true;
}

void
EmbedLiteChromeSessionChild::NotifyLocation(nsIURI* aLocation)
{
  if (!mWindow || !mReady) {
    return;
  }

  CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext();
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
  Unused << mWindow->SendOnLocationChanged(
    spec, canGoBack, canGoForward);
}

void
EmbedLiteChromeSessionChild::NotifyTitle()
{
  if (!mWindow || !mReady) {
    return;
  }

  nsAutoString title;
  if (CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext()) {
    if (WindowGlobalParent* windowGlobal =
          browsingContext->GetCurrentWindowGlobal()) {
      windowGlobal->GetDocumentTitle(title);
    } else if (Document* document = browsingContext->GetDocument()) {
      document->GetTitle(title);
    }
  }
  Unused << mWindow->SendOnTitleChanged(title);
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::HandleEvent(Event* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("DidChangeBrowserRemoteness") ||
      type.EqualsLiteral("XULFrameLoaderCreated")) {
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
    if (NS_SUCCEEDED(RebindProgressListener())) {
      ApplyActiveState();
      ApplyFocusState();
      NotifyLocation();
      NotifyTitle();
    }
  } else if (type.EqualsLiteral("DOMTitleChanged") ||
             type.EqualsLiteral("pagetitlechanged")) {
    NotifyTitle();
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
  if (!IsCurrentWebProgress(aWebProgress) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)) {
    return NS_OK;
  }

  if (aStateFlags & nsIWebProgressListener::STATE_START) {
    nsCString location;
    if (CanonicalBrowsingContext* browsingContext = CurrentBrowsingContext()) {
      if (nsCOMPtr<nsIURI> uri = browsingContext->GetCurrentURI()) {
        Unused << uri->GetSpec(location);
      }
    }
    Unused << mWindow->SendOnLoadStarted(location);
  } else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {
    Unused << mWindow->SendOnLoadFinished();
    NotifyLocation();
    NotifyTitle();
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
  if (!IsCurrentWebProgress(aWebProgress, true) ||
      aMaxTotalProgress <= 0 || aCurTotalProgress > aMaxTotalProgress) {
    return NS_OK;
  }

  const int32_t progress = static_cast<int32_t>(
    (static_cast<int64_t>(aCurTotalProgress) * 100) / aMaxTotalProgress);
  Unused << mWindow->SendOnLoadProgress(
    progress, aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteChromeSessionChild::OnLocationChange(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    nsIURI* aLocation, uint32_t aFlags)
{
  Unused << aRequest;
  Unused << aFlags;
  if (IsCurrentWebProgress(aWebProgress)) {
    NotifyLocation(aLocation);
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
  Unused << aWebProgress;
  Unused << aRequest;
  Unused << aState;
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
