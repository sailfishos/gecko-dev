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

namespace mozilla {
namespace embedlite {

class EmbedLiteViewProcessChild : public PEmbedLiteViewChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteViewProcessChild)
public:
  MOZ_IMPLICIT EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId);
  virtual ~EmbedLiteViewProcessChild();

private:
  void InitGeckoWindow(const uint32_t& parentId);

protected:
  virtual bool
  RecvLoadURL(const nsString& url);

  virtual bool
  RecvGoBack();

  virtual bool
  RecvGoForward();

  virtual bool
  RecvStopLoad();

  virtual bool
  RecvReload(const bool& hardReload);

  virtual bool
  RecvLoadFrameScript(const nsString& uri);

  virtual bool
  RecvSetViewSize(const gfxSize& aSize);

  virtual bool
  RecvSetGLViewSize(const gfxSize& aSize);

  virtual bool
  RecvSetIsActive(const bool& aIsActive);

  virtual bool
  RecvSetIsFocused(const bool& aIsFocused);

  virtual bool
  RecvSuspendTimeouts();

  virtual bool
  RecvResumeTimeouts();

  virtual bool
  RecvAsyncScrollDOMEvent(
          const gfxRect& contentRect,
          const gfxSize& scrollSize);

  virtual bool
  RecvUpdateFrame(const FrameMetrics& frame);

  virtual bool
  RecvHandleDoubleTap(const nsIntPoint& point);

  virtual bool
  RecvHandleSingleTap(const nsIntPoint& point);

  virtual bool
  RecvHandleLongTap(
          const nsIntPoint& point,
          const ScrollableLayerGuid& aGuid,
          const uint64_t& aInputBlockId);

  virtual bool
  RecvAcknowledgeScrollUpdate(
          const ViewID& aScrollId,
          const uint32_t& aScrollGeneration);

  virtual bool
  RecvHandleTextEvent(
          const nsString& commit,
          const nsString& preEdit);

  virtual bool
  RecvHandleKeyPressEvent(
          const int& domKeyCode,
          const int& gmodifiers,
          const int& charCode);

  virtual bool
  RecvHandleKeyReleaseEvent(
          const int& domKeyCode,
          const int& gmodifiers,
          const int& charCode);

  virtual bool
  RecvMouseEvent(
          const nsString& aType,
          const float& aX,
          const float& aY,
          const int32_t& aButton,
          const int32_t& aClickCount,
          const int32_t& aModifiers,
          const bool& aIgnoreRootScrollFrame);

  virtual bool
  RecvInputDataTouchEvent(
          const ScrollableLayerGuid& aGuid,
          const MultiTouchInput& event,
          const uint64_t& aInputBlockId);

  virtual bool
  RecvInputDataTouchMoveEvent(
          const ScrollableLayerGuid& aGuid,
          const MultiTouchInput& event,
          const uint64_t& aInputBlockId);

  virtual bool
  RecvAddMessageListener(const nsCString& name);

  virtual bool
  RecvRemoveMessageListener(const nsCString& name);

  virtual bool
  RecvAddMessageListeners(const nsTArray<nsString>& messageNames);

  virtual bool
  RecvRemoveMessageListeners(const nsTArray<nsString>& messageNames);

  virtual bool
  RecvDestroy();

  virtual bool
  RecvAsyncMessage(const nsString& aMessage,
                   const nsString& aData);

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

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewProcessChild);
};
} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PROCESS_CHILD_H
