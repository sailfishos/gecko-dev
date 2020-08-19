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

  virtual void HandleTap(TapType aType,
                         const LayoutDevicePoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId) override;

  void ClearRenderFrame();
  virtual void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) override;
  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);

  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg = 0) override;
  virtual void NotifyFlushComplete() override;

  virtual void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                  const ScrollableLayerGuid& aGuid,
                                  LayoutDeviceCoord aSpanChange,
                                  Modifiers aModifiers) override;

  virtual bool IsRepaintThread() override;

  virtual void NotifyAsyncScrollbarDragRejected(const FrameMetrics::ViewID &aViewId) override;
  virtual void NotifyAsyncAutoscrollRejected(const FrameMetrics::ViewID &aViewId) override;
  virtual void CancelAutoscroll(const ScrollableLayerGuid& aGuid) override;

  virtual void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;

private:
  EmbedLiteViewListener *GetListener() const;

  void HandleDoubleTap(const LayoutDevicePoint aPoint, Modifiers aModifiers, const ScrollableLayerGuid aGuid);
  void HandleSingleTap(const LayoutDevicePoint aPoint, Modifiers aModifiers, const ScrollableLayerGuid aGuid);
  void HandleLongTap(const LayoutDevicePoint aPoint, Modifiers aModifiers, const ScrollableLayerGuid aGuid, uint64_t aInputBlockId);

  void DoRequestContentRepaint(const FrameMetrics aFrameMetrics);
  void DoSendScrollEvent(const FrameMetrics aFrameMetrics);

  void DoNotifyAPZStateChange(const mozilla::layers::ScrollableLayerGuid &aGuid, APZStateChange aChange, int aArg);
  void DoNotifyFlushComplete();

  nsIntPoint convertIntPoint(const LayoutDevicePoint &aPoint);

  MessageLoop* mUILoop;
  EmbedLiteViewBaseParent* mRenderFrame;
};

}}

#endif
