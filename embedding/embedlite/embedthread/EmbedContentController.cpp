/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedContentController.h"
#include "EmbedLog.h"
#include "EmbedLiteView.h"
#include "mozilla/unused.h"
#include "EmbedLiteViewBaseParent.h"
#include "mozilla/layers/CompositorParent.h"
#include "EmbedLiteCompositorParent.h"

using namespace mozilla::embedlite;
using namespace mozilla::gfx;
using namespace mozilla::layers;

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
  mUILoop->PostTask(
              FROM_HERE,
              NewRunnableMethod(this, &EmbedContentController::DoRequestContentRepaint, aFrameMetrics));
}

void EmbedContentController::RequestFlingSnap(const FrameMetrics::ViewID &aScrollId, const mozilla::CSSPoint &aDestination)
{
  LOGT();
  mUILoop->PostTask(
              FROM_HERE,
              NewRunnableMethod(this, &EmbedContentController::DoRequestFlingSnap, aScrollId, aDestination));
}

void EmbedContentController::HandleDoubleTap(const CSSPoint& aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &EmbedContentController::HandleDoubleTap, aPoint, aModifiers, aGuid));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    Unused << mRenderFrame->SendHandleDoubleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleSingleTap(const CSSPoint& aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &EmbedContentController::HandleSingleTap, aPoint, aModifiers, aGuid));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleSingleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    Unused << mRenderFrame->SendHandleSingleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleLongTap(const CSSPoint& aPoint,
                                           Modifiers aModifiers,
                                           const ScrollableLayerGuid& aGuid,
                                           uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &EmbedContentController::HandleLongTap, aPoint, aModifiers, aGuid, aInputBlockId));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleLongTap(nsIntPoint(aPoint.x, aPoint.y))) {
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
    mUILoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &EmbedContentController::DoSendScrollEvent, aFrameMetrics));
    return;
  }

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

void EmbedContentController::DoRequestFlingSnap(const FrameMetrics::ViewID &aScrollId, const mozilla::CSSPoint &aDestination)
{
  if (mRenderFrame) {
    Unused << mRenderFrame->SendRequestFlingSnap(aScrollId, aDestination);
  }
}

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

void EmbedContentController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &EmbedContentController::AcknowledgeScrollUpdate, aScrollId, aScrollGeneration));
    return;
  }
  if (mRenderFrame && !GetListener()->AcknowledgeScrollUpdate((uint32_t)aScrollId, aScrollGeneration)) {
    Unused << mRenderFrame->SendAcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  }
}

void EmbedContentController::ClearRenderFrame()
{
  mRenderFrame = nullptr;
}

/**
 * Schedules a runnable to run on the controller/UI thread at some time
 * in the future.
 */
void EmbedContentController::PostDelayedTask(Task* aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

EmbedLiteViewListener* const EmbedContentController::GetListener() const
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
  mUILoop->PostTask(
              FROM_HERE,
              NewRunnableMethod(this, &EmbedContentController::DoNotifyAPZStateChange, aGuid, aChange, aArg));
}

void EmbedContentController::NotifyFlushComplete()
{
  LOGT();
  mUILoop->PostTask(
              FROM_HERE,
              NewRunnableMethod(this, &EmbedContentController::DoNotifyFlushComplete));
}
