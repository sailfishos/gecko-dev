/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedContentController.h"
#include "EmbedLog.h"
#include "EmbedLiteView.h"
#include "mozilla/Unused.h"
#include "EmbedLiteViewParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "EmbedLiteCompositorBridgeParent.h"

using namespace mozilla::embedlite;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using mozilla::layers::GeckoContentController;

class FakeListener : public EmbedLiteViewListener {};

EmbedContentController::EmbedContentController(EmbedLiteViewParent *aRenderFrame, nsISerialEventTarget *aUIThread)
  : mUILoop(MessageLoop::current())
  , mUIThread(aUIThread)
  , mRenderFrame(aRenderFrame)
{
}

EmbedContentController::~EmbedContentController()
{
  LOGT();
}

void EmbedContentController::RequestContentRepaint(const layers::RepaintRequest &aRequest)
{
  LOGT();

  // We always need to post requests into the "UI thread" otherwise the
  // requests may get processed out of order.
  mUILoop->PostTask(NewRunnableMethod<const layers::RepaintRequest>("mozilla::embedlite::EmbedContentController::DoRequestContentRepaint",
                                                                        this,
                                                                        &EmbedContentController::DoRequestContentRepaint,
                                                                        aRequest));
}

void EmbedContentController::NotifyLayerTransforms(nsTArray<layers::MatrixMessage>&& aTransforms)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::NotifyAsyncScrollbarDragInitiated(uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID &aScrollId, layers::ScrollDirection aDirection)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::HandleTap(TapType aType, const LayoutDevicePoint &aPoint,
                                       Modifiers aModifiers, const ScrollableLayerGuid &aGuid, uint64_t aInputBlockId)
{
  switch (aType) {
    case GeckoContentController::TapType::eSingleTap:
      HandleSingleTap(aPoint, aModifiers, aGuid, aInputBlockId);
      break;
    case GeckoContentController::TapType::eSecondTap:
      [[fallthrough]];
    case GeckoContentController::TapType::eDoubleTap:
      HandleDoubleTap(aPoint, aModifiers, aGuid, aInputBlockId);
      break;
    case GeckoContentController::TapType::eLongTap:
      HandleLongTap(aPoint, aModifiers, aGuid, aInputBlockId);
      break;
    case GeckoContentController::TapType::eLongTapUp:
      break;
  }
}

void EmbedContentController::HandleDoubleTap(const LayoutDevicePoint aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid aGuid,
                                             uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint,
                                          Modifiers,
                                          const ScrollableLayerGuid,
                                          uint64_t>("mozilla::embedlite::EmbedContentController::HandleDoubleTap",
                                                    this,
                                                    &EmbedContentController::HandleDoubleTap,
                                                    aPoint,
                                                    aModifiers,
                                                    aGuid,
                                                    aInputBlockId));
    return;
  }

  if (mRenderFrame && !GetListener()->HandleDoubleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleDoubleTap(aPoint, aModifiers, aGuid, aInputBlockId);
  }
}

void EmbedContentController::HandleSingleTap(const LayoutDevicePoint aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid aGuid,
                                             uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint,
                                          Modifiers,
                                          const ScrollableLayerGuid,
                                          uint64_t>("mozilla::embedlite::EmbedContentController::HandleSingleTap",
                                                    this,
                                                    &EmbedContentController::HandleSingleTap,
                                                    aPoint,
                                                    aModifiers,
                                                    aGuid,
                                                    aInputBlockId));
    return;
  }

  if (mRenderFrame && !GetListener()->HandleSingleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleSingleTap(aPoint, aModifiers, aGuid, aInputBlockId);
  }
}

void EmbedContentController::HandleLongTap(const LayoutDevicePoint aPoint,
                                           Modifiers aModifiers,
                                           const ScrollableLayerGuid aGuid,
                                           uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const LayoutDevicePoint, Modifiers, const ScrollableLayerGuid, uint64_t>("mozilla::embedlite::EmbedContentController::HandleLongTap",
                                                                                                                 this,
                                                                                                                 &EmbedContentController::HandleLongTap,
                                                                                                                 aPoint,
                                                                                                                 aModifiers,
                                                                                                                 aGuid,
                                                                                                                 aInputBlockId));
    return;
  }

  if (mRenderFrame && !GetListener()->HandleLongTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleLongTap(aPoint, aGuid, aInputBlockId);
  }
}

/**
 * Sends a scroll event to embedder.
 * |aIsRootScrollFrame| is a root scroll frame
 * |aContentRect| is in CSS pixels, relative to the current cssPage.
 * |aScrollableSize| is the current content width/height in CSS pixels.
 */
void EmbedContentController::DoSendScrollEvent(const layers::RepaintRequest aRequest)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<const layers::RepaintRequest>("mozilla::embedlite::EmbedContentController::DoSendScrollEvent",
                                                                        this,
                                                                        &EmbedContentController::DoSendScrollEvent,
                                                                        aRequest));
    return;
  }

  CSSRect contentRect = (aRequest.GetZoom() == CSSToParentLayerScale2D(0, 0)) ? CSSRect() : (aRequest.GetCompositionBounds() / aRequest.GetZoom());
  contentRect.MoveTo(aRequest.GetVisualScrollOffset());

  CSSRect scrollableRect(0, 0, 0, 0);
  if (!mRenderFrame || !mRenderFrame->GetScrollableRect(scrollableRect)) {
    LOGW("Failed to read root scroll frame scrollable rect");
  }

  LOGT("contentR[%g, %g, %g, %g], scrSize[%g, %g, %g, %g]",
        contentRect.x, contentRect.y, contentRect.width, contentRect.height,
        scrollableRect.x, scrollableRect.y, scrollableRect.width, scrollableRect.height);
  gfxRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  gfxSize size(scrollableRect.width, scrollableRect.height);

  if (mRenderFrame && !GetListener()->HandleScrollEvent(rect, size)) {
    Unused << mRenderFrame->SendHandleScrollEvent(rect, size);
  }
}

nsIntPoint EmbedContentController::convertIntPoint(const LayoutDevicePoint &aPoint)
{
  return nsIntPoint((int)nearbyint(aPoint.x), (int)nearbyint(aPoint.y));
}

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
  LOGT();
  MessageLoop::current()->PostDelayedTask(std::move(aTask), aDelayMs);
}

EmbedLiteViewListener *EmbedContentController::GetListener() const
{
  static FakeListener sFakeListener;
  return mRenderFrame && mRenderFrame->mView ?
         mRenderFrame->mView->GetListener() : &sFakeListener;
}

void EmbedContentController::DoRequestContentRepaint(const layers::RepaintRequest aRequest)
{
  LOGT("render frame %p", mRenderFrame);
  if (mRenderFrame && !GetListener()->RequestContentRepaint()) {
    DoSendScrollEvent(aRequest);
    Unused << mRenderFrame->SendUpdateFrame(aRequest);
  }
}

void EmbedContentController::NotifyAPZStateChange(const mozilla::layers::ScrollableLayerGuid &aGuid, APZStateChange aChange, int aArg)
{
  LOGT();
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<const mozilla::layers::ScrollableLayerGuid &, APZStateChange, int>("mozilla::embedlite::EmbedContentController::NotifyAPZStateChange",
                                                                                                             this,
                                                                                                             &EmbedContentController::NotifyAPZStateChange,
                                                                                                             aGuid,
                                                                                                             aChange,
                                                                                                             aArg));
    return;
  }

  LOGT("render frame: %p", mRenderFrame);
  if (mRenderFrame) {
    Unused << mRenderFrame->SendNotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void EmbedContentController::NotifyFlushComplete()
{
  LOGT();
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod("mozilla::embedlite::EmbedContentController::NotifyFlushComplete",
                                          this,
                                          &EmbedContentController::NotifyFlushComplete));
    return;
  }

  if (mRenderFrame) {
    Unused << mRenderFrame->SendNotifyFlushComplete();
  }
}


void EmbedContentController::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType, const EmbedContentController::ScrollableLayerGuid& aGuid, const LayoutDevicePoint &aFocusPoint, LayoutDeviceCoord aSpanChange, Modifiers aModifiers)
{
  LOGT("NOT YET IMPLEMENTED");
}

bool EmbedContentController::IsRepaintThread()
{
  return MessageLoop::current() == mUILoop;
}

void EmbedContentController::NotifyAsyncScrollbarDragRejected(const ScrollableLayerGuid::ViewID &aViewId)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::NotifyAsyncAutoscrollRejected(const ScrollableLayerGuid::ViewID &aViewId)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::CancelAutoscroll(const EmbedContentController::ScrollableLayerGuid &aGuid)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  mUILoop->PostTask(std::move(aTask));
}

uint32_t EmbedContentController::GetUniqueID() const
{
  uint32_t id = 0;
  if (mRenderFrame) {
    mRenderFrame->GetUniqueID(& id);
  }
  return id;
}
