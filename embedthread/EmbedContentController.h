/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBED_CONTENT_CONTROLLER_H
#define MOZ_EMBED_CONTENT_CONTROLLER_H

#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "mozilla/layers/GeckoContentController.h"
#include "FrameMetrics.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteViewListener;
class EmbedLiteViewBaseParent;

class EmbedContentController : public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::TouchBehaviorFlags TouchBehaviorFlags;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;

public:
  EmbedContentController(EmbedLiteViewBaseParent* aRenderFrame, MessageLoop* aUILoop);
  virtual ~EmbedContentController();

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) override;
  virtual void RequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                                const mozilla::CSSPoint& aDestination) override;

  virtual void HandleDoubleTap(const CSSPoint& aPoint, Modifiers aModifiers, const ScrollableLayerGuid& aGuid) override;
  virtual void HandleSingleTap(const CSSPoint& aPoint, Modifiers aModifiers, const ScrollableLayerGuid& aGuid) override;
  virtual void HandleLongTap(const CSSPoint& aPoint, Modifiers aModifiers, const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId) override;
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID&, const uint32_t&) override;
  void ClearRenderFrame();
  virtual void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) override;
  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);

  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg = 0) override;
  virtual void NotifyFlushComplete() override;

private:
  EmbedLiteViewListener *GetListener() const;
  void DoRequestContentRepaint(const FrameMetrics& aFrameMetrics);
  void DoSendScrollEvent(const FrameMetrics& aFrameMetrics);
  void DoRequestFlingSnap(const FrameMetrics::ViewID &aScrollId, const mozilla::CSSPoint &aDestination);
  void DoNotifyAPZStateChange(const mozilla::layers::ScrollableLayerGuid &aGuid, APZStateChange aChange, int aArg);
  void DoNotifyFlushComplete();

  MessageLoop* mUILoop;
  EmbedLiteViewBaseParent* mRenderFrame;
};

}}

#endif
