/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteViewThreadParent"
#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteView.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"

#include "EmbedLiteCompositorParent.h"
#include "mozilla/unused.h"
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/GeckoContentController.h"

#include "EmbedLiteRenderTarget.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "ClientLayerManager.h"         // for ClientLayerManager, etc

#include "BasicLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "gfxUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::gl;

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

/**
 * Currently EmbedAsyncPanZoomController is needed because we need to do extra steps when panning ends.
 * This is not optimal way to implement HandlePanEnd as AsyncPanZoomController
 * invokes GeckoContentController::HandlePanEnd and overridden EmbedContentController::HandlePanEnd invokes
 * a method of EmbedAsyncPanZoomController so that we can call protected methods of ASyncPanZoomController.
 * There could be a virtual method that would allow us to do the same thing.
 *
 * Regardless of above, this helps us to keep AsyncPanZoomZontroller clean from
 * embedlite specifc fixes.
 */
class EmbedAsyncPanZoomController : public AsyncPanZoomController
{
  public:
    EmbedAsyncPanZoomController(uint64_t aLayersId,
                                GeckoContentController* aGeckoContentController,
                                GestureBehavior aGestures)
      : AsyncPanZoomController(aLayersId, nullptr, aGeckoContentController, aGestures)
    { }

    void HandlePanEnd() {
        ScheduleComposite();
        RequestContentRepaint();
    }
};

class EmbedContentController : public GeckoContentController
{
  public:
    EmbedContentController(EmbedLiteViewThreadParent* aRenderFrame)
      : mUILoop(MessageLoop::current())
      , mRenderFrame(aRenderFrame)
      , mAsyncPanZoomController(0)
    { }

    virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE {
      // We always need to post requests into the "UI thread" otherwise the
      // requests may get processed out of order.
      LOGT();
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &EmbedContentController::DoRequestContentRepaint,
      aFrameMetrics));
    }

    virtual void HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::HandleDoubleTap, aPoint, aModifiers));
        return;
      }
      EmbedLiteViewListener* listener = GetListener();
      if (listener && !listener->HandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y))) {
        unused << mRenderFrame->SendHandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y));
      }
    }

    virtual void HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::HandleSingleTap, aPoint, aModifiers));
        return;
      }
      EmbedLiteViewListener* listener = GetListener();
      if (listener && !listener->HandleSingleTap(nsIntPoint(aPoint.x, aPoint.y))) {
        unused << mRenderFrame->SendHandleSingleTap(nsIntPoint(aPoint.x, aPoint.y));
      }
    }

    virtual void HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::HandleLongTap, aPoint, aModifiers));
        return;
      }
      EmbedLiteViewListener* listener = GetListener();
      if (listener && !listener->HandleLongTap(nsIntPoint(aPoint.x, aPoint.y))) {
        unused << mRenderFrame->SendHandleLongTap(nsIntPoint(aPoint.x, aPoint.y));
      }
    }

    virtual void HandlePanEnd() MOZ_OVERRIDE {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::HandlePanEnd));
        return;
      }

      if (mAsyncPanZoomController) {
          mAsyncPanZoomController->HandlePanEnd();
      }
    }

    /**
     * Requests sending a mozbrowserasyncscroll domevent to embedder.
     * |aContentRect| is in CSS pixels, relative to the current cssPage.
     * |aScrollableSize| is the current content width/height in CSS pixels.
     */
    virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                         const CSSRect& aContentRect,
                                         const CSSSize& aScrollableSize) {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::SendAsyncScrollDOMEvent,
                            aIsRoot, aContentRect, aScrollableSize));
        return;
      }
      LOGNI("contentR[%g,%g,%g,%g], scrSize[%g,%g]",
            aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height,
            aScrollableSize.width, aScrollableSize.height);
      gfxRect rect(aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height);
      gfxSize size(aScrollableSize.width, aScrollableSize.height);
      EmbedLiteViewListener* listener = GetListener();
      if (listener && !listener->SendAsyncScrollDOMEvent(rect, size)) {
        unused << mRenderFrame->SendAsyncScrollDOMEvent(rect, size);
      }
    }

    virtual void ScrollUpdate(const CSSPoint& aPosition, const float aResolution)
    {
      if (MessageLoop::current() != mUILoop) {
        // We have to send this message from the "UI thread" (main
        // thread).
        mUILoop->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &EmbedContentController::ScrollUpdate,
                            aPosition, aResolution));
        return;
      }
      EmbedLiteViewListener* listener = GetListener();
      if (mRenderFrame) {
        mRenderFrame->UpdateLastResolution(aResolution);
      }
      if (listener) {
        listener->ScrollUpdate(gfxPoint(aPosition.x, aPosition.y), aResolution);
      }
    }

    void ClearRenderFrame() {
      mRenderFrame = nullptr;
    }

    /**
     * Schedules a runnable to run on the controller/UI thread at some time
     * in the future.
     */
    virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE
    {
      MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
    }

    void SetAsyncPanZoomController(EmbedAsyncPanZoomController* aEmbedAsyncPanZoomController) {
      mAsyncPanZoomController = aEmbedAsyncPanZoomController;
    }

  private:
    EmbedLiteViewListener* GetListener() {
      return mRenderFrame && mRenderFrame->mView ?
             mRenderFrame->mView->GetListener() : nullptr;
    }

    void DoRequestContentRepaint(const FrameMetrics& aFrameMetrics) {
      EmbedLiteViewListener* listener = GetListener();
      if (listener && !listener->RequestContentRepaint()) {
        unused << mRenderFrame->SendUpdateFrame(aFrameMetrics);
      }
    }

    MessageLoop* mUILoop;
    EmbedLiteViewThreadParent* mRenderFrame;
    EmbedAsyncPanZoomController* mAsyncPanZoomController;
};

EmbedLiteViewThreadParent::EmbedLiteViewThreadParent(const uint32_t& id, const uint32_t& parentId)
  : mId(id)
  , mView(EmbedLiteApp::GetInstance()->GetViewByID(id))
  , mViewAPIDestroyed(false)
  , mCompositor(nullptr)
  , mScrollOffset(0, 0)
  , mLastScale(1.0f)
  , mInTouchProcess(false)
  , mUILoop(MessageLoop::current())
  , mLastIMEState(0)
  , mLastResolution(1.0f)
  , mUploadTexture(0)
{
  MOZ_COUNT_CTOR(EmbedLiteViewThreadParent);
  MOZ_ASSERT(mView, "View destroyed during OMTC view construction");
  mView->SetImpl(this);
  mGeckoController = new EmbedContentController(this);
}

EmbedLiteViewThreadParent::~EmbedLiteViewThreadParent()
{
  MOZ_COUNT_DTOR(EmbedLiteViewThreadParent);
  LOGT("mView:%p, mGeckoController:%p, mController:%p, mCompositor:%p", mView, mGeckoController.get(), mController.get(), mCompositor.get());
  bool mHadCompositor = mCompositor.get() != nullptr;
  if (mGeckoController) {
    mGeckoController->ClearRenderFrame();
    mGeckoController->SetAsyncPanZoomController(0);
  }
  if (mController) {
    mController->SetCompositorParent(nullptr);
    mController->Destroy();
    mController = nullptr;
  }

  if (mView) {
    mView->SetImpl(NULL);
  }
  // If we haven't had compositor created, then noone will notify app that view destroyed
  // Let's do it here
  if (!mHadCompositor) {
    EmbedLiteApp::GetInstance()->ViewDestroyed(mId);
  }
}

void
EmbedLiteViewThreadParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mGeckoController) {
    mGeckoController->ClearRenderFrame();
    mGeckoController->SetAsyncPanZoomController(0);
  }
  if (mController) {
    mController->Destroy();
    mController = nullptr;
  }
}

void
EmbedLiteViewThreadParent::SetCompositor(EmbedLiteCompositorParent* aCompositor)
{
  LOGT();
  mCompositor = aCompositor;
  UpdateScrollController();
  if (mCompositor)
    mCompositor->SetSurfaceSize(mGLViewPortSize.width, mGLViewPortSize.height);
}

void
EmbedLiteViewThreadParent::UpdateScrollController()
{
  mController = nullptr;
  if (mViewAPIDestroyed) {
    return;
  }

  NS_ENSURE_TRUE(mView, );
  if (mView->GetPanZoomControlType() != EmbedLiteView::PanZoomControlType::EXTERNAL) {
    AsyncPanZoomController::GestureBehavior type;
    if (mView->GetPanZoomControlType() == EmbedLiteView::PanZoomControlType::GECKO_SIMPLE) {
      type = AsyncPanZoomController::DEFAULT_GESTURES;
    } else if (mView->GetPanZoomControlType() == EmbedLiteView::PanZoomControlType::GECKO_TOUCH) {
      type = AsyncPanZoomController::USE_GESTURE_DETECTOR;
    } else {
      return;
    }
#warning "Maybe Need to switch to APZCTreeManager"
    mController = new EmbedAsyncPanZoomController(0, mGeckoController, type);
    mController->SetCompositorParent(mCompositor);
    mController->UpdateCompositionBounds(ScreenIntRect(0, 0, mViewSize.width, mViewSize.height));
    mGeckoController->SetAsyncPanZoomController(mController);
  }
}

AsyncPanZoomController*
EmbedLiteViewThreadParent::GetDefaultPanZoomController()
{
  return mController;
}

// Child notification

bool
EmbedLiteViewThreadParent::RecvInitialized()
{
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->ViewInitialized();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLocationChanged(const nsCString& aLocation,
                                                 const bool& aCanGoBack,
                                                 const bool& aCanGoForward)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLocationChanged(aLocation.get(), aCanGoBack, aCanGoForward);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadStarted(const nsCString& aLocation)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadStarted(aLocation.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadFinished()
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadFinished();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadRedirect()
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadRedirect();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadProgress(const int32_t& aProgress, const int32_t& aCurTotal, const int32_t& aMaxTotal)
{
  LOGNI("progress:%i", aProgress);
  NS_ENSURE_TRUE(mView, true);
  if (mViewAPIDestroyed) {
    return true;
  }

  mView->GetListener()->OnLoadProgress(aProgress, aCurTotal, aMaxTotal);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnSecurityChanged(const nsCString& aStatus,
                                                 const uint32_t& aState)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnSecurityChanged(aStatus.get(), aState);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnFirstPaint(const int32_t& aX,
                                            const int32_t& aY)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnFirstPaint(aX, aY);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnScrolledAreaChanged(const uint32_t& aWidth,
                                                     const uint32_t& aHeight)
{
  LOGNI("area[%u,%u]", aWidth, aHeight);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnScrolledAreaChanged(aWidth, aHeight);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnScrollChanged(const int32_t& offSetX,
                                               const int32_t& offSetY)
{
  LOGNI("off[%i,%i]", offSetX, offSetY);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnScrollChanged(offSetX, offSetY);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnTitleChanged(const nsString& aTitle)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  mView->GetListener()->OnTitleChanged(aTitle.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvUpdateZoomConstraints(const bool& val, const float& min, const float& max)
{
  if (mController) {
    mController->UpdateZoomConstraints(val, CSSToScreenScale(min), CSSToScreenScale(max));
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvUpdateScrollOffset(const uint32_t& aPresShellId,
                                  const ViewID& aViewId,
                                  const CSSIntPoint& aScrollOffset)
{
  if (mController) {
    // TODO: currently aPresShelId and aViewId aren't used (But they're used in TabChild to dispatch many APZCs).
    mController->UpdateScrollOffset(aScrollOffset);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvZoomToRect(const CSSRect& aRect)
{
  if (mController) {
    mController->ZoomToRect(aRect);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvCancelDefaultPanZoom()
{
  if (mController && mInTouchProcess) {
    mController->CancelDefaultPanZoom();
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvContentReceivedTouch(const bool& aPreventDefault)
{
  if (mController) {
    mController->ContentReceivedTouch(aPreventDefault);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvDetectScrollableSubframe()
{
  if (mController) {
    mController->DetectScrollableSubframe();
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSetBackgroundColor(const nscolor& aColor)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->SetBackgroundColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
  return true;
}

// Incoming API calls

void
EmbedLiteViewThreadParent::LoadURL(const char* aUrl)
{
  LOGT("url:%s", aUrl);
  unused << SendLoadURL(NS_ConvertUTF8toUTF16(nsDependentCString(aUrl)));
}

void EmbedLiteViewThreadParent::GoBack()
{
  unused << SendGoBack();
}

void EmbedLiteViewThreadParent::GoForward()
{
  unused << SendGoForward();
}

void EmbedLiteViewThreadParent::StopLoad()
{
  unused << SendStopLoad();
}

void EmbedLiteViewThreadParent::Reload(bool hardReload)
{
  unused << SendReload(hardReload);
}

void
EmbedLiteViewThreadParent::SetIsActive(bool aIsActive)
{
  LOGF();
  unused << SendSetIsActive(aIsActive);
}

void
EmbedLiteViewThreadParent::SetIsFocused(bool aIsFocused)
{
  LOGF();
  unused << SendSetIsFocused(aIsFocused);
}

void
EmbedLiteViewThreadParent::SuspendTimeouts()
{
  LOGF();
  unused << SendSuspendTimeouts();
}

void
EmbedLiteViewThreadParent::ResumeTimeouts()
{
  LOGF();
  unused << SendResumeTimeouts();
}

void
EmbedLiteViewThreadParent::LoadFrameScript(const char* aURI)
{
  LOGT("uri:%s", aURI);
  unused << SendLoadFrameScript(NS_ConvertUTF8toUTF16(nsDependentCString(aURI)));
}

void
EmbedLiteViewThreadParent::DoSendAsyncMessage(const PRUnichar* aMessageName, const PRUnichar* aMessage)
{
  LOGT("msgName:%ls, msg:%ls", aMessageName, aMessage);
  const nsDependentString msgname(aMessageName);
  const nsDependentString msg(aMessage);
  unused << SendAsyncMessage(msgname,
                             msg);
}

void
EmbedLiteViewThreadParent::AddMessageListener(const char* aMessageName)
{
  LOGT("msgName:%s", aMessageName);
  unused << SendAddMessageListener(nsDependentCString(aMessageName));
}

void
EmbedLiteViewThreadParent::RemoveMessageListener(const char* aMessageName)
{
  LOGT("msgName:%s", aMessageName);
  unused << SendRemoveMessageListener(nsDependentCString(aMessageName));
}

void
EmbedLiteViewThreadParent::AddMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  unused << SendAddMessageListeners(aMessageNames);
}

void
EmbedLiteViewThreadParent::RemoveMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  unused << SendRemoveMessageListeners(aMessageNames);
}

bool
EmbedLiteViewThreadParent::RecvAsyncMessage(const nsString& aMessage,
                                            const nsString& aData)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  LOGF("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->RecvAsyncMessage(aMessage.get(), aData.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSyncMessage(const nsString& aMessage,
                                           const nsString& aJSON,
                                           InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aJSON).get());
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  char* retval =
    mView->GetListener()->
    RecvSyncMessage(aMessage.get(), aJSON.get());
  if (retval) {
    aJSONRetVal->AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(retval)));
    delete retval;
  }
  return true;
}

static inline gfxASurface::gfxImageFormat
_depth_to_gfxformat(int depth)
{
  switch (depth) {
    case 32:
      return gfxASurface::ImageFormatARGB32;
    case 24:
      return gfxASurface::ImageFormatRGB24;
    case 16:
      return gfxASurface::ImageFormatRGB16_565;
    default:
      return gfxASurface::ImageFormatUnknown;
  }
}

bool
EmbedLiteViewThreadParent::RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth)
{
  LOGF("d:%p, sz[%i,%i], stride:%i, depth:%i", aData, imgW, imgH, stride, depth);
  if (mCompositor) {
    nsRefPtr<gfxImageSurface> source =
      new gfxImageSurface(aData, gfxIntSize(imgW, imgH), stride, _depth_to_gfxformat(depth));
    {
      nsRefPtr<gfxContext> context = new gfxContext(source);
      return mCompositor->RenderToContext(context);
    }
  }
  return false;
}

bool
EmbedLiteViewThreadParent::RenderGL()
{
  if (mCompositor) {
    return mCompositor->RenderGL();
  }
  return false;
}

bool
EmbedLiteViewThreadParent::ScrollBy(int aDX, int aDY, bool aDoOverflow)
{
  LOGT("d[%i,%i]", aDX, aDY);
  mScrollOffset.MoveBy(-aDX, -aDY);

  return true;
}

void
EmbedLiteViewThreadParent::SetViewSize(int width, int height)
{
  LOGT("sz[%i,%i]", width, height);
  mViewSize = gfxSize(width, height);
  unused << SendSetViewSize(mViewSize);
  if (mController) {
    mController->UpdateCompositionBounds(ScreenIntRect(0, 0, width, height));
  }
}

bool
EmbedLiteViewThreadParent::RecvGetGLViewSize(gfxSize* aSize)
{
  *aSize = mGLViewPortSize;
  return true;
}

void
EmbedLiteViewThreadParent::SetGLViewPortSize(int width, int height)
{
  mGLViewPortSize = gfxSize(width, height);
  if (mCompositor) {
    mCompositor->SetSurfaceSize(width, height);
  }
  unused << SendSetGLViewSize(mGLViewPortSize);
}

void
EmbedLiteViewThreadParent::SetGLViewTransform(gfxMatrix matrix)
{
  if (mCompositor) {
    mCompositor->SetWorldTransform(matrix);
  }
}

void
EmbedLiteViewThreadParent::SetViewClipping(const gfxRect& aClipRect)
{
  if (mCompositor) {
    mCompositor->SetClipping(aClipRect);
  }
}

void
EmbedLiteViewThreadParent::SetViewOpacity(const float aOpacity)
{
  if (mCompositor) {
    mCompositor->SetWorldOpacity(aOpacity);
  }
}

void
EmbedLiteViewThreadParent::SetTransformation(float aScale, nsIntPoint aScrollOffset)
{
}

void
EmbedLiteViewThreadParent::ScheduleRender()
{
  if (mCompositor) {
    mCompositor->ScheduleRenderOnCompositorThread();
  }
}

void
EmbedLiteViewThreadParent::ReceiveInputEvent(const InputData& aEvent)
{
  if (mController) {
    mController->ReceiveInputEvent(aEvent);
    if (aEvent.mInputType == MULTITOUCH_INPUT) {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      mozilla::CSSToScreenScale sz(mLastResolution, mLastResolution);
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_START ||
          multiTouchInput.mType == MultiTouchInput::MULTITOUCH_ENTER) {
        mInTouchProcess = true;
      } else if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_END ||
                 multiTouchInput.mType == MultiTouchInput::MULTITOUCH_LEAVE) {
        mInTouchProcess = false;
      }
      gfxPoint diff = mController->GetTempScrollOffset();
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_MOVE) {
        unused << SendInputDataTouchMoveEvent(multiTouchInput, gfxSize(sz.scale, sz.scale), diff);
      } else {
        unused << SendInputDataTouchEvent(multiTouchInput, gfxSize(sz.scale, sz.scale), diff);
      }
    }
  }
}

void
EmbedLiteViewThreadParent::TextEvent(const char* composite, const char* preEdit)
{
  LOGT("commit:%s, pre:%s, mLastIMEState:%i", composite, preEdit, mLastIMEState);
  if (mLastIMEState) {
    unused << SendHandleTextEvent(NS_ConvertUTF8toUTF16(nsDependentCString(composite)),
                                  NS_ConvertUTF8toUTF16(nsDependentCString(preEdit)));
  } else {
    NS_ERROR("Text event must not be sent while IME disabled");
  }
}

void
EmbedLiteViewThreadParent::ViewAPIDestroyed()
{
  mViewAPIDestroyed = true;
  mView = nullptr;
}

void
EmbedLiteViewThreadParent::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyPressEvent(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteViewThreadParent::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyReleaseEvent(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteViewThreadParent::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_START, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event);
    unused << SendMouseEvent(NS_LITERAL_STRING("mousedown"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

void
EmbedLiteViewThreadParent::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_END, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event);
    unused << SendMouseEvent(NS_LITERAL_STRING("mouseup"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

void
EmbedLiteViewThreadParent::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_MOVE, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event);
    unused << SendMouseEvent(NS_LITERAL_STRING("mousemove"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

bool
EmbedLiteViewThreadParent::RecvGetInputContext(int32_t* aIMEEnabled,
                                               int32_t* aIMEOpen,
                                               intptr_t* aNativeIMEContext)
{
  LOGT("mLastIMEState:%i", mLastIMEState);
  *aIMEEnabled = mLastIMEState;
  *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  *aNativeIMEContext = 0;
  if (mView) {
    mView->GetListener()->GetIMEStatus(aIMEEnabled, aIMEOpen, aNativeIMEContext);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSetInputContext(const int32_t& aIMEEnabled,
                                               const int32_t& aIMEOpen,
                                               const nsString& aType,
                                               const nsString& aInputmode,
                                               const nsString& aActionHint,
                                               const int32_t& aCause,
                                               const int32_t& aFocusChange)
{
  LOGT("IMEEnabled:%i, IMEOpen:%i, type:%s, imMode:%s, actHint:%s, cause:%i, focusChange:%i, mLastIMEState:%i->%i",
       aIMEEnabled, aIMEOpen, NS_ConvertUTF16toUTF8(aType).get(), NS_ConvertUTF16toUTF8(aInputmode).get(),
       NS_ConvertUTF16toUTF8(aActionHint).get(), aCause, aFocusChange, mLastIMEState, aIMEEnabled);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mLastIMEState = aIMEEnabled;
  mView->GetListener()->IMENotification(aIMEEnabled, aIMEOpen, aCause, aFocusChange, aType.get(), aInputmode.get());
  return true;
}

uint32_t
EmbedLiteViewThreadParent::GetUniqueID()
{
  return mId;
}

void EmbedLiteViewThreadParent::UpdateLastResolution(const float aResolution)
{
  mLastResolution = aResolution;
}

bool EmbedLiteViewThreadParent::GetPendingTexture(EmbedLiteRenderTarget* aContextWrapper, int* textureID, int* width, int* height)
{
  NS_ENSURE_TRUE(aContextWrapper && textureID && width && height, false);
  NS_ENSURE_TRUE(mCompositor, false);

  LayerManagerComposite* mgr = mCompositor->GetLayerManager();
  NS_ENSURE_TRUE(mgr, false);

  GLContext* context = static_cast<CompositorOGL*>(mgr->GetCompositor())->gl();
  NS_ENSURE_TRUE(context && context->IsOffscreen(), false);

  GLContext* consumerContext = aContextWrapper->GetConsumerContext();
  NS_ENSURE_TRUE(consumerContext && consumerContext->Init(), false);

  SharedSurface* sharedSurf = context->RequestFrame();
  NS_ENSURE_TRUE(sharedSurf, false);

  gfxImageSurface* toUpload = nullptr;
  GLint textureHandle = 0;
  if (sharedSurf->Type() == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = SharedSurface_EGLImage::Cast(sharedSurf);
    textureHandle = eglImageSurf->AcquireConsumerTexture(consumerContext);
    if (!textureHandle) {
      NS_WARNING("Failed to get texture handle, fallback to pixels?");
      toUpload = eglImageSurf->GetPixels();
    }
  } else if (sharedSurf->Type() == SharedSurfaceType::GLTextureShare) {
    SharedSurface_GLTexture* glTexSurf = SharedSurface_GLTexture::Cast(sharedSurf);
    glTexSurf->SetConsumerGL(consumerContext);
    textureHandle = glTexSurf->Texture();
    NS_ASSERTION(textureHandle, "Failed to get texture handle, fallback to pixels?");
  } else if (sharedSurf->Type() == SharedSurfaceType::Basic) {
    toUpload = SharedSurface_Basic::Cast(sharedSurf)->GetData();
  } else {
    NS_ERROR("Unhandled Image type");
  }

  if (toUpload) {
    // mBounds seems to end up as (0,0,0,0) a lot, so don't use it?
    nsIntSize size(toUpload->GetSize());
    nsIntRect rect(nsIntPoint(0,0), size);
    nsIntRegion bounds(rect);
    consumerContext->UploadSurfaceToTexture(toUpload,
                                            bounds,
                                            mUploadTexture,
                                            true);
    textureHandle = mUploadTexture;
  } else if (textureHandle) {
    if (consumerContext) {
      MOZ_ASSERT(consumerContext);
      if (consumerContext->MakeCurrent()) {
        consumerContext->fDeleteTextures(1, &mUploadTexture);
      }
    }
  }

  NS_ASSERTION(textureHandle, "Failed to get texture handle from EGLImage, fallback to pixels?");

  *width = sharedSurf->Size().width;
  *height = sharedSurf->Size().height;
  *textureID = textureHandle;
  return true;
}

} // namespace embedlite
} // namespace mozilla
