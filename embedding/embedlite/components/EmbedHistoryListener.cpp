/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedHistoryListener.h"
#include "nsIURI.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/Link.h"
#include "nsIEmbedLiteJSON.h"
#include "nsIObserverService.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIURI.h"

#include "mozilla/Services.h"

#define NS_LINK_VISITED_EVENT_TOPIC "link-visited"

// We copy Places here.
// Note that we don't yet observe this pref at runtime.
#define PREF_HISTORY_ENABLED "places.history.enabled"

// Time we wait to see if a pending visit is really a redirect
#define PENDING_REDIRECT_TIMEOUT 3000

using namespace mozilla;
using mozilla::dom::Link;

NS_IMPL_ISUPPORTS(EmbedHistoryListener, IHistory, nsIRunnable, nsITimerCallback)

EmbedHistoryListener* EmbedHistoryListener::sHistory = nullptr;

// Ease up porting from nsAndroidHistory.
namespace AndroidBridge {
  bool
  HasEnv() {
    // Disable for now. This not currently in use and causes crashing. JB#39908
    return false;
  }
}

namespace embed {
  namespace GeckoAppShell {
    void
    MarkURIVisited(const nsAString &aUri)
    {
      nsString message;
      // Just simple property bag support still
      nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
      nsCOMPtr<nsIWritablePropertyBag2> root;
      json->CreateObject(getter_AddRefs(root));
      root->SetPropertyAsACString(NS_LITERAL_STRING("msg"), NS_LITERAL_CSTRING("markvisited"));
      root->SetPropertyAsAString(NS_LITERAL_STRING("uri"), aUri);

      json->CreateJSON(root, message);
      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      if (observerService) {
        observerService->NotifyObservers(nullptr, "em:history", message.get());
      }
    }

    void
    SetURITitle(const nsAString &aUri, const nsAString& aTitle)
    {
      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      nsString message;
      // Just simple property bag support still
      nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
      nsCOMPtr<nsIWritablePropertyBag2> root;
      json->CreateObject(getter_AddRefs(root));
      root->SetPropertyAsACString(NS_LITERAL_STRING("msg"), NS_LITERAL_CSTRING("settitle"));
      root->SetPropertyAsAString(NS_LITERAL_STRING("uri"), aUri);
      root->SetPropertyAsAString(NS_LITERAL_STRING("title"), aTitle);

      json->CreateJSON(root, message);
      if (observerService) {
        observerService->NotifyObservers(nullptr, "em:history", message.get());
      }
    }

    void
    CheckURIVisited(const nsAString &aUri)
    {
      nsString message;
      // Just simple property bag support still
      nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
      nsCOMPtr<nsIWritablePropertyBag2> root;
      json->CreateObject(getter_AddRefs(root));
      root->SetPropertyAsACString(NS_LITERAL_STRING("msg"), NS_LITERAL_CSTRING("checkvisited"));
      root->SetPropertyAsAString(NS_LITERAL_STRING("uri"), aUri);

      json->CreateJSON(root, message);
      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      // Possible we can avoid json stuff for this case and send uri directly
      if (observerService) {
        observerService->NotifyObservers(nullptr, "em:history", message.get());
      }
    }
  }
}

/*static*/
already_AddRefed<EmbedHistoryListener> EmbedHistoryListener::GetSingleton()
{
  if (!sHistory) {
    auto historyListener = MakeRefPtr<EmbedHistoryListener>();
    sHistory = historyListener.get();
    return historyListener.forget();
  }

  return do_AddRef(sHistory);
}

EmbedHistoryListener::EmbedHistoryListener()
  : mHistoryEnabled(true)
{
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    (void)observerService->AddObserver(this,
                                       "history:notifyVisited", false);
  }

  LoadPrefs();

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
}

void
EmbedHistoryListener::RegisterVisitedCallback(nsIURI *aURI, Link *aContent)
{
  if (!aContent || !aURI)
    return;

  // Silently return if URI is something we would never add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, );
  if (!canAdd) {
    return;
  }

  nsAutoCString uri;
  rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) return;
  NS_ConvertUTF8toUTF16 uriString(uri);

  nsTArray<Link*>* list = mListeners.Get(uriString);
  if (! list) {
    list = new nsTArray<Link*>();
    mListeners.Put(uriString, list);
  }
  list->AppendElement(aContent);

  if (AndroidBridge::HasEnv()) {
    // widget::GeckoAppShell::CheckURIVisited
    embed::GeckoAppShell::CheckURIVisited(uriString);
  }
  return;
}

void
EmbedHistoryListener::UnregisterVisitedCallback(nsIURI *aURI, Link *aContent)
{
  if (!aContent || !aURI)
    return;

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) return;
  NS_ConvertUTF8toUTF16 uriString(uri);

  nsTArray<Link*>* list = mListeners.Get(uriString);
  if (! list)
    return;

  list->RemoveElement(aContent);
  if (list->IsEmpty()) {
    mListeners.Remove(uriString);
    delete list;
  }
  return;
}

nsIEmbedAppService *EmbedHistoryListener::GetService()
{
  if (!mService) {
    mService = do_GetService("@mozilla.org/embedlite-app-service;1");
  }
  return mService.get();
}

void
EmbedHistoryListener::AppendToRecentlyVisitedURIs(nsIURI* aURI) {
  if (mRecentlyVisitedURIs.Length() < RECENTLY_VISITED_URI_SIZE) {
    // Append a new element while the array is not full.
    mRecentlyVisitedURIs.AppendElement(aURI);
  } else {
    // Otherwise, replace the oldest member.
    mRecentlyVisitedURIsNextIndex %= RECENTLY_VISITED_URI_SIZE;
    mRecentlyVisitedURIs.ElementAt(mRecentlyVisitedURIsNextIndex) = aURI;
    mRecentlyVisitedURIsNextIndex++;
  }
}

bool
EmbedHistoryListener::ShouldRecordHistory()
{
  return mHistoryEnabled;
}

void
EmbedHistoryListener::LoadPrefs()
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv)) {
    prefs->GetBoolPref(PREF_HISTORY_ENABLED, &mHistoryEnabled);
  }
}

inline bool
EmbedHistoryListener::IsRecentlyVisitedURI(nsIURI* aURI) {
  bool equals = false;
  RecentlyVisitedArray::index_type i;
  RecentlyVisitedArray::size_type length = mRecentlyVisitedURIs.Length();
  for (i = 0; i < length && !equals; ++i) {
    aURI->Equals(mRecentlyVisitedURIs.ElementAt(i), &equals);
  }
  return equals;
}

void
EmbedHistoryListener::AppendToEmbedURIs(nsIURI* aURI) {
  if (mEmbedURIs.Length() < EMBED_URI_SIZE) {
    // Append a new element while the array is not full.
    mEmbedURIs.AppendElement(aURI);
  } else {
    // Otherwise, replace the oldest member.
    mEmbedURIsNextIndex %= EMBED_URI_SIZE;
    mEmbedURIs.ElementAt(mEmbedURIsNextIndex) = aURI;
    mEmbedURIsNextIndex++;
  }
}

inline bool
EmbedHistoryListener::IsEmbedURI(nsIURI* aURI) {
  bool equals = false;
  EmbedArray::index_type i;
  EmbedArray::size_type length = mEmbedURIs.Length();
  for (i = 0; i < length && !equals; ++i) {
    aURI->Equals(mEmbedURIs.ElementAt(i), &equals);
  }
  return equals;
}

inline bool
EmbedHistoryListener::RemovePendingVisitURI(nsIURI* aURI) {
  // Remove the first pending URI that matches. Return a boolean to
  // let the caller know if we removed a URI or not.
  bool equals = false;
  PendingVisitArray::index_type i;
  for (i = 0; i < mPendingVisitURIs.Length(); ++i) {
    aURI->Equals(mPendingVisitURIs.ElementAt(i), &equals);
    if (equals) {
      mPendingVisitURIs.RemoveElementAt(i);
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
EmbedHistoryListener::Notify(nsITimer *timer)
{
  // Any pending visits left in the queue have exceeded our threshold for
  // redirects, so save them
  PendingVisitArray::index_type i;
  for (i = 0; i < mPendingVisitURIs.Length(); ++i) {
    SaveVisitURI(mPendingVisitURIs.ElementAt(i));
  }
  mPendingVisitURIs.Clear();

  return NS_OK;
}

void
EmbedHistoryListener::SaveVisitURI(nsIURI* aURI) {
  // Add the URI to our cache so we can take a fast path later
  AppendToRecentlyVisitedURIs(aURI);

  if (AndroidBridge::HasEnv()) {
    // Save this URI in our history
    nsAutoCString spec;
    (void)aURI->GetSpec(spec);
    // widget::GeckoAppShell::MarkURIVisited(NS_ConvertUTF8toUTF16(spec));
    embed::GeckoAppShell::MarkURIVisited(NS_ConvertUTF8toUTF16(spec));
  }

  // Finally, notify that we've been visited.
  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nullptr);
  }
}


NS_IMETHODIMP
EmbedHistoryListener::VisitURI(nsIWidget *aWidget, nsIURI *aURI, nsIURI *aLastVisitedURI, uint32_t aFlags)
{
  if (!aURI) {
    return NS_OK;
  }

  if (!(aFlags & VisitFlags::TOP_LEVEL)) {
    return NS_OK;
  }

  if (aFlags & VisitFlags::UNRECOVERABLE_ERROR) {
    return NS_OK;
  }

  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  if (aLastVisitedURI) {
    if (aFlags & VisitFlags::REDIRECT_SOURCE ||
        aFlags & VisitFlags::REDIRECT_PERMANENT ||
        aFlags & VisitFlags::REDIRECT_TEMPORARY) {
      // aLastVisitedURI redirected to aURI. We want to ignore aLastVisitedURI,
      // so remove the pending visit. We want to give aURI a chance to be saved,
      // so don't return early.
      RemovePendingVisitURI(aLastVisitedURI);
    }

    bool same;
    rv = aURI->Equals(aLastVisitedURI, &same);
    NS_ENSURE_SUCCESS(rv, rv);
    if (same && IsRecentlyVisitedURI(aURI)) {
      // Do not save refresh visits if we have visited this URI recently.
      return NS_OK;
    }

    // Since we have a last visited URI and we were not redirected, it is
    // safe to save the visit if it's still pending.
    if (RemovePendingVisitURI(aLastVisitedURI)) {
      SaveVisitURI(aLastVisitedURI);
    }
  }

  // Let's wait and see if this visit is not a redirect.
  mPendingVisitURIs.AppendElement(aURI);
  mTimer->InitWithCallback(this, PENDING_REDIRECT_TIMEOUT, nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}

NS_IMETHODIMP
EmbedHistoryListener::SetURITitle(nsIURI *aURI, const nsAString& aTitle)
{
  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  if (IsEmbedURI(aURI)) {
    return NS_OK;
  }

  if (AndroidBridge::HasEnv()) {
    nsAutoCString uri;
    nsresult rv = aURI->GetSpec(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUTF16 uriString(uri);
    // widget::GeckoAppShell::SetURITitle(uriString, aTitle);
    embed::GeckoAppShell::SetURITitle(uriString, aTitle);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedHistoryListener::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const char16_t *aData)
{
  if (!strcmp(aTopic, "history:notifyVisited")) {
    sHistory->mPendingLinkURIs.Push(nsDependentString(aData));
    NS_DispatchToMainThread(sHistory);
  }
  return NS_OK;
}

void
EmbedHistoryListener::NotifyVisited(nsIURI *aURI, VisitedStatus aStatus)
{
  if (aURI && sHistory) {
    nsAutoCString spec;
    (void)aURI->GetSpec(spec);
    sHistory->mPendingLinkURIs.Push(NS_ConvertUTF8toUTF16(spec));
    NS_DispatchToMainThread(sHistory);
  }
}

NS_IMETHODIMP
EmbedHistoryListener::Run()
{
  while (! mPendingLinkURIs.IsEmpty()) {
    nsString uriString = mPendingLinkURIs.Pop();
    nsTArray<Link*>* list = sHistory->mListeners.Get(uriString);
    if (list) {
      for (unsigned int i = 0; i < list->Length(); i++) {
        list->ElementAt(i)->VisitedQueryFinished(true);
      }
      // as per the IHistory interface contract, remove the
      // Link pointers once they have been notified
      mListeners.Remove(uriString);
      delete list;
    }
  }
  return NS_OK;
}


// Filter out unwanted URIs such as "chrome:", "mailbox:", etc.
//
// The model is if we don't know differently then add which basically means
// we are suppose to try all the things we know not to allow in and then if
// we don't bail go on and allow it in.
//
// Logic ported from nsNavHistory::CanAddURI.

NS_IMETHODIMP
EmbedHistoryListener::CanAddURI(nsIURI* aURI, bool* canAdd)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(canAdd);

  // See if we're disabled.
  if (!ShouldRecordHistory()) {
    *canAdd = false;
    return NS_OK;
  }

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // first check the most common cases (HTTP, HTTPS) to allow in to avoid most
  // of the work
  if (scheme.EqualsLiteral("http")) {
    *canAdd = true;
    return NS_OK;
  }
  if (scheme.EqualsLiteral("https")) {
    *canAdd = true;
    return NS_OK;
  }

  // now check for all bad things
  if (scheme.EqualsLiteral("about") ||
      scheme.EqualsLiteral("imap") ||
      scheme.EqualsLiteral("news") ||
      scheme.EqualsLiteral("mailbox") ||
      scheme.EqualsLiteral("moz-anno") ||
      scheme.EqualsLiteral("view-source") ||
      scheme.EqualsLiteral("chrome") ||
      scheme.EqualsLiteral("resource") ||
      scheme.EqualsLiteral("data") ||
      scheme.EqualsLiteral("wyciwyg") ||
      scheme.EqualsLiteral("javascript") ||
      scheme.EqualsLiteral("blob")) {
    *canAdd = false;
    return NS_OK;
  }
  *canAdd = true;
  return NS_OK;
}
