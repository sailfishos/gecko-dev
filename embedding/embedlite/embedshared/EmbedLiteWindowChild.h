/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_CHILD_H
#define MOZ_WINDOW_EMBED_CHILD_H

#include "mozilla/embedlite/PEmbedLiteWindowChild.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsTArray.h"
#include "base/task.h" // for CancelableRunnable

class nsIAppWindow;

namespace mozilla {
class MultiTouchInput;
namespace dom {
class BrowsingContext;
class Promise;
}
}

namespace mozilla {
namespace embedlite {

class nsWindow;
class EmbedLiteChromeSessionChild;
class EmbedLiteWindowListener;

class EmbedLiteWindowChild : public PEmbedLiteWindowChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteWindowChild)

public:
  EmbedLiteWindowChild(const uint16_t &width, const uint16_t &height,
                       const uint32_t &id,
                       EmbedLiteWindowListener *aListener,
                       const bool &chromeHosted,
                       const nsCString &initialContentURI);

  static EmbedLiteWindowChild *From(const uint32_t id);
  static bool RequestChromeTabBeforeUnloadPrompt(
    dom::BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    dom::Promise* aPromise);

  uint32_t GetUniqueID() const { return mId; }
  nsWindow *GetWidget() const;
  LayoutDeviceIntRect GetSize() const { return mBounds; }
  EmbedLiteWindowListener* GetListener() const { return mListener; }
  void SetScreenProperties(const int &depth, const float &density, const float &dpi);
  float GetDPI() const { return mDpi; }
  float GetDensity() const { return mDensity > 0.0f ? mDensity : 1.0f; }

protected:
  virtual ~EmbedLiteWindowChild() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  friend class PEmbedLiteWindowChild;
  friend class EmbedLiteChromeSessionChild;
  friend class nsWindow;
  void CreateWidget();
  bool CreateChromeAppWindow();
  void DestroyChromeAppWindow();
  void ChromeSessionInitializationFinished(bool aSuccess);
  void ChromeInputContextChanged(const widget::InputContext& aContext,
                                 const widget::InputContextAction& aAction);

  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvSetSize(const gfxSize &size);
  mozilla::ipc::IPCResult RecvSetContentOrientation(const uint32_t &);
  mozilla::ipc::IPCResult RecvLoadURL(const nsCString& aURL,
                                     const bool& aFromExternal);
  mozilla::ipc::IPCResult RecvGoBack(const bool& aRequireUserInteraction,
                                    const bool& aUserActivation);
  mozilla::ipc::IPCResult RecvGoForward(const bool& aRequireUserInteraction,
                                       const bool& aUserActivation);
  mozilla::ipc::IPCResult RecvStopLoad();
  mozilla::ipc::IPCResult RecvReload(const bool& aHardReload);
  mozilla::ipc::IPCResult RecvRestoreTabs(
    const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
    const int32_t& aSelectedTabIndex);
  mozilla::ipc::IPCResult RecvNewTab(const nsCString& aURL,
                                    const uint64_t& aPersistentId,
                                    const bool& aFromExternal,
                                    const bool& aInBackground);
  mozilla::ipc::IPCResult RecvAssociateTab(
    const uint64_t& aTabId, const uint64_t& aPersistentId);
  mozilla::ipc::IPCResult RecvSelectTab(const uint64_t& aTabId);
  mozilla::ipc::IPCResult RecvCloseTab(const uint64_t& aTabId);
  mozilla::ipc::IPCResult RecvResolveBeforeUnloadPrompt(
    const uint64_t& aRequestId, const uint64_t& aTabId,
    const bool& aPermit);
  mozilla::ipc::IPCResult RecvSetActive(const bool& aActive);
  mozilla::ipc::IPCResult RecvSetFocused(const bool& aFocused);
  mozilla::ipc::IPCResult RecvHandleTextEvent(
    const nsCString& aCommit, const nsCString& aPreEdit,
    const int32_t& aReplacementStart,
    const int32_t& aReplacementLength);
  mozilla::ipc::IPCResult RecvHandleKeyPressEvent(
    const int32_t& aDomKeyCode, const int32_t& aModifiers,
    const int32_t& aCharCode);
  mozilla::ipc::IPCResult RecvHandleKeyReleaseEvent(
    const int32_t& aDomKeyCode, const int32_t& aModifiers,
    const int32_t& aCharCode);
  mozilla::ipc::IPCResult RecvReceiveInputEvent(
    const MultiTouchInput& aEvent);
  void RefreshScreen();

  uint32_t mId;
  EmbedLiteWindowListener *const mListener;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIAppWindow> mChromeWindow;
  RefPtr<EmbedLiteChromeSessionChild> mChromeSession;
  LayoutDeviceIntRect mBounds;
  mozilla::ScreenRotation mRotation;
  RefPtr<CancelableRunnable> mCreateWidgetTask;
  const nsCString mInitialContentURI;
  nsTArray<EmbedLiteChromeTabRestoreData> mPendingRestoreTabs;
  int32_t mPendingSelectedTabIndex;

  const bool mChromeHosted;
  bool mRestoreTabsReceived;
  bool mInitialized;
  bool mDestroyAfterInit;
  bool mDestroying;

  int mDepth;
  float mDensity;
  float mDpi;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_CHILD_H
