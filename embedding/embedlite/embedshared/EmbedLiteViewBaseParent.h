/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_BASE_PARENT_H
#define MOZ_VIEW_EMBED_BASE_PARENT_H

#include "mozilla/embedlite/PEmbedLiteViewParent.h"
#include "mozilla/WidgetUtils.h"
#include "EmbedLiteViewIface.h"
#include "EmbedLiteWindowBaseParent.h"
#include "GLDefs.h"
#include <functional>

#include "mozilla/layers/IAPZCTreeManager.h" // public IAPZCTreeManager

namespace mozilla {
namespace embedlite {

class EmbedContentController;
class EmbedLiteCompositorBridgeParent;
class EmbedLiteView;
class nsWindow;

class EmbedLiteViewBaseParent : public PEmbedLiteViewParent,
                                public EmbedLiteViewIface,
				public EmbedLiteWindowParentObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteViewBaseParent)
public:
  EmbedLiteViewBaseParent(const uint32_t& windowId, const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow, const bool &isDesktopMode);

  NS_DECL_EMBEDLITEVIEWIFACE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  EmbedLiteCompositorBridgeParent* GetCompositor() { return mCompositor.get(); }; // XXX: Remove

protected:
  virtual ~EmbedLiteViewBaseParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvInitialized() override;
  virtual mozilla::ipc::IPCResult RecvDestroyed() override;
  virtual mozilla::ipc::IPCResult RecvOnLocationChanged(const nsCString &aLocation,
                                                        const bool &aCanGoBack,
                                                        const bool &aCanGoForward) override;

  virtual mozilla::ipc::IPCResult RecvOnLoadStarted(const nsCString &aLocation) override;
  virtual mozilla::ipc::IPCResult RecvOnLoadFinished() override;
  virtual mozilla::ipc::IPCResult RecvOnWindowCloseRequested() override;
  virtual mozilla::ipc::IPCResult RecvOnLoadRedirect() override;

  virtual mozilla::ipc::IPCResult RecvOnLoadProgress(const int32_t &aProgress,
                                                     const int32_t &aCurTotal,
                                                     const int32_t &aMaxTotal) override;

  virtual mozilla::ipc::IPCResult RecvOnSecurityChanged(const nsCString &aStatus,
                                                        const uint32_t &aState) override;

  virtual mozilla::ipc::IPCResult RecvOnFirstPaint(const int32_t &aX, const int32_t &aY) override;

  virtual mozilla::ipc::IPCResult RecvOnScrolledAreaChanged(const uint32_t &aWidth,
                                                            const uint32_t &aHeight) override;

  virtual mozilla::ipc::IPCResult RecvOnScrollChanged(const int32_t &offSetX,
                                                      const int32_t &offSetY) override;

  virtual mozilla::ipc::IPCResult RecvOnTitleChanged(const nsString &aTitle) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString &aMessage,
                                                   const nsString &aData) override;
  virtual mozilla::ipc::IPCResult RecvSyncMessage(const nsString &aMessage,
                                                  const nsString &aJSON,
                                                  InfallibleTArray<nsString> *aJSONRetVal) override;
  virtual mozilla::ipc::IPCResult RecvRpcMessage(const nsString &aMessage,
                                                 const nsString &aJSON,
                                                 InfallibleTArray<nsString> *aJSONRetVal) override;

  virtual mozilla::ipc::IPCResult RecvUpdateZoomConstraints(const uint32_t &aPresShellId,
                                                            const ViewID &aViewId,
                                                            const Maybe<ZoomConstraints> &aConstraints) override;

  virtual mozilla::ipc::IPCResult RecvZoomToRect(const uint32_t &aPresShellId,
                                                 const ViewID &aViewId,
                                                 const CSSRect &aRect) override;
  virtual mozilla::ipc::IPCResult RecvSetBackgroundColor(const nscolor &aColor) override;
  virtual mozilla::ipc::IPCResult RecvContentReceivedInputBlock(const ScrollableLayerGuid &aGuid,
                                             const uint64_t &aInputBlockId,
                                             const bool &aPreventDefault) override;
  virtual mozilla::ipc::IPCResult RecvSetTargetAPZC(const uint64_t &aInputBlockId,
                                                    nsTArray<ScrollableLayerGuid> &&aTargets) override;
  virtual mozilla::ipc::IPCResult RecvSetAllowedTouchBehavior(const uint64_t &aInputBlockId,
                                                              nsTArray<mozilla::layers::TouchBehaviorFlags> &&aFlags) override;

  // IME
  virtual mozilla::ipc::IPCResult RecvGetInputContext(int32_t *aIMEEnabled,
                                                      int32_t *aIMEOpen) override;
  virtual mozilla::ipc::IPCResult RecvSetInputContext(const int32_t &aIMEEnabled,
                                                      const int32_t &aIMEOpen,
                                                      const nsString &aType,
                                                      const nsString &aInputmode,
                                                      const nsString &aActionHint,
                                                      const int32_t &aCause,
                                                      const int32_t &aFocusChange) override;

  virtual mozilla::ipc::IPCResult RecvOnHttpUserAgentUsed(const nsString &aHttpUserAgent) override;

  // EmbedLiteWindowParentObserver:
  void CompositorCreated() override;

  virtual mozilla::ipc::IPCResult RecvGetDPI(float *aValue) override;

  mozilla::embedlite::nsWindow *GetWindowWidget() const;

private:
  friend class EmbedContentController;
  friend class EmbedLiteCompositorBridgeParent;
  // The sole purpose of this friendliness is to set mView which is used only as a proxy to view's Listener
  friend class EmbedLiteView;

  void SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor); // XXX: Remove
  void UpdateScrollController();

  mozilla::layers::IAPZCTreeManager *GetApzcTreeManager();

  uint32_t mWindowId;
  uint32_t mId;
  EmbedLiteView* mView;
  bool mViewAPIDestroyed;
  EmbedLiteWindowBaseParent& mWindow;
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;

  float mDPI;

  MessageLoop* mUILoop;
  int mLastIMEState;

  GLuint mUploadTexture;

  RefPtr<mozilla::layers::IAPZCTreeManager> mApzcTreeManager;
  RefPtr<EmbedContentController> mContentController;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewBaseParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_BASE_PARENT_H
