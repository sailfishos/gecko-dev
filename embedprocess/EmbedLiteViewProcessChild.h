/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_PROCESS_CHILD_H
#define MOZ_VIEW_EMBED_PROCESS_CHILD_H

#include "mozilla/embedlite/PEmbedLiteViewChild.h"

#include "nsIWebBrowser.h"
#include "nsIWidget.h"
#include "nsIWebNavigation.h"
#include "WebBrowserChrome.h"
#include "nsIEmbedBrowserChromeListener.h"
#include "TabChildHelper.h"
#include "EmbedLiteViewChildIface.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteViewProcessChild : public PEmbedLiteViewChild,
                                  public nsIEmbedBrowserChromeListener,
                                  public EmbedLiteViewChildIface
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteViewProcessChild)
public:
  MOZ_IMPLICIT EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId);

  NS_DECL_NSIEMBEDBROWSERCHROMELISTENER

  virtual ~EmbedLiteViewProcessChild();

private:
  void InitGeckoWindow(const uint32_t& parentId);

protected:
  virtual bool
  SetInputContext(const int32_t& IMEEnabled,
                  const int32_t& IMEOpen,
                  const nsString& type,
                  const nsString& inputmode,
                  const nsString& actionHint,
                  const int32_t& cause,
                  const int32_t& focusChange) MOZ_OVERRIDE;

  virtual bool
  GetInputContext(int32_t* IMEEnabled,
                  int32_t* IMEOpen,
                  intptr_t* NativeIMEContext) MOZ_OVERRIDE;
  virtual void ResetInputState() MOZ_OVERRIDE;
  virtual gfxSize GetGLViewSize() MOZ_OVERRIDE;

  /*------------------------------------------------------------------*/

  virtual bool
  ZoomToRect(const uint32_t& aPresShellId,
             const ViewID& aViewId,
             const CSSRect& aRect) MOZ_OVERRIDE;

  virtual bool
  UpdateZoomConstraints(const uint32_t& aPresShellId,
                        const ViewID& aViewId,
                        const bool& aIsRoot,
                        const ZoomConstraints& aConstraints) MOZ_OVERRIDE;

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

  virtual nsIWebNavigation* WebNavigation() MOZ_OVERRIDE;
  virtual nsIWidget* WebWidget() MOZ_OVERRIDE;

  /*------------------------------------------------------------------*/

  virtual uint32_t GetID() MOZ_OVERRIDE;
  virtual nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome) MOZ_OVERRIDE;

  virtual nsresult GetBrowser(nsIWebBrowser** outBrowser) MOZ_OVERRIDE;
  virtual uint64_t GetOuterID() MOZ_OVERRIDE;
  virtual void AddGeckoContentListener(EmbedLiteContentController* listener) MOZ_OVERRIDE;
  virtual void RemoveGeckoContentListener(EmbedLiteContentController* listener) MOZ_OVERRIDE;

  virtual bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId) MOZ_OVERRIDE;
  virtual bool ContentReceivedTouch(const mozilla::layers::ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault) MOZ_OVERRIDE;


  /*------------------------------------------------------------------*/
  virtual bool
  RecvLoadURL(const nsString& url) MOZ_OVERRIDE;

  virtual bool
  RecvGoBack() MOZ_OVERRIDE;

  virtual bool
  RecvGoForward() MOZ_OVERRIDE;

  virtual bool
  RecvStopLoad() MOZ_OVERRIDE;

  virtual bool
  RecvReload(const bool& hardReload) MOZ_OVERRIDE;

  virtual bool
  RecvLoadFrameScript(const nsString& uri) MOZ_OVERRIDE;

  virtual bool
  RecvSetViewSize(const gfxSize& aSize) MOZ_OVERRIDE;

  virtual bool
  RecvSetGLViewSize(const gfxSize& aSize) MOZ_OVERRIDE;

  virtual bool
  RecvSetIsActive(const bool& aIsActive) MOZ_OVERRIDE;

  virtual bool
  RecvSetIsFocused(const bool& aIsFocused) MOZ_OVERRIDE;

  virtual bool
  RecvSuspendTimeouts() MOZ_OVERRIDE;

  virtual bool
  RecvResumeTimeouts() MOZ_OVERRIDE;

  virtual bool
  RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                          const gfxSize& scrollSize) MOZ_OVERRIDE;

  virtual bool
  RecvUpdateFrame(const FrameMetrics& frame) MOZ_OVERRIDE;

  virtual bool
  RecvHandleDoubleTap(const nsIntPoint& point) MOZ_OVERRIDE;

  virtual bool
  RecvHandleSingleTap(const nsIntPoint& point) MOZ_OVERRIDE;

  virtual bool
  RecvHandleLongTap(const nsIntPoint& point,
                    const ScrollableLayerGuid& aGuid,
                    const uint64_t& aInputBlockId) MOZ_OVERRIDE;

  virtual bool
  RecvAcknowledgeScrollUpdate(
          const ViewID& aScrollId,
          const uint32_t& aScrollGeneration) MOZ_OVERRIDE;

  virtual bool
  RecvHandleTextEvent(
          const nsString& commit,
          const nsString& preEdit) MOZ_OVERRIDE;

  virtual bool
  RecvHandleKeyPressEvent(
          const int& domKeyCode,
          const int& gmodifiers,
          const int& charCode) MOZ_OVERRIDE;

  virtual bool
  RecvHandleKeyReleaseEvent(
          const int& domKeyCode,
          const int& gmodifiers,
          const int& charCode) MOZ_OVERRIDE;

  virtual bool
  RecvMouseEvent(
          const nsString& aType,
          const float& aX,
          const float& aY,
          const int32_t& aButton,
          const int32_t& aClickCount,
          const int32_t& aModifiers,
          const bool& aIgnoreRootScrollFrame) MOZ_OVERRIDE;

  virtual bool
  RecvInputDataTouchEvent(
          const ScrollableLayerGuid& aGuid,
          const MultiTouchInput& event,
          const uint64_t& aInputBlockId) MOZ_OVERRIDE;

  virtual bool
  RecvInputDataTouchMoveEvent(
          const ScrollableLayerGuid& aGuid,
          const MultiTouchInput& event,
          const uint64_t& aInputBlockId) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListener(const nsCString& name) MOZ_OVERRIDE;

  virtual bool
  RecvRemoveMessageListener(const nsCString& name) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListeners(const nsTArray<nsString>& messageNames) MOZ_OVERRIDE;

  virtual bool
  RecvRemoveMessageListeners(const nsTArray<nsString>& messageNames) MOZ_OVERRIDE;

  virtual bool
  RecvDestroy() MOZ_OVERRIDE;

  virtual bool
  RecvAsyncMessage(const nsString& aMessage,
                   const nsString& aData) MOZ_OVERRIDE;

private:
  CancelableTask* mInitWindowTask;
  nsCOMPtr<nsIWebBrowser> mWebBrowser;

  uint32_t mId;
  uint64_t mOuterId;
  nsCOMPtr<nsIWidget> mWidget;
  nsRefPtr<WebBrowserChrome> mChrome;
  nsCOMPtr<nsIDOMWindow> mDOMWindow;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  gfxSize mViewSize;
  bool mViewResized;
  gfxSize mGLViewSize;

  nsRefPtr<TabChildHelper> mHelper;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewProcessChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PROCESS_CHILD_H
