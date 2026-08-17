/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_CHROME_SESSION_CHILD_H
#define MOZ_EMBEDLITE_CHROME_SESSION_CHILD_H

#include <map>

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakReference.h"
#include "mozilla/UniquePtr.h"

class nsIAppWindow;
class nsIBrowserDOMWindow;
class nsIContentSecurityPolicy;
class nsIOpenURIInFrameParams;
class nsIOpenWindowInfo;
class nsIPrincipal;
class nsIReferrerInfo;
class nsIURI;
class nsIWebProgress;

namespace mozilla {
class MultiTouchInput;
namespace dom {
class BrowsingContext;
class CanonicalBrowsingContext;
class Element;
class Promise;
} // namespace dom

namespace embedlite {

class EmbedLiteWindowChild;
class EmbedLiteBrowserDOMWindow;
class EmbedLiteChromeTabProgressListener;
class EmbedLiteChromeTabRestoreData;

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
  bool RestoreTabs(const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
                   int32_t aSelectedTabIndex);
  bool NewTab(const nsACString& aURL, uint64_t aPersistentId,
              bool aFromExternal, bool aInBackground);
  bool AssociateTab(uint64_t aTabId, uint64_t aPersistentId);
  bool SelectTab(uint64_t aTabId);
  bool CloseTab(uint64_t aTabId);

private:
  friend class EmbedLiteBrowserDOMWindow;
  friend class EmbedLiteWindowChild;

  struct TabHistoryEntry
  {
    nsCString location;
    nsString title;
  };

  struct TabRecord
  {
    TabRecord(uint64_t aId, uint64_t aPersistentId);

    uint64_t id;
    uint64_t persistentId;
    uint64_t locationRevision;
    RefPtr<dom::Element> browser;
    nsCOMPtr<nsIWebProgress> webProgress;
    RefPtr<EmbedLiteChromeTabProgressListener> progressListener;
    nsCString location;
    nsString title;
    nsTArray<TabHistoryEntry> history;
    uint32_t selectedHistoryIndex;
    bool progressListenerRegistered;
    bool progressRetryPending;
    uint8_t progressRetryAttempts;
    bool loading;
    bool closing;
    bool discarded;
    bool restoring;
    bool canGoBack;
    bool canGoForward;
    int32_t progress;
    int64_t current;
    int64_t total;
  };

  struct PendingBeforeUnloadPrompt
  {
    uint64_t tabId;
    RefPtr<dom::Promise> promise;
  };

  ~EmbedLiteChromeSessionChild();

  nsresult BrowserBecameVisible();
  nsresult InstallBrowserDOMWindow();
  nsresult TryCompleteInitialization();
  void ScheduleInitializationRetry();
  void ScheduleInitializationCompletion(bool aSuccess);
  void CompleteInitialization(bool aSuccess);
  nsresult CreateTab(nsIOpenWindowInfo* aOpenWindowInfo,
                     const nsAString& aName, bool aInBackground,
                     bool aSkipLoad, uint64_t aPersistentId,
                     TabRecord** aTab,
                     dom::Element** aBrowser = nullptr,
                     dom::BrowsingContext** aBrowsingContext = nullptr);
  nsresult CreateBrowserForTab(
    TabRecord& aTab, nsIOpenWindowInfo* aOpenWindowInfo,
    const nsAString& aName, bool aInBackground,
    dom::Element** aBrowser = nullptr,
    dom::BrowsingContext** aBrowsingContext = nullptr);
  nsresult MaterializeTab(TabRecord& aTab);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult RestoreTabHistory(TabRecord& aTab);
  nsresult OpenTabFromBrowserDOMWindow(
    nsIURI* aURI, nsIOpenWindowInfo* aOpenWindowInfo,
    nsIReferrerInfo* aReferrerInfo, nsIPrincipal* aTriggeringPrincipal,
    nsIContentSecurityPolicy* aCsp, const nsAString& aName,
    int16_t aWhere, int32_t aFlags, bool aSkipLoad,
    dom::Element** aBrowser, dom::BrowsingContext** aBrowsingContext);
  bool LoadTab(TabRecord& aTab, const nsACString& aURL,
               bool aFromExternal, nsIReferrerInfo* aReferrerInfo = nullptr,
               nsIPrincipal* aTriggeringPrincipal = nullptr,
               nsIContentSecurityPolicy* aCsp = nullptr);
  nsresult RebindProgressListener(TabRecord& aTab);
  void ScheduleProgressListenerRetry(uint64_t aTabId);
  void FinishProgressListenerRebind(TabRecord& aTab);
  void RemoveProgressListener(TabRecord& aTab);
  void AddBrowserEventListeners(TabRecord& aTab);
  void RemoveBrowserEventListeners(TabRecord& aTab);
  TabRecord* FindTab(uint64_t aTabId) const;
  TabRecord* FindTab(dom::Element* aBrowser) const;
  TabRecord* FindTab(nsIWebProgress* aWebProgress) const;
  TabRecord* FindTab(dom::BrowsingContext* aBrowsingContext) const;
  TabRecord* SelectedTab() const;
  dom::BrowsingContext* BrowsingContextFor(const TabRecord& aTab) const;
  dom::CanonicalBrowsingContext* CurrentBrowsingContext() const;
  bool SelectTab(TabRecord& aTab);
  void ApplyTabActiveState(TabRecord& aTab, bool aSelected);
  void UpdateSiblingState();
  void RemoveTab(uint64_t aTabId);
  bool CanCloseTabs();
  void ApplyActiveState();
  void ApplyFocusState();
  void ScheduleUpdate();
  void UpdateLocation(TabRecord& aTab, nsIURI* aLocation = nullptr);
  void UpdateTitle(TabRecord& aTab);
  void ScheduleTabSnapshot();
  void SendTabSnapshot();
  bool RequestBeforeUnloadPrompt(
    dom::BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    dom::Promise* aPromise);
  void ResolveBeforeUnloadPrompt(uint64_t aRequestId,
                                 uint64_t aTabId,
                                 bool aPermit);
  void CancelBeforeUnloadPrompts(uint64_t aTabId = 0);
  void RemoveObserver();

  EmbedLiteWindowChild* mWindow; // Not owned.
  nsIAppWindow* mAppWindow; // Not owned; mWindow owns it.
  nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;
  RefPtr<dom::Element> mTabContainer;
  nsTArray<mozilla::UniquePtr<TabRecord>> mTabs;
  nsCString mInitialContentURI;
  uint64_t mNextTabId;
  uint64_t mSelectedTabId;
  uint64_t mPendingSelectedTabId;
  uint64_t mTabRevision;
  uint64_t mNextBeforeUnloadPromptId;
  std::map<uint64_t, PendingBeforeUnloadPrompt> mBeforeUnloadPrompts;
  bool mObservingWindowVisible;
  bool mInitializationRetryPending;
  uint8_t mInitializationRetryAttempts;
  bool mInitializationCompletionPending;
  bool mInitializationCompletionSuccess;
  bool mTabSnapshotPending;
  bool mCheckingCanClose;
  bool mRestoreTabsReceived;
  bool mInitializationFinished;
  bool mReady;
  bool mActive;
  bool mFocused;
};

} // namespace embedlite
} // namespace mozilla

#endif
