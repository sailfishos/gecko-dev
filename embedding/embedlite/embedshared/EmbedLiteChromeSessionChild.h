/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_CHROME_SESSION_CHILD_H
#define MOZ_EMBEDLITE_CHROME_SESSION_CHILD_H

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIAppWindow;
class nsIURI;
class nsIWebProgress;

namespace mozilla {
class MultiTouchInput;
namespace dom {
class CanonicalBrowsingContext;
class Element;
} // namespace dom

namespace embedlite {

class EmbedLiteWindowChild;

class EmbedLiteChromeSessionChild final : public nsIObserver,
                                          public nsIDOMEventListener,
                                          public nsIWebProgressListener,
                                          public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIWEBPROGRESSLISTENER

  explicit EmbedLiteChromeSessionChild(EmbedLiteWindowChild* aWindow);

  nsresult Start(nsIAppWindow* aAppWindow,
                 const nsACString& aInitialContentURI);
  void Shutdown();

  bool LoadURL(const nsACString& aURL, bool aFromExternal);
  bool GoBack(bool aRequireUserInteraction, bool aUserActivation);
  bool GoForward(bool aRequireUserInteraction, bool aUserActivation);
  bool StopLoad();
  bool Reload(bool aHardReload);
  bool SetActive(bool aActive);
  bool SetFocused(bool aFocused);
  bool ReceiveInputEvent(const MultiTouchInput& aEvent);

private:
  ~EmbedLiteChromeSessionChild();

  nsresult BrowserBecameVisible();
  nsresult TryCompleteInitialization();
  void ScheduleInitializationRetry();
  void ScheduleInitializationCompletion(bool aSuccess);
  void CompleteInitialization(bool aSuccess);
  nsresult RebindProgressListener();
  dom::CanonicalBrowsingContext* CurrentBrowsingContext() const;
  bool IsCurrentWebProgress(nsIWebProgress* aWebProgress,
                            bool aAllowNull = false) const;
  void ApplyActiveState();
  void ApplyFocusState();
  void ScheduleUpdate();
  void NotifyLocation(nsIURI* aLocation = nullptr);
  void NotifyTitle();
  void RemoveObserver();
  void RemoveBrowserEventListeners();

  EmbedLiteWindowChild* mWindow; // Not owned.
  nsIAppWindow* mAppWindow; // Not owned; mWindow owns it.
  RefPtr<dom::Element> mBrowser;
  nsCOMPtr<nsIWebProgress> mWebProgress;
  nsCString mInitialContentURI;
  bool mObservingWindowVisible;
  bool mProgressListenerRegistered;
  bool mInitializationRetryPending;
  bool mInitializationCompletionPending;
  bool mInitializationFinished;
  bool mReady;
  bool mActive;
  bool mFocused;
};

} // namespace embedlite
} // namespace mozilla

#endif
