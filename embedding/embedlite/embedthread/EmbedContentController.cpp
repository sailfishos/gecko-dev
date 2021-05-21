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
  : mUIThread(aUIThread)
  , mRenderFrame(aRenderFrame)
{
}

EmbedContentController::~EmbedContentController()
{
  LOGT();
}

void EmbedContentController::RequestContentRepaint(const layers::RepaintRequest &aRequest)
{
  // We always need to post requests into the "UI thread" otherwise the
  // requests may get processed out of order.
  LOGT();
  // nsThreadUtils version
  mUIThread->Dispatch(NewRunnableMethod<const layers::RepaintRequest>("mozilla::embedlite::EmbedContentController::DoRequestContentRepaint",
                                                                      this,
                                                                      &EmbedContentController::DoRequestContentRepaint,
                                                                      aRequest));
}

void EmbedContentController::NotifyLayerTransforms(const nsTArray<layers::MatrixMessage> &aTransforms)
{
  LOGT("NOT YET IMPLEMENTED");
}

void EmbedContentController::NotifyAsyncScrollbarDragInitiated(uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID &aScrollId, layers::ScrollDirection aDirection)
{
  LOGT("NOT YET IMPLEMENTED");
}

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
      [[fallthrough]];
    case GeckoContentController::TapType::eLongTapUp:
      break;
  }
}

void EmbedContentController::HandleDoubleTap(const LayoutDevicePoint aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid aGuid)
{
  if (NS_GetCurrentThread() != mUIThread) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUIThread->Dispatch(NewRunnableMethod<const LayoutDevicePoint, Modifiers, const ScrollableLayerGuid>("mozilla::embedlite::EmbedContentController::HandleDoubleTap",
                                                                                                       this,
                                                                                                       &EmbedContentController::HandleDoubleTap,
                                                                                                       aPoint,
                                                                                                       aModifiers,
                                                                                                       aGuid));
  } else if (mRenderFrame && !GetListener()->HandleDoubleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleDoubleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleSingleTap(const LayoutDevicePoint aPoint,
                                             Modifiers aModifiers,
                                             const ScrollableLayerGuid aGuid)
{
  if (NS_GetCurrentThread() != mUIThread) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUIThread->Dispatch(NewRunnableMethod<const LayoutDevicePoint, Modifiers, const ScrollableLayerGuid>("mozilla::embedlite::EmbedContentController::HandleSingleTap",
                                                                                                       this,
                                                                                                       &EmbedContentController::HandleSingleTap,
                                                                                                       aPoint,
                                                                                                       aModifiers,
                                                                                                       aGuid));
  } else if (mRenderFrame && !GetListener()->HandleSingleTap(convertIntPoint(aPoint))) {
    Unused << mRenderFrame->SendHandleSingleTap(aPoint, aModifiers, aGuid);
  }
}

void EmbedContentController::HandleLongTap(const LayoutDevicePoint aPoint,
                                           Modifiers aModifiers,
                                           const ScrollableLayerGuid aGuid,
                                           uint64_t aInputBlockId)
{
  if (NS_GetCurrentThread() != mUIThread) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUIThread->Dispatch(NewRunnableMethod<const LayoutDevicePoint, Modifiers, const ScrollableLayerGuid, uint64_t>("mozilla::embedlite::EmbedContentController::HandleLongTap",
                                                                                                                 this,
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
void EmbedContentController::DoSendScrollEvent(const layers::RepaintRequest aRequest)
{
  if (NS_GetCurrentThread() != mUIThread) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUIThread->Dispatch(NewRunnableMethod<const layers::RepaintRequest>("mozilla::embedlite::EmbedContentController::DoSendScrollEvent",
                                                                        this,
                                                                        &EmbedContentController::DoSendScrollEvent,
                                                                        aRequest));
    return;
  } else {
    CSSRect contentRect = (aRequest.GetZoom() == CSSToParentLayerScale2D(0, 0)) ? CSSRect() : (aRequest.GetCompositionBounds() / aRequest.GetZoom());
    contentRect.MoveTo(aRequest.GetScrollOffset());

    // FIXME - RepaintRequest does not contain scrollable rect size.
    CSSSize scrollableSize(0, 0);

    LOGNI("contentR[%g,%g,%g,%g], scrSize[%g,%g]",
          contentRect.x, contentRect.y, contentRect.width, contentRect.height,
          scrollableSize.width, scrollableSize.height);
    gfxRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
    gfxSize size(scrollableSize.width, scrollableSize.height);

    if (mRenderFrame && !GetListener()->HandleScrollEvent(aRequest.IsRootContent(), rect, size)) {
      Unused << mRenderFrame->SendHandleScrollEvent(aRequest.IsRootContent(), rect, size);
    }
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
  NS_GetCurrentThread()->DelayedDispatch(std::move(aTask), aDelayMs);
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
  mUIThread->Dispatch(NewRunnableMethod<const mozilla::layers::ScrollableLayerGuid &, APZStateChange, int>("mozilla::embedlite::EmbedContentController::DoNotifyAPZStateChange",
                                                                                                         this,
                                                                                                         &EmbedContentController::DoNotifyAPZStateChange,
                                                                                                         aGuid,
                                                                                                         aChange,
                                                                                                         aArg));
}

void EmbedContentController::NotifyFlushComplete()
{
  LOGT();
  mUIThread->Dispatch(NewRunnableMethod("mozilla::embedlite::EmbedContentController::DoNotifyFlushComplete",
                                      this,
                                      &EmbedContentController::DoNotifyFlushComplete));
}

void EmbedContentController::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType, const EmbedContentController::ScrollableLayerGuid &aGuid, LayoutDeviceCoord aSpanChange, Modifiers aModifiers)
{
  LOGT("NOT YET IMPLEMENTED");
}

bool EmbedContentController::IsRepaintThread()
{
  return NS_GetCurrentThread() == mUIThread;
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
  mUIThread->Dispatch(std::move(aTask));
}
