/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_HOSTED_WINDOW_H
#define MOZ_EMBEDLITE_HOSTED_WINDOW_H

#include "mozilla/embedlite/EmbedLiteChromeTypes.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsTArray.h"
#include "base/task.h"

class nsIAppWindow;

namespace mozilla {
class MultiTouchInput;
namespace dom {
class BrowsingContext;
class Promise;
}

namespace embedlite {

class nsWindow;
class EmbedLiteChromeSessionChild;
class EmbedLiteWindowListener;
class EmbedLiteWindowParent;

class EmbedLiteHostedWindow final
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteHostedWindow)

public:
  EmbedLiteHostedWindow(uint16_t aWidth, uint16_t aHeight, uint32_t aId,
                        EmbedLiteWindowParent* aOwner,
                        const nsACString& aInitialContentURI,
                        bool aPrivateBrowsing);

  static EmbedLiteHostedWindow* From(uint32_t aId);
  static bool RequestChromeTabBeforeUnloadPrompt(
    dom::BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    dom::Promise* aPromise);

  uint32_t GetUniqueID() const { return mId; }
  nsWindow* GetWidget() const;
  EmbedLiteWindowListener* GetListener() const;
  LayoutDeviceIntRect GetSize() const { return mBounds; }
  void SetScreenProperties(int aDepth, float aDensity, float aDpi);
  float GetDPI() const { return mDpi; }
  float GetDensity() const { return mDensity > 0.0f ? mDensity : 1.0f; }

private:
  friend class EmbedLiteChromeSessionChild;
  friend class EmbedLiteWindowParent;
  friend class nsWindow;

  ~EmbedLiteHostedWindow();

  void CreateWidget();
  bool CreateChromeAppWindow();
  void DestroyChromeAppWindow();
  void ChromeSessionInitializationFinished(bool aSuccess);
  void ChromeInputContextChanged(const widget::InputContext& aContext,
                                 const widget::InputContextAction& aAction);
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

  bool Destroy();
  bool SetSize(const gfxSize& aSize);
  bool SetContentOrientation(uint32_t aRotation);
  bool LoadURL(const nsACString& aURL, bool aFromExternal);
  bool GoBack(bool aRequireUserInteraction, bool aUserActivation);
  bool GoForward(bool aRequireUserInteraction, bool aUserActivation);
  bool StopLoad();
  bool Reload(bool aHardReload);
  bool RestoreTabs(const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
                   int32_t aSelectedTabIndex);
  bool NewTab(const nsACString& aURL, uint64_t aPersistentId,
              bool aFromExternal, bool aInBackground);
  bool AssociateTab(uint64_t aTabId, uint64_t aPersistentId);
  bool SelectTab(uint64_t aTabId);
  bool CloseTab(uint64_t aTabId);
  bool ResolveBeforeUnloadPrompt(uint64_t aRequestId, uint64_t aTabId,
                                 bool aPermit);
  bool LoadContentFrameScript(const nsACString& aURI);
  bool AddContentMessageListener(const nsACString& aName);
  bool RemoveContentMessageListener(const nsACString& aName);
  bool SendContentAsyncMessage(uint64_t aTabId, const nsAString& aName,
                               const nsAString& aJSON);
  bool SendContentMouseEvent(uint64_t aTabId, uint8_t aType, int32_t aX,
                             int32_t aY, uint64_t aTime, uint32_t aButton,
                             uint32_t aButtons, uint32_t aModifiers,
                             uint32_t aClickCount);
  bool SendContentWheelEvent(uint64_t aTabId, int32_t aX, int32_t aY,
                             uint64_t aTime, double aDeltaX, double aDeltaY,
                             uint32_t aDeltaMode, uint32_t aModifiers);
  bool ContentScrollTo(uint64_t aTabId, int32_t aX, int32_t aY);
  bool ContentScrollBy(uint64_t aTabId, int32_t aX, int32_t aY);
  bool ContentZoomToRect(uint64_t aTabId, float aX, float aY,
                         float aWidth, float aHeight);
  bool SetContentDesktopMode(uint64_t aTabId, bool aValue);
  bool SetContentJavascriptEnabled(bool aEnabled);
  bool SetContentThrottlePainting(uint64_t aTabId, bool aValue);
  bool SuspendContentTimeouts(uint64_t aTabId);
  bool ResumeContentTimeouts(uint64_t aTabId);
  bool SetContentHttpUserAgent(uint64_t aTabId, const nsAString& aValue);
  bool SetContentMargins(uint64_t aTabId, int32_t aTop, int32_t aRight,
                         int32_t aBottom, int32_t aLeft);
  bool SetContentSafeAreaInsets(uint64_t aTabId, int32_t aTop,
                                int32_t aRight, int32_t aBottom,
                                int32_t aLeft);
  bool SetContentDynamicToolbarHeight(uint64_t aTabId, int32_t aHeight);
  bool SetContentScreenProperties(int32_t aDepth, float aDensity,
                                  float aDpi);
  bool SetActive(bool aActive);
  bool SetFocused(bool aFocused);
  bool HandleTextEvent(const nsACString& aCommit,
                       const nsACString& aPreEdit,
                       int32_t aReplacementStart,
                       int32_t aReplacementLength);
  bool HandleTextEventAtOffset(const nsACString& aCommit,
                               const nsACString& aPreEdit,
                               uint32_t aReplacementOffset,
                               int32_t aReplacementLength);
  bool HandleKeyPressEvent(int32_t aDomKeyCode, int32_t aModifiers,
                           int32_t aCharCode);
  bool HandleKeyReleaseEvent(int32_t aDomKeyCode, int32_t aModifiers,
                             int32_t aCharCode);
  bool ReceiveInputEvent(const MultiTouchInput& aEvent);
  void RefreshScreen();

  uint32_t mId;
  EmbedLiteWindowParent* mOwner;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIAppWindow> mChromeWindow;
  RefPtr<EmbedLiteChromeSessionChild> mChromeSession;
  LayoutDeviceIntRect mBounds;
  mozilla::ScreenRotation mRotation;
  RefPtr<CancelableRunnable> mCreateWidgetTask;
  const nsCString mInitialContentURI;
  nsTArray<EmbedLiteChromeTabRestoreData> mPendingRestoreTabs;
  int32_t mPendingSelectedTabIndex;
  const bool mPrivateBrowsing;
  bool mRestoreTabsReceived;
  bool mInitialized;
  bool mDestroyAfterInit;
  bool mDestroying;
  int mDepth;
  float mDensity;
  float mDpi;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteHostedWindow);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_EMBEDLITE_HOSTED_WINDOW_H
