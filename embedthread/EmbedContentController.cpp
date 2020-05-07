/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedContentController.h"
#include "EmbedLog.h"
#include "EmbedLiteView.h"
#include "mozilla/Unused.h"
#include "EmbedLiteViewBaseParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "EmbedLiteCompositorBridgeParent.h"

using namespace mozilla::embedlite;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using mozilla::layers::GeckoContentController;

class FakeListener : public EmbedLiteViewListener {};

EmbedContentController::EmbedContentController(EmbedLiteViewBaseParent* aRenderFrame, MessageLoop* aUILoop)
  : mUILoop(aUILoop)
  , mRenderFrame(aRenderFrame)
{
}

EmbedContentController::~EmbedContentController()
{
  LOGT();
}

void EmbedContentController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  // We always need to post requests into the "UI thread" otherwise the
  // requests may get processed out of order.
  LOGT();
  // nsThreadUtils version
  mUILoop->PostTask(NewRunnableMethod<const FrameMetrics&>(this,
                                                           &EmbedContentController::DoRequestContentRepaint,
                                                           aFrameMetrics));
}

#if 0 // sha1 5753e3da8314bb0522bdbf92819beb6d89faeb06
void EmbedContentController::RequestFlingSnap(const FrameMetrics::ViewID &aScrollId, const mozilla::CSSPoint &aDestination)
{
  LOGT();
  mUILoop->PostTask(NewRunnableMethod(this, &EmbedContentController::DoRequestFlingSnap, aScrollId, aDestination));
}
#endif

void EmbedContentController::HandleTap(TapType aType, const LayoutDevicePoint &aPoint, Modifiers aModifiers, const EmbedContentController::ScrollableLayerGuid &aGuid, uint64_t aInputBlockId)
{
  switch (aType) {
    case GeckoContentController::TapType::eSingleTap:
      HandleSingleTap(aPoint, aModifiers, aGuid);
      break;
    case GeckoContentController::TapType::eDoubleTap:
      HandleDoubleTap(aPoint, aModifiers, aGuid);
      break;
    case GeckoContentController::TapType::eLongTap:
      HandleLongTap(aPoint, aModifiers, aGuid, aInputBlockId);
      break;
    case GeckoContentController::TapType::eSecondTap:
    case GeckoContentController::TapType::eLongTapUp:
    case GeckoContentController::TapType::eSentinel:
      // What to do with these?
      MOZ_FALLTHROUGH;
      break;
  }
}

void EmbedContentController::HandleDoubleTap(const LayoutDevicePoint& aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint &, Modifiers, const ScrollableLayerGuid &>(this,
                                                                                                  &EmbedContentController::HandleDoubleTap,
                                                                                                  aPoint,
                                                                                                  aModifiers,
                                                                                                  aGuid));
  } else if (mRenderFrame && !GetListener()->HandleDoubleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleDoubleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleSingleTap(const LayoutDevicePoint& aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint &, Modifiers, const ScrollableLayerGuid &>(this,
                                                                                                           &EmbedContentController::HandleSingleTap,
                                                                                                           aPoint,
                                                                                                           aModifiers,
                                                                                                           aGuid));
  } else if (mRenderFrame && !GetListener()->HandleSingleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleSingleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleLongTap(const LayoutDevicePoint &aPoint,
                                           Modifiers aModifiers,
                                           const ScrollableLayerGuid& aGuid,
                                           uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint &, Modifiers, const ScrollableLayerGuid &, uint64_t>(this,
                                                                                                                     &EmbedContentController::HandleLongTap,
                                                                                                                     aPoint,
                                                                                                                     aModifiers,
                                                                                                                     aGuid,
                                                                                                                     aInputBlockId));
  } else if (mRenderFrame && !GetListener()->HandleLongTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleLongTap(aPoint, aGuid, aInputBlockId);
  }
}

/**
 * Sends a scroll event to embedder.
 * |aIsRootScrollFrame| is a root scroll frame
 * |aContentRect| is in CSS pixels, relative to the current cssPage.
 * |aScrollableSize| is the current content width/height in CSS pixels.
 */
void EmbedContentController::DoSendScrollEvent(const FrameMetrics &aFrameMetrics)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const FrameMetrics &>(this,
                                                              &EmbedContentController::DoSendScrollEvent,
                                                              aFrameMetrics));
    return;
  } else {
    CSSRect contentRect = aFrameMetrics.CalculateCompositedRectInCssPixels();
    contentRect.MoveTo(aFrameMetrics.GetScrollOffset());
    CSSSize scrollableSize = aFrameMetrics.GetScrollableRect().Size();

    LOGNI("contentR[%g,%g,%g,%g], scrSize[%g,%g]",
          contentRect.x, contentRect.y, contentRect.width, contentRect.height,
          scrollableSize.width, scrollableSize.height);
    gfxRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
    gfxSize size(scrollableSize.width, scrollableSize.height);

    if (mRenderFrame && !GetListener()->HandleScrollEvent(aFrameMetrics.IsRootContent(), rect, size)) {
      Unused << mRenderFrame->SendHandleScrollEvent(aFrameMetrics.IsRootContent(), rect, size);
    }
  }
}

#if 0 // sha1 5753e3da8314bb0522bdbf92819beb6d89faeb06
void EmbedContentController::DoRequestFlingSnap(const FrameMetrics::ViewID &aScrollId, const mozilla::CSSPoint &aDestination)
{
  if (mRenderFrame) {
    Unused << mRenderFrame->SendRequestFlingSnap(aScrollId, aDestination);
  }
}
#endif

void EmbedContentController::DoNotifyAPZStateChange(const mozilla::layers::ScrollableLayerGuid &aGuid, APZStateChange aChange, int aArg)
{
  if (mRenderFrame) {
    Unused << mRenderFrame->SendNotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void EmbedContentController::DoNotifyFlushComplete()
{
  if (mRenderFrame) {
    Unused << mRenderFrame->SendNotifyFlushComplete();
  }
}

nsIntPoint EmbedContentController::convertIntPoint(const LayoutDevicePoint &aPoint)
{
  return nsIntPoint((int)nearbyint(aPoint.x), (int)nearbyint(aPoint.y));
}

#if 0 // sha1 6afa98a3ea0ba814b77f7aeb162624433e427ccf
void EmbedContentController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<>(this, &EmbedContentController::AcknowledgeScrollUpdate, aScrollId, aScrollGeneration));
    return;
  }
  if (mRenderFrame && !GetListener()->AcknowledgeScrollUpdate((uint32_t)aScrollId, aScrollGeneration)) {
    Unused << mRenderFrame->SendAcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  }
}
#endif

void EmbedContentController::ClearRenderFrame()
{
  mRenderFrame = nullptr;
}

/**
 * Schedules a runnable to run on the controller/UI thread at some time
 * in the future.
 */
void EmbedContentController::PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(Move(aTask), aDelayMs);
}

EmbedLiteViewListener *EmbedContentController::GetListener() const
{
  static FakeListener sFakeListener;
  return mRenderFrame && mRenderFrame->mView ?
         mRenderFrame->mView->GetListener() : &sFakeListener;
}

void EmbedContentController::DoRequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  LOGT("render frame %p", mRenderFrame);
  if (mRenderFrame && !GetListener()->RequestContentRepaint()) {
    DoSendScrollEvent(aFrameMetrics);
    Unused << mRenderFrame->SendUpdateFrame(aFrameMetrics);
  }
}

void EmbedContentController::NotifyAPZStateChange(const mozilla::layers::ScrollableLayerGuid &aGuid, APZStateChange aChange, int aArg)
{
  LOGT();
  mUILoop->PostTask(NewRunnableMethod<const mozilla::layers::ScrollableLayerGuid &, APZStateChange, int>(this,
                                                                                                         &EmbedContentController::DoNotifyAPZStateChange,
                                                                                                         aGuid,
                                                                                                         aChange,
                                                                                                         aArg));
}

void EmbedContentController::NotifyFlushComplete()
{
  LOGT();
  mUILoop->PostTask(NewRunnableMethod(this, &EmbedContentController::DoNotifyFlushComplete));
}

void EmbedContentController::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType, const EmbedContentController::ScrollableLayerGuid &aGuid, LayoutDeviceCoord aSpanChange, Modifiers aModifiers)
{
  LOGT("NOT YET IMPLEMENTED");
}

bool EmbedContentController::IsRepaintThread()
{
  LOGT("NOT YET IMPLEMENTED");
  return true;
}

void EmbedContentController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  LOGT("NOT YET IMPLEMENTED");
}
