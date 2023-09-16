/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_PARENT_H
#define MOZ_VIEW_EMBED_PARENT_H

#include "mozilla/embedlite/PEmbedLiteViewParent.h"
#include "mozilla/embedlite/EmbedLiteWindowParent.h"
#include "mozilla/WidgetUtils.h"
#include "EmbedLiteViewIface.h"
#include "GLDefs.h"
#include <functional>

#include "mozilla/layers/IAPZCTreeManager.h" // public IAPZCTreeManager

namespace mozilla {
namespace embedlite {

class EmbedContentController;
class EmbedLiteCompositorBridgeParent;
class EmbedLiteView;
class nsWindow;

class EmbedLiteViewParent : public PEmbedLiteViewParent,
                            public EmbedLiteViewIface,
                            public EmbedLiteWindowParentObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteViewParent)
public:
  EmbedLiteViewParent(const uint32_t &windowId,
                      const uint32_t &id,
                      const uint32_t &parentId,
                      const uintptr_t &parentBrowsingContext,
                      const bool &isPrivateWindow,
                      const bool &isDesktopMode);

  NS_DECL_EMBEDLITEVIEWIFACE
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  EmbedLiteCompositorBridgeParent* GetCompositor() { return mCompositor.get(); }; // XXX: Remove

protected:
  virtual ~EmbedLiteViewParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvInitialized();
  virtual mozilla::ipc::IPCResult RecvDestroyed();
  virtual mozilla::ipc::IPCResult RecvDynamicToolbarHeightChanged(const int &height);
  virtual mozilla::ipc::IPCResult RecvMarginsChanged(const int &top,
                                                     const int &right,
                                                     const int &bottom,
                                                     const int &left);
  virtual mozilla::ipc::IPCResult RecvOnLocationChanged(const nsCString &aLocation,
                                                        const bool &aCanGoBack,
                                                        const bool &aCanGoForward);

  virtual mozilla::ipc::IPCResult RecvOnLoadStarted(const nsCString &aLocation);
  virtual mozilla::ipc::IPCResult RecvOnLoadFinished();
  virtual mozilla::ipc::IPCResult RecvOnWindowCloseRequested();
  virtual mozilla::ipc::IPCResult RecvOnLoadRedirect();

  virtual mozilla::ipc::IPCResult RecvOnLoadProgress(const int32_t &aProgress,
                                                     const int32_t &aCurTotal,
                                                     const int32_t &aMaxTotal);

  virtual mozilla::ipc::IPCResult RecvOnSecurityChanged(const nsCString &aStatus,
                                                        const uint32_t &aState);

  virtual mozilla::ipc::IPCResult RecvOnFirstPaint(const int32_t &aX, const int32_t &aY);

  virtual mozilla::ipc::IPCResult RecvOnScrolledAreaChanged(const uint32_t &aWidth,
                                                            const uint32_t &aHeight);

  virtual mozilla::ipc::IPCResult RecvOnScrollChanged(const int32_t &offSetX,
                                                      const int32_t &offSetY);

  virtual mozilla::ipc::IPCResult RecvOnTitleChanged(const nsString &aTitle);

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString &aMessage,
                                                   const nsString &aData);
  virtual mozilla::ipc::IPCResult RecvSyncMessage(const nsString &aMessage,
                                                  const nsString &aJSON,
                                                  nsTArray<nsString> *aJSONRetVal);

  virtual mozilla::ipc::IPCResult RecvUpdateZoomConstraints(const uint32_t &aPresShellId,
                                                            const ViewID &aViewId,
                                                            const Maybe<ZoomConstraints> &aConstraints);

  virtual mozilla::ipc::IPCResult RecvZoomToRect(const uint32_t &aPresShellId,
                                                 const ViewID &aViewId,
                                                 const ZoomTarget &aRect);
  virtual mozilla::ipc::IPCResult RecvSetBackgroundColor(const nscolor &aColor);
  virtual mozilla::ipc::IPCResult RecvContentReceivedInputBlock(const uint64_t &aInputBlockId,
                                                                const bool &aPreventDefault);
  virtual mozilla::ipc::IPCResult RecvSetTargetAPZC(const uint64_t &aInputBlockId,
                                                    nsTArray<ScrollableLayerGuid> &&aTargets);
  virtual mozilla::ipc::IPCResult RecvSetAllowedTouchBehavior(const uint64_t &aInputBlockId,
                                                              nsTArray<mozilla::layers::TouchBehaviorFlags> &&aFlags);

  // IME
  virtual mozilla::ipc::IPCResult RecvGetInputContext(int32_t *aIMEEnabled,
                                                      int32_t *aIMEOpen);
  virtual mozilla::ipc::IPCResult RecvSetInputContext(const int32_t &aIMEEnabled,
                                                      const int32_t &aIMEOpen,
                                                      const nsString &aType,
                                                      const nsString &aInputmode,
                                                      const nsString &aActionHint,
                                                      const int32_t &aCause,
                                                      const int32_t &aFocusChange);

  virtual mozilla::ipc::IPCResult RecvOnHttpUserAgentUsed(const nsString &aHttpUserAgent);

  // EmbedLiteWindowParentObserver:
  void CompositorCreated() override;

  virtual mozilla::ipc::IPCResult RecvGetDPI(float *aValue);

  mozilla::embedlite::nsWindow *GetWindowWidget() const;

  bool GetScrollableRect(CSSRect &scrollableRect);

private:
  friend class EmbedContentController;
  friend class EmbedLiteCompositorBridgeParent;
  friend class PEmbedLiteViewParent;
  // The sole purpose of this friendliness is to set mView which is used only as a proxy to view's Listener
  friend class EmbedLiteView;

  void SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor); // XXX: Remove
  void UpdateScrollController();

  mozilla::layers::IAPZCTreeManager *GetApzcTreeManager();

  uint32_t mWindowId;
  uint32_t mId;
  EmbedLiteView* mView;
  bool mViewAPIDestroyed;
  EmbedLiteWindowParent& mWindow;
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;

  float mDPI;

  nsISerialEventTarget *mThread;
  int mLastIMEState;

  GLuint mUploadTexture;

  RefPtr<mozilla::layers::IAPZCTreeManager> mApzcTreeManager;
  RefPtr<EmbedContentController> mContentController;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PARENT_H
