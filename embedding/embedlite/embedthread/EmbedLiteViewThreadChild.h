/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_THREAD_CHILD_H
#define MOZ_VIEW_EMBED_THREAD_CHILD_H

#include "mozilla/embedlite/PEmbedLiteViewChild.h"

#include "nsIWebBrowser.h"
#include "nsIWidget.h"
#include "nsIWebNavigation.h"
#include "WebBrowserChrome.h"
#include "nsIEmbedBrowserChromeListener.h"
#include "TabChildHelper.h"
#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteViewBaseChild.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteContentController;
class EmbedLitePuppetWidget;
class EmbedLiteAppThreadChild;

class EmbedLiteViewThreadChild : public EmbedLiteViewBaseChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteViewThreadChild)
public:
  EmbedLiteViewThreadChild(const uint32_t& id, const uint32_t& parentId);

  NS_DECL_NSIEMBEDBROWSERCHROMELISTENER

/*---------TabChildIface---------------*/

  virtual bool HasMessageListener(const nsAString& aMessageName) MOZ_OVERRIDE;

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage) MOZ_OVERRIDE;
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 InfallibleTArray<nsString>* aJSONRetVal) MOZ_OVERRIDE;
  virtual bool DoCallRpcMessage(const char16_t* aMessageName,
                                const char16_t* aMessage,
                                InfallibleTArray<nsString>* aJSONRetVal) MOZ_OVERRIDE;

  /**
   * Relay given frame metrics to listeners subscribed via EmbedLiteAppService
   */
  virtual void RelayFrameMetrics(const mozilla::layers::FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;

/*---------TabChildIface---------------*/

  void AddGeckoContentListener(EmbedLiteContentController* listener);
  void RemoveGeckoContentListener(EmbedLiteContentController* listener);

  /**
   * This method is used by EmbedLiteAppService::ZoomToRect() only.
   */
  bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId);

  virtual bool RecvAsyncMessage(const nsString& aMessage,
                                const nsString& aData);

/*---------WidgetIface---------------*/

  virtual void ResetInputState() MOZ_OVERRIDE;
  virtual gfxSize GetGLViewSize() MOZ_OVERRIDE;

/*---------WidgetIface---------------*/

protected:
  virtual ~EmbedLiteViewThreadChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDestroy() MOZ_OVERRIDE;

  virtual bool RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                       const gfxSize& scrollSize) MOZ_OVERRIDE;

  virtual bool RecvHandleDoubleTap(const nsIntPoint& aPoint) MOZ_OVERRIDE;
  virtual bool RecvHandleSingleTap(const nsIntPoint& aPoint) MOZ_OVERRIDE;
  virtual bool RecvHandleLongTap(const nsIntPoint& aPoint,
                                 const mozilla::layers::ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId) MOZ_OVERRIDE;
  virtual bool RecvMouseEvent(const nsString& aType,
                              const float&    aX,
                              const float&    aY,
                              const int32_t&  aButton,
                              const int32_t&  aClickCount,
                              const int32_t&  aModifiers,
                              const bool&     aIgnoreRootScrollFrame) MOZ_OVERRIDE;
  virtual bool RecvHandleTextEvent(const nsString& commit, const nsString& preEdit) MOZ_OVERRIDE;
  virtual bool RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) MOZ_OVERRIDE;
  virtual bool RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) MOZ_OVERRIDE;
  virtual bool RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&, const uint64_t& aInputBlockId) MOZ_OVERRIDE;
  virtual bool RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&, const uint64_t& aInputBlockId) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListener(const nsCString&) MOZ_OVERRIDE;
  virtual bool
  RecvRemoveMessageListener(const nsCString&) MOZ_OVERRIDE;
  void RecvAsyncMessage(const nsAString& aMessage,
                        const nsAString& aData) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListeners(const InfallibleTArray<nsString>& messageNames) MOZ_OVERRIDE;

  virtual bool
  RecvRemoveMessageListeners(const InfallibleTArray<nsString>& messageNames) MOZ_OVERRIDE;

private:
  friend class TabChildHelper;
  friend class EmbedLiteAppService;
  friend class EmbedLiteAppThreadChild;

  EmbedLiteAppThreadChild* AppChild();
  void InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint = nullptr);

  bool mDispatchSynthMouseEvents;
  bool mIMEComposing;
  uint64_t mPendingTouchPreventedBlockId;

  nsDataHashtable<nsStringHashKey, bool/*start with key*/> mRegisteredMessages;
  nsTArray<EmbedLiteContentController*> mControllerListeners;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_THREAD_CHILD_H
