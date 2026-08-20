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
#include "EmbedLiteChromeContentRegistrations.h"

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
  bool SendTextEvent(const nsAString& aCommit, const nsAString& aPreEdit,
                     int32_t aReplacementStart,
                     int32_t aReplacementLength);
  bool SendKeyPress(int32_t aDomKeyCode, int32_t aModifiers,
                    int32_t aCharCode);
  bool SendKeyRelease(int32_t aDomKeyCode, int32_t aModifiers,
                      int32_t aCharCode);
  bool ReceiveInputEvent(const MultiTouchInput& aEvent);
  bool RestoreTabs(const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
                   int32_t aSelectedTabIndex);
  bool NewTab(const nsACString& aURL, uint64_t aPersistentId,
              bool aFromExternal, bool aInBackground);
  bool AssociateTab(uint64_t aTabId, uint64_t aPersistentId);
  bool SelectTab(uint64_t aTabId);
  bool CloseTab(uint64_t aTabId);
  bool LoadFrameScript(const nsACString&);
  bool AddMessageListener(const nsACString&);
  bool RemoveMessageListener(const nsACString&);
  bool SendAsyncMessage(uint64_t, const nsAString&, const nsAString&);
  bool SendMouseEvent(uint64_t, uint8_t, int32_t, int32_t, uint64_t,
                      uint32_t, uint32_t, uint32_t, uint32_t);
  bool SendWheelEvent(uint64_t, int32_t, int32_t, uint64_t, double, double,
                      uint32_t, uint32_t);
  bool ScrollTo(uint64_t, int32_t, int32_t);
  bool ScrollBy(uint64_t, int32_t, int32_t);
  bool ZoomToRect(uint64_t, float, float, float, float);
  bool SetDesktopMode(uint64_t, bool);
  bool SetThrottlePainting(uint64_t, bool);
  bool SuspendTimeouts(uint64_t);
  bool ResumeTimeouts(uint64_t);
  bool SetHttpUserAgent(uint64_t, const nsAString&);
  bool SetMargins(uint64_t, int32_t, int32_t, int32_t, int32_t);
  bool SetSafeAreaInsets(uint64_t, int32_t, int32_t, int32_t, int32_t);
  bool SetDynamicToolbarHeight(uint64_t, int32_t);
  bool SendContentMessageFromAppService(uint64_t, const nsAString&,
                                        const nsAString&);
  bool SendContentMessageToEmbedder(uint64_t, const nsAString&,
                                    const nsAString&);
  dom::BrowsingContext* BrowsingContextForTab(uint64_t) const;
  dom::Element* BrowserForTab(uint64_t) const;
  uint32_t SelectedEndpointId() const;

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
    uint32_t endpointId;
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
    bool awaitingDocumentLocation;
    bool canGoBack;
    bool canGoForward;
    int32_t progress;
    int64_t current;
    int64_t total;
    nsCString securityStatus;
    uint32_t securityState;
    nsString httpUserAgent;
    int32_t dynamicToolbarHeight;
    bool hasHttpUserAgent;
    bool hasDynamicToolbarHeight;
    bool timeoutsSuspended;
    bool throttlePainting;
    bool fullscreen;
    bool firstPaint;
    int32_t firstPaintX;
    int32_t firstPaintY;
    uint32_t scrollWidth;
    uint32_t scrollHeight;
    int32_t scrollX;
    int32_t scrollY;
    double viewportX;
    double viewportY;
    double viewportWidth;
    double viewportHeight;
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
  bool DispatchContentCommand(TabRecord&, const nsAString&,
                              const nsAString& = EmptyString());
  void ReplayContentRegistrations(TabRecord&);
  void BeginDocumentNavigation(TabRecord&);
  void SendContentState(TabRecord&);
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
  void SendTabCloseResult(uint64_t aTabId, bool aClosed);
  bool RemoveTab(uint64_t aTabId);
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
  EmbedLiteChromeContentRegistrations mContentRegistrations;
  nsCString mInitialContentURI;
  uint64_t mNextTabId;
  uint64_t mSelectedTabId;
  uint64_t mPendingSelectedTabId;
  uint64_t mTabRevision;
  uint64_t mContentRevision;
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
  bool mShuttingDown;
  bool mReady;
  bool mActive;
  bool mFocused;
};

} // namespace embedlite
} // namespace mozilla

#endif
