/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_PARENT_H
#define MOZ_WINDOW_EMBED_PARENT_H

#include <map>
#include <set>

#include "mozilla/embedlite/EmbedLiteChromeTypes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WidgetUtils.h"
#include "EmbedLiteChromeInputSession.h"
#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteChromeSession.h"
#include "EmbedLiteChromeTabSession.h"
#include "EmbedLiteChromeContentRegistrations.h"
#include "EmbedLiteWindow.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteWindow;
class EmbedLiteWindowListener;
class EmbedLiteCompositorBridgeParent;
class EmbedLiteHostedWindow;

class EmbedLiteWindowParentObserver
{
public:
  virtual void CompositorCreated() = 0;
};

class EmbedLiteWindowParent : public EmbedLiteChromeSession,
                              public EmbedLiteChromeTabSession,
                              public EmbedLiteChromeContentSession,
                              public EmbedLiteChromeInputSession
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteWindowParent)
public:
  EmbedLiteWindowParent(uint16_t width, uint16_t height,
                        uint32_t id, EmbedLiteWindowListener *aListener,
                        const nsACString& aInitialContentURI,
                        bool aPrivateBrowsing);

  static RefPtr<EmbedLiteWindowParent> From(const uint32_t id);
  static RefPtr<EmbedLiteWindowParent> Current();

  void AddObserver(EmbedLiteWindowParentObserver*);
  void RemoveObserver(EmbedLiteWindowParentObserver*);

  EmbedLiteCompositorBridgeParent* GetCompositor() const { return mCompositor.get(); }

  void Initialize();
  void Destroy();

  void SetSize(int width, int height);
  void SetContentOrientation(uint32_t );
  bool ScheduleUpdate();
  void SuspendRendering();
  void ResumeRendering();
  bool WithPlatformImage(const PlatformImageCallback& callback);
  void ClearPlatformImage();
  bool AcquirePlatformFrame(const PlatformFrameToken& token,
                            const PlatformFrameCallback& callback);
  bool ReleasePlatformFrame(const PlatformFrameRelease& release);
  bool SetPlatformFrameDeliveryEnabled(bool enabled);
  bool SetPlatformFrameListener(EmbedLitePlatformFrameListener* listener);
  EmbedLiteWindowListener *GetListener() const { return mListener; }

  // EmbedLiteChromeSession:
  void SetListener(EmbedLiteChromeSessionListener* aListener) override;
  bool LoadURL(const char* aURL, bool aFromExternal) override;
  bool GoBack(bool aRequireUserInteraction, bool aUserActivation) override;
  bool GoForward(bool aRequireUserInteraction, bool aUserActivation) override;
  bool StopLoad() override;
  bool Reload(bool aHardReload) override;
  bool SetActive(bool aActive) override;
  bool SetFocused(bool aFocused) override;
  bool ReceiveInputEvent(const EmbedTouchInput& aEvent) override;
  // EmbedLiteChromeInputSession:
  void SetInputListener(
    EmbedLiteChromeInputSessionListener* aListener) override;
  bool SendTextEvent(const char* aCommit, const char* aPreEdit,
                     int32_t aReplacementStart,
                     int32_t aReplacementLength) override;
  bool SendTextEventAtOffset(const char* aCommit, const char* aPreEdit,
                             uint32_t aReplacementOffset,
                             int32_t aReplacementLength) override;
  bool SendKeyPress(int32_t aDomKeyCode, int32_t aModifiers,
                    int32_t aCharCode) override;
  bool SendKeyRelease(int32_t aDomKeyCode, int32_t aModifiers,
                      int32_t aCharCode) override;
  // EmbedLiteChromeTabSession:
  void SetTabListener(
    EmbedLiteChromeTabSessionListener* aListener) override;
  bool RestoreTabs(const EmbedLiteChromeRestoredTab* aTabs,
                   uint32_t aTabCount,
                   int32_t aSelectedTabIndex) override;
  bool NewTab(const char* aURL, uint64_t aPersistentId,
              bool aFromExternal, bool aInBackground) override;
  bool AssociateTab(uint64_t aTabId,
                    uint64_t aPersistentId) override;
  bool SelectTab(uint64_t aTabId) override;
  bool CloseTab(uint64_t aTabId) override;
  bool ResolveBeforeUnloadPrompt(uint64_t aRequestId,
                                 uint64_t aTabId,
                                 bool aPermit) override;
  // EmbedLiteChromeContentSession:
  void SetContentListener(
    EmbedLiteChromeContentSessionListener* aListener) override;
  bool LoadFrameScript(const char*) override;
  bool AddMessageListener(const char*) override;
  bool RemoveMessageListener(const char*) override;
  bool SendAsyncMessage(uint64_t, const char16_t*,
                        const char16_t*) override;
  bool SendMouseEvent(uint64_t, EmbedLiteChromeMouseType, int32_t, int32_t,
                      uint64_t, uint32_t, uint32_t, uint32_t,
                      uint32_t) override;
  bool SendWheelEvent(uint64_t, int32_t, int32_t, uint64_t, double, double,
                      uint32_t, uint32_t) override;
  bool ScrollTo(uint64_t, int32_t, int32_t) override;
  bool ScrollBy(uint64_t, int32_t, int32_t) override;
  bool ZoomToRect(uint64_t, float, float, float, float) override;
  bool SetDesktopMode(uint64_t, bool) override;
  bool SetJavascriptEnabled(bool) override;
  bool SetThrottlePainting(uint64_t, bool) override;
  bool SuspendTimeouts(uint64_t) override;
  bool ResumeTimeouts(uint64_t) override;
  bool SetHttpUserAgent(uint64_t, const char16_t*) override;
  bool SetMargins(uint64_t, int32_t, int32_t, int32_t, int32_t) override;
  bool SetSafeAreaInsets(uint64_t, int32_t, int32_t, int32_t,
                         int32_t) override;
  bool SetDynamicToolbarHeight(uint64_t, int32_t) override;
  bool SetScreenProperties(int32_t, float, float) override;

  void OnInitialized(bool aSuccess);
  void OnDestroyed();
  bool OnTabSnapshot(const EmbedLiteChromeSessionData& aSnapshot);
  bool OnBeforeUnloadPrompt(
    const EmbedLiteChromeBeforeUnloadData& aPrompt);
  bool OnTabCloseResult(uint64_t aTabId, bool aClosed);
  bool OnContentStateChanged(
    const EmbedLiteChromeContentStateData& aState);
  bool OnContentAsyncMessage(
    uint64_t aTabId, uint64_t aPersistentId,
    uint64_t aLocationRevision, const nsAString& aName,
    const nsAString& aJSON);
  bool OnContentWindowCloseRequested(uint64_t aTabId,
                                     uint64_t aPersistentId);
  void OnInputContextChanged(
    int32_t aEnabled, int32_t aOpen, const nsAString& aInputType,
    const nsAString& aInputMode, const nsAString& aActionHint,
    int32_t aCause, int32_t aFocusChange);

protected:
  friend class EmbedLiteCompositorBridgeParent;
  friend class EmbedLiteWindow;

  virtual ~EmbedLiteWindowParent() override;

  void SetEmbedAPIWindow(EmbedLiteWindow* window);
  void SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor);

private:
  friend class EmbedLiteApp;
  friend class EmbedLiteHostedWindow;
  typedef nsTArray<EmbedLiteWindowParentObserver*> ObserverArray;

  static void Register(EmbedLiteWindowParent* aParent);
  static void Unregister(EmbedLiteWindowParent* aParent);

  struct GeometryState {
    gfxSize size;
    mozilla::ScreenRotation rotation;
  };

  gfxSize GetSurfaceSize();

  bool OnLocationChanged(
    const nsCString& aLocation, bool aCanGoBack,
    bool aCanGoForward);
  bool OnLoadStarted(const nsCString& aLocation);
  bool OnLoadFinished();
  bool OnLoadProgress(int32_t aProgress, int64_t aCurrent,
                      int64_t aTotal);
  bool OnTitleChanged(const nsString& aTitle);
  void NotifySessionsDestroyed();

  bool CanSendChromeSessionCommand() const;
  bool CanTargetContentTab(uint64_t aTabId) const;
  void ReplayChromeSessionState();
  void ReplayChromeInputContext();
  void ReplayTabSnapshot();
  void UpdateSelectedChromeSessionState();

  uint32_t mId;
  EmbedLiteWindowListener *const mListener;
  EmbedLiteWindow* mWindow;
  EmbedLiteChromeSessionListener* mChromeSessionListener;
  EmbedLiteChromeTabSessionListener* mChromeTabSessionListener;
  EmbedLiteChromeContentSessionListener* mChromeContentSessionListener;
  EmbedLiteChromeInputSessionListener* mChromeInputSessionListener;
  bool mInitialized;
  bool mDestroying;
  bool mHasLocation;
  bool mHasLoadStarted;
  bool mHasLoadProgress;
  bool mHasTitle;
  bool mHasTabSnapshot;
  bool mHasInputContext;
  bool mRestoreTabsSent;
  uint64_t mProjectedTabId;
  nsCString mLocation;
  bool mCanGoBack;
  bool mCanGoForward;
  nsCString mLoadStartedLocation;
  int32_t mLoadProgress;
  int64_t mLoadCurrent;
  int64_t mLoadTotal;
  nsString mTitle;
  int32_t mInputEnabled;
  int32_t mInputOpen;
  nsString mInputType;
  nsString mInputMode;
  nsString mActionHint;
  int32_t mInputCause;
  int32_t mInputFocusChange;
  EmbedLiteChromeSessionData mTabSnapshot;
  EmbedLiteChromeContentStateData mContentState;
  bool mHasContentState;
  EmbedLiteChromeContentRegistrations mContentRegistrations;
  std::map<uint64_t, uint64_t> mPendingBeforeUnloadPrompts;
  std::set<uint64_t> mPendingTabCloses;
  EmbedLitePlatformFrameListener* mPlatformFrameListener;
  ObserverArray mObservers;
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;
  RefPtr<EmbedLiteHostedWindow> mHostedWindow;
  DataMutex<GeometryState> mGeometry;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_PARENT_H
