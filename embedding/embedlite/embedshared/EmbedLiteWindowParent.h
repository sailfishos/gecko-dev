/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_PARENT_H
#define MOZ_WINDOW_EMBED_PARENT_H

#include <map>

#include "mozilla/embedlite/PEmbedLiteWindowParent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WidgetUtils.h"
#include "EmbedLiteChromeSession.h"
#include "EmbedLiteChromeTabSession.h"
#include "EmbedLiteWindow.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteWindow;
class EmbedLiteWindowListener;
class EmbedLiteCompositorBridgeParent;
class EmbedLiteAppThreadParent;

class EmbedLiteWindowParentObserver
{
public:
  virtual void CompositorCreated() = 0;
};

class EmbedLiteWindowParent : public PEmbedLiteWindowParent,
                              public EmbedLiteChromeSession,
                              public EmbedLiteChromeTabSession
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteWindowParent)
public:
  EmbedLiteWindowParent(const uint16_t &width, const uint16_t &height,
                        const uint32_t &id, EmbedLiteWindowListener *aListener,
                        bool aChromeHosted);

  static RefPtr<EmbedLiteWindowParent> From(const uint32_t id);
  static RefPtr<EmbedLiteWindowParent> Current();

  void AddObserver(EmbedLiteWindowParentObserver*);
  void RemoveObserver(EmbedLiteWindowParentObserver*);

  EmbedLiteCompositorBridgeParent* GetCompositor() const { return mCompositor.get(); }

  void SetSize(int width, int height);
  void SetContentOrientation(const uint32_t &);
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

protected:
  friend class EmbedLiteCompositorBridgeParent;
  friend class EmbedLiteWindow;

  virtual ~EmbedLiteWindowParent() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void SetEmbedAPIWindow(EmbedLiteWindow* window);
  void SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor);

private:
  friend class EmbedLiteAppThreadParent;
  friend class PEmbedLiteWindowParent;
  typedef nsTArray<EmbedLiteWindowParentObserver*> ObserverArray;

  static void Register(EmbedLiteWindowParent* aParent);
  static void Unregister(EmbedLiteWindowParent* aParent);

  mozilla::ipc::IPCResult RecvInitialized(const bool &success);
  mozilla::ipc::IPCResult RecvDestroyed();
  mozilla::ipc::IPCResult RecvOnLocationChanged(
    const nsCString& aLocation, const bool& aCanGoBack,
    const bool& aCanGoForward);
  mozilla::ipc::IPCResult RecvOnLoadStarted(const nsCString& aLocation);
  mozilla::ipc::IPCResult RecvOnLoadFinished();
  mozilla::ipc::IPCResult RecvOnLoadProgress(const int32_t& aProgress,
                                             const int64_t& aCurrent,
                                             const int64_t& aTotal);
  mozilla::ipc::IPCResult RecvOnTitleChanged(const nsString& aTitle);
  mozilla::ipc::IPCResult RecvOnTabSnapshot(
    const EmbedLiteChromeSessionData& aSnapshot);
  mozilla::ipc::IPCResult RecvOnBeforeUnloadPrompt(
    const EmbedLiteChromeBeforeUnloadData& aPrompt);

  bool CanSendChromeSessionCommand() const;
  void ReplayChromeSessionState();
  void ReplayTabSnapshot();
  void UpdateSelectedChromeSessionState();

  uint32_t mId;
  EmbedLiteWindowListener *const mListener;
  EmbedLiteWindow* mWindow;
  EmbedLiteChromeSessionListener* mChromeSessionListener;
  EmbedLiteChromeTabSessionListener* mChromeTabSessionListener;
  const bool mChromeHosted;
  bool mInitialized;
  bool mDestroying;
  bool mHasLocation;
  bool mHasLoadStarted;
  bool mHasLoadProgress;
  bool mHasTitle;
  bool mHasTabSnapshot;
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
  EmbedLiteChromeSessionData mTabSnapshot;
  std::map<uint64_t, uint64_t> mPendingBeforeUnloadPrompts;
  EmbedLitePlatformFrameListener* mPlatformFrameListener;
  ObserverArray mObservers;
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;

  gfxSize mSize;
  mozilla::ScreenRotation mRotation;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_PARENT_H
