/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteWindow.h"
#include "EmbedLiteWindowParent.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsThreadUtils.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SurfaceTypes.h"               // for SurfaceStreamType

using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace embedlite {

class PlatformFrameReadyEvent final : public wr::RendererEvent
{
public:
  explicit PlatformFrameReadyEvent(
      EmbedLiteCompositorBridgeParent* aCompositor)
    : mCompositor(aCompositor)
  {
  }

  void Run(wr::RenderThread&, wr::WindowId) override
  {
    mCompositor->NotifyLatestPlatformFrameReady();
  }

  const char* Name() override
  {
    return "PlatformFrameReadyEvent";
  }

private:
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;
};

class PlatformFrameRetirementEvent final : public wr::RendererEvent
{
public:
  explicit PlatformFrameRetirementEvent(
      EmbedLiteCompositorBridgeParent* aCompositor)
    : mCompositor(aCompositor)
  {
  }

  void Run(wr::RenderThread&, wr::WindowId) override
  {
    mCompositor->RetireReleasedPlatformFrames();
  }

  const char* Name() override
  {
    return "PlatformFrameRetirementEvent";
  }

private:
  RefPtr<EmbedLiteCompositorBridgeParent> mCompositor;
};

EmbedLiteCompositorBridgeParent::EmbedLiteCompositorBridgeParent(uint32_t windowId,
                                                                 CompositorManagerParent* aManager,
                                                                 uint32_t aNamespace,
                                                                 CSSToLayoutDeviceScale aScale,
                                                                 const TimeDuration &aVsyncRate,
                                                                 const CompositorOptions &aOptions,
                                                                 bool aRenderToEGLSurface,
                                                                 const gfx::IntSize &aSurfaceSize,
                                                                 uint64_t aInnerWindowId)
  : CompositorBridgeParent(aManager, aNamespace, aScale, aVsyncRate,
                           aOptions, aRenderToEGLSurface, aSurfaceSize,
                           aInnerWindowId)
  , mWindowId(windowId)
  , mCurrentCompositeTask(nullptr)
  , mSurfaceOrigin(0, 0)
  , mRenderMutex("EmbedLiteCompositorBridgeParent render mutex")
  , mPlatformImageCallbackMutex(
      "EmbedLiteCompositorBridgeParent platform image callback mutex")
  , mPlatformImageMutex("EmbedLiteCompositorBridgeParent platform image mutex")
  , mPlatformImageGeneration(1)
  , mNextPlatformFrameSequence(0)
  , mLatestPlatformFrameToken({0, 0})
  , mPlatformFrameListener(nullptr)
  , mPlatformFrameDeliveryEnabled(false)
  , mPlatformFrameDeliveryStopPending(false)
  , mPlatformImageRetryPending(false)
  , mPlatformFrameRetirementPending(false)
{
  LOGT("EmbedLiteCompositorBridgeParent::EmbedLiteCompositorBridgeParent");
  if (mWindowId == 0) {
    mWindowId = EmbedLiteWindowParent::Current();
  }
  EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId);
  LOGT("this:%p, window:%p, sz[%i,%i]", this, parentWindow, aSurfaceSize.width, aSurfaceSize.height);

  parentWindow->SetCompositor(this);
  parentWindow->GetListener()->CompositorCreated();

  // Post open parent?
  //
}

EmbedLiteCompositorBridgeParent::~EmbedLiteCompositorBridgeParent()
{
  LOGT("EmbedLiteCompositorBridgeParent::~EmbedLiteCompositorBridgeParent");
  MOZ_ASSERT(!mPlatformFrameDeliveryEnabled);
  MOZ_ASSERT(mPlatformFrames.empty());
  LOGT();
}

void
EmbedLiteCompositorBridgeParent::SetWebRenderGLContext(GLContext* aGL)
{
  LOGT("gl:%p", aGL);
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  bool retire = false;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    MutexAutoLock lock(mRenderMutex);
    if (mGLContext != aGL) {
      mFrontBuffer.reset();
      ++mPlatformImageGeneration;
      if (!mPlatformImageGeneration) {
        ++mPlatformImageGeneration;
      }
      mGLContext = aGL;
      retire = !mPlatformFrames.empty();
      MarkReadyPlatformFramesReleased();
    }
  }
  if (retire) {
    RetireReleasedPlatformFrames();
  }
}

void
EmbedLiteCompositorBridgeParent::EnsureSurfaceSizeFromWindow()
{
  EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId);
  if (!parentWindow) {
    return;
  }

  const int width = static_cast<int>(parentWindow->mSize.width);
  const int height = static_cast<int>(parentWindow->mSize.height);
  if (width <= 0 || height <= 0) {
    return;
  }

  MutexAutoLock lock(mRenderMutex);
  if (!mEGLSurfaceSize.IsEmpty()) {
    return;
  }
  mSurfaceOrigin.MoveTo(0, 0);
  SetEGLSurfaceRect(0, 0, width, height);
}

bool
EmbedLiteCompositorBridgeParent::CompositeToDefaultTarget(WebRenderBridgeParent* aWrBridge,
                                                          VsyncId aId,
                                                          wr::RenderReasons aReasons)
{
  aWrBridge->CompositeToTarget(aId, aReasons, nullptr, nullptr);
  return true;
}

bool
EmbedLiteCompositorBridgeParent::PresentOffscreenSurface(
    PlatformFrameToken* aToken)
{
  LOGT("EmbedLiteCompositorBridgeParent::PresentOffscreenSurface");
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  if (mPlatformFrameDeliveryEnabled) {
    RetireReleasedPlatformFrames();
  }
  if (aToken) {
    *aToken = {0, 0};
  }
  RefPtr<GLContext> context;
  uint64_t generation;
  {
    MutexAutoLock lock(mRenderMutex);
    context = mGLContext;
    generation = mPlatformImageGeneration;
  }
  if (!context || !context->Screen()) {
    MutexAutoLock lock(mRenderMutex);
    if (context == mGLContext) {
      mFrontBuffer.reset();
    }
    return false;
  }

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);

  if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
    NS_ERROR("Failed to publish context frame");
    MutexAutoLock lock(mRenderMutex);
    if (context == mGLContext && generation == mPlatformImageGeneration) {
      mFrontBuffer.reset();
    }
    return false;
  }

  std::shared_ptr<SharedSurface> frontBuffer = screen->FrontBuffer();
  SharedSurface* sharedSurf = frontBuffer.get();
  PlatformImageDescriptor descriptor;
  bool hasDescriptor = false;
  PlatformFrameFenceHandleType releaseFenceHandleType =
    PlatformFrameFenceHandleType::NoHandle;
  if (sharedSurf && sharedSurf->mDesc.type == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf =
      static_cast<SharedSurface_EGLImage*>(sharedSurf);
    PlatformImageTextureTarget textureTarget;
    switch (eglImageSurf->EmbedderTextureTarget()) {
      case LOCAL_GL_TEXTURE_2D:
        textureTarget = PlatformImageTextureTarget::Texture2D;
        break;
      case LOCAL_GL_TEXTURE_EXTERNAL:
        textureTarget = PlatformImageTextureTarget::ExternalOES;
        break;
      default:
        NS_WARNING("Unsupported EmbedLite platform image texture target");
        textureTarget = PlatformImageTextureTarget::Texture2D;
        break;
    }
    if (eglImageSurf->EmbedderTextureTarget() == LOCAL_GL_TEXTURE_2D ||
        eglImageSurf->EmbedderTextureTarget() == LOCAL_GL_TEXTURE_EXTERNAL) {
      descriptor = {
        PlatformImageHandleType::EGLImage,
        eglImageSurf->mImage,
        textureTarget,
        sharedSurf->mDesc.size.width,
        sharedSurf->mDesc.size.height
      };
      releaseFenceHandleType = eglImageSurf->SupportsConsumerFence()
        ? PlatformFrameFenceHandleType::EGLSync
        : PlatformFrameFenceHandleType::NoHandle;
      hasDescriptor = true;
    }
  }

  PlatformFrameToken token = {0, 0};
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    MutexAutoLock lock(mRenderMutex);
    if (context != mGLContext || generation != mPlatformImageGeneration) {
      return false;
    }
    mFrontBuffer = frontBuffer;
    if (mPlatformFrameDeliveryEnabled && hasDescriptor) {
      for (auto it = mPlatformFrames.begin(); it != mPlatformFrames.end();) {
        if (it->state == PlatformFrameState::Ready) {
          it = mPlatformFrames.erase(it);
        } else {
          ++it;
        }
      }
      ++mNextPlatformFrameSequence;
      if (!mNextPlatformFrameSequence) {
        ++mNextPlatformFrameSequence;
      }
      token = {mPlatformImageGeneration, mNextPlatformFrameSequence};
      mPlatformFrames.push_back({token, context, frontBuffer, descriptor,
                                 releaseFenceHandleType, nullptr,
                                 PlatformFrameState::Ready});
      mLatestPlatformFrameToken = token;
    }
  }
  if (aToken) {
    *aToken = token;
  }
  return !!frontBuffer;
}

void
EmbedLiteCompositorBridgeParent::WebRenderComposited()
{
  PlatformFrameToken token = {0, 0};
  if (!PresentOffscreenSurface(&token)) {
    return;
  }

  if (EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId)) {
    if (token.IsValid()) {
      NotifyLatestPlatformFrameReady();
    }
    parentWindow->GetListener()->CompositingFinished();
  }
}

bool EmbedLiteCompositorBridgeParent::GetScrollableRect(CSSRect&)
{
  LOGT("EmbedLiteCompositorBridgeParent::GetScrollableRect");
  return true;
}

void EmbedLiteCompositorBridgeParent::SetSurfaceRect(int x, int y, int width, int height)
{
  LOGT("EmbedLiteCompositorBridgeParent::SetSurfaceRect");
  bool retire = false;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    MutexAutoLock lock(mRenderMutex);
    if (width > 0 && height > 0 && (mEGLSurfaceSize.width != width ||
                                    mEGLSurfaceSize.height != height ||
                                    mSurfaceOrigin.x != x ||
                                    mSurfaceOrigin.y != y)) {
      mSurfaceOrigin.MoveTo(x, y);
      SetEGLSurfaceRect(x, y, width, height);
      if (mPlatformFrameDeliveryEnabled) {
        mFrontBuffer.reset();
        ++mPlatformImageGeneration;
        if (!mPlatformImageGeneration) {
          ++mPlatformImageGeneration;
        }
        MarkReadyPlatformFramesReleased();
        retire = true;
      }
    }
  }
  if (retire) {
    SchedulePlatformFrameRetirement();
  }
}

void
EmbedLiteCompositorBridgeParent::SchedulePlatformImageRetry()
{
  if (!mPlatformImageRetryPending.compareExchange(false, true)) {
    return;
  }

  nsISerialEventTarget* compositorThread = CompositorThread();
  if (!compositorThread) {
    mPlatformImageRetryPending = false;
    return;
  }

  RefPtr<EmbedLiteCompositorBridgeParent> self = this;
  RefPtr<Runnable> retry = NS_NewRunnableFunction(
    "EmbedLiteCompositorBridgeParent::SchedulePlatformImageRetry",
    [self]() {
      self->mPlatformImageRetryPending = false;
      if (EmbedLiteWindowParent* parentWindow =
              EmbedLiteWindowParent::From(self->mWindowId)) {
        self->PostLatestPlatformFrameReadyEvent();
        parentWindow->GetListener()->CompositingFinished();
      }
    });
  if (NS_FAILED(compositorThread->DelayedDispatch(retry.forget(), 16))) {
    mPlatformImageRetryPending = false;
  }
}

void
EmbedLiteCompositorBridgeParent::MarkReadyPlatformFramesReleased()
{
  for (PlatformFrameRecord& frame : mPlatformFrames) {
    if (frame.state == PlatformFrameState::Ready) {
      frame.state = PlatformFrameState::Released;
    }
  }
  mLatestPlatformFrameToken = {0, 0};
}

void
EmbedLiteCompositorBridgeParent::PostLatestPlatformFrameReadyEvent()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  WebRenderBridgeParent* wrBridge = GetWrBridge();
  RefPtr<wr::WebRenderAPI> api;
  if (wrBridge) {
    api = wrBridge->GetWebRenderAPI();
  }
  if (!api || !wr::RenderThread::Get()) {
    return;
  }
  wr::RenderThread::Get()->PostEvent(
    api->GetId(), MakeUnique<PlatformFrameReadyEvent>(this));
}

void
EmbedLiteCompositorBridgeParent::NotifyLatestPlatformFrameReady()
{
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  PlatformFrameToken token = {0, 0};
  EmbedLitePlatformFrameListener* listener = nullptr;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    for (const PlatformFrameRecord& frame : mPlatformFrames) {
      if (frame.token == mLatestPlatformFrameToken &&
          frame.state == PlatformFrameState::Ready) {
        token = frame.token;
        listener = mPlatformFrameListener;
        break;
      }
    }
  }
  if (token.IsValid() && listener) {
    listener->PlatformFrameReady(token);
  }
}

bool
EmbedLiteCompositorBridgeParent::SchedulePlatformFrameRetirement()
{
  if (!mPlatformFrameRetirementPending.compareExchange(false, true)) {
    return true;
  }

  nsIThread* compositorThread = CompositorThread();
  if (!compositorThread) {
    mPlatformFrameRetirementPending = false;
    return false;
  }
  if (CompositorThreadHolder::IsInCompositorThread()) {
    PostPlatformFrameRetirementEvent();
    return true;
  }

  nsresult rv = compositorThread->Dispatch(NewRunnableMethod(
    "EmbedLiteCompositorBridgeParent::PostPlatformFrameRetirementEvent",
    this,
    &EmbedLiteCompositorBridgeParent::PostPlatformFrameRetirementEvent));
  if (NS_FAILED(rv)) {
    mPlatformFrameRetirementPending = false;
    return false;
  }
  return true;
}

void
EmbedLiteCompositorBridgeParent::PostPlatformFrameRetirementEvent()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  WebRenderBridgeParent* wrBridge = GetWrBridge();
  RefPtr<wr::WebRenderAPI> api;
  if (wrBridge) {
    api = wrBridge->GetWebRenderAPI();
  }
  if (!api || !wr::RenderThread::Get()) {
    mPlatformFrameRetirementPending = false;
    if (!SchedulePlatformFrameRetirementRetry(100)) {
      NS_WARNING("Failed to retry EmbedLite frame retirement");
    }
    return;
  }
  wr::RenderThread::Get()->PostEvent(
    api->GetId(), MakeUnique<PlatformFrameRetirementEvent>(this));
}

void
EmbedLiteCompositorBridgeParent::RetireReleasedPlatformFrames()
{
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  bool retired = false;
  bool fencePending = false;
  bool deliveryStopped = false;
  EmbedLitePlatformFrameListener* stoppedListener = nullptr;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    for (auto it = mPlatformFrames.begin(); it != mPlatformFrames.end();) {
      if (it->state != PlatformFrameState::Released) {
        ++it;
        continue;
      }
      if (!it->consumerFence) {
        it = mPlatformFrames.erase(it);
        retired = true;
        continue;
      }

      SharedSurface_EGLImage* eglImageSurf =
        static_cast<SharedSurface_EGLImage*>(it->surface.get());
      const EGLSync consumerFence =
        static_cast<EGLSync>(it->consumerFence);
      switch (eglImageSurf->GetConsumerFenceStatus(consumerFence)) {
        case ConsumerFenceStatus::Signaled:
          if (!eglImageSurf->DestroyConsumerFence(consumerFence)) {
            NS_WARNING("Failed to destroy EmbedLite consumer fence");
          }
          it = mPlatformFrames.erase(it);
          retired = true;
          continue;
        case ConsumerFenceStatus::Invalid:
          NS_WARNING("EmbedLite consumer fence became invalid");
          it = mPlatformFrames.erase(it);
          retired = true;
          continue;
        case ConsumerFenceStatus::Pending:
          fencePending = true;
          break;
      }
      ++it;
    }
    if (mPlatformFrameDeliveryStopPending && mPlatformFrames.empty()) {
      mPlatformFrameDeliveryStopPending = false;
      deliveryStopped = true;
      stoppedListener = mPlatformFrameListener;
    }
    mPlatformFrameRetirementPending = false;
  }

  if (retired) {
    NotifyLatestPlatformFrameReady();
  }
  if (deliveryStopped) {
    if (stoppedListener) {
      stoppedListener->PlatformFrameDeliveryStopped();
    }
  }
  if (!fencePending) {
    return;
  }

  if (!SchedulePlatformFrameRetirementRetry(16)) {
    NS_WARNING("Failed to retry EmbedLite frame retirement");
  }
}

bool
EmbedLiteCompositorBridgeParent::SchedulePlatformFrameRetirementRetry(
  uint32_t aDelayMs)
{
  nsIThread* compositorThread = CompositorThread();
  if (!compositorThread) {
    return false;
  }
  RefPtr<EmbedLiteCompositorBridgeParent> self = this;
  RefPtr<Runnable> retry = NS_NewRunnableFunction(
    "EmbedLiteCompositorBridgeParent::RetryPlatformFrameRetirement",
    [self]() { self->SchedulePlatformFrameRetirement(); });
  if (NS_FAILED(compositorThread->DelayedDispatch(retry.forget(), aDelayMs))) {
    NS_WARNING("Failed to schedule EmbedLite frame retirement retry");
    return false;
  }
  return true;
}

bool
EmbedLiteCompositorBridgeParent::WithPlatformImage(
  const PlatformImageCallback& callback)
{
  LOGT("EmbedLiteCompositorBridgeParent::WithPlatformImage");
  if (!callback) {
    return false;
  }

  RefPtr<GLContext> context;
  std::shared_ptr<SharedSurface> frontBuffer;
  uint64_t generation;
  {
    MutexAutoLock lock(mRenderMutex);
    context = mGLContext;
    frontBuffer = mFrontBuffer;
    generation = mPlatformImageGeneration;
  }
  if (!context || !frontBuffer) {
    return false;
  }

  MutexAutoLock callbackLock(mPlatformImageCallbackMutex);
  MutexAutoLock imageLock(mPlatformImageMutex);
  {
    MutexAutoLock lock(mRenderMutex);
    if (generation != mPlatformImageGeneration) {
      return false;
    }
  }

  SharedSurface* sharedSurf = frontBuffer.get();
  if (sharedSurf->mDesc.type != SharedSurfaceType::EGLImageShare) {
    return false;
  }

  if (!sharedSurf->IsBufferAvailable()) {
    SchedulePlatformImageRetry();
    return false;
  }

  SharedSurface_EGLImage* eglImageSurf =
    static_cast<SharedSurface_EGLImage*>(sharedSurf);
  PlatformImageTextureTarget textureTarget;
  switch (eglImageSurf->EmbedderTextureTarget()) {
    case LOCAL_GL_TEXTURE_2D:
      textureTarget = PlatformImageTextureTarget::Texture2D;
      break;
    case LOCAL_GL_TEXTURE_EXTERNAL:
      textureTarget = PlatformImageTextureTarget::ExternalOES;
      break;
    default:
      NS_WARNING("Unsupported EmbedLite platform image texture target");
      return false;
  }

  const PlatformImageDescriptor descriptor = {
    PlatformImageHandleType::EGLImage,
    eglImageSurf->mImage,
    textureTarget,
    sharedSurf->mDesc.size.width,
    sharedSurf->mDesc.size.height
  };

  sharedSurf->ProducerReadAcquire();
  callback(descriptor);
  sharedSurf->ProducerReadRelease();
  return true;
}

bool
EmbedLiteCompositorBridgeParent::AcquirePlatformFrame(
  const PlatformFrameToken& token,
  const PlatformFrameCallback& callback)
{
  LOGT("EmbedLiteCompositorBridgeParent::AcquirePlatformFrame");
  if (!token.IsValid() || !callback) {
    return false;
  }

  MutexAutoLock callbackLock(mPlatformImageCallbackMutex);
  bool retry = false;
  RefPtr<GLContext> context;
  std::shared_ptr<SharedSurface> surface;
  PlatformFrameDescriptor descriptor;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    auto found = mPlatformFrames.end();
    size_t outstanding = 0;
    for (auto it = mPlatformFrames.begin(); it != mPlatformFrames.end(); ++it) {
      if (it->state != PlatformFrameState::Ready) {
        ++outstanding;
      }
      if (it->token == token) {
        found = it;
      }
    }
    if (found == mPlatformFrames.end() ||
        found->state != PlatformFrameState::Ready || outstanding >= 2) {
      return false;
    }

    SharedSurface* sharedSurf = found->surface.get();
    if (!sharedSurf->IsBufferAvailable()) {
      retry = true;
    } else {
      descriptor = {
        found->token,
        found->image,
        found->releaseFenceHandleType
      };
      context = found->context;
      surface = found->surface;
      found->state = PlatformFrameState::Acquiring;
    }
  }
  if (retry) {
    SchedulePlatformImageRetry();
    return false;
  }

  MOZ_RELEASE_ASSERT(context);
  surface->ProducerReadAcquire();
  const bool accepted = callback(descriptor);
  surface->ProducerReadRelease();
  if (accepted) {
    MutexAutoLock imageLock(mPlatformImageMutex);
    for (PlatformFrameRecord& frame : mPlatformFrames) {
      if (frame.token == token &&
          frame.state == PlatformFrameState::Acquiring) {
        frame.state = PlatformFrameState::Acquired;
        return true;
      }
    }
    return false;
  } else {
    // The record still pins these resources. Drop the calling-thread copies
    // before a retirement event can erase that record on the render thread.
    surface.reset();
    context = nullptr;
    bool retire = false;
    {
      MutexAutoLock imageLock(mPlatformImageMutex);
      for (PlatformFrameRecord& frame : mPlatformFrames) {
        if (!(frame.token == token) ||
            frame.state != PlatformFrameState::Acquiring) {
          continue;
        }
        MutexAutoLock lock(mRenderMutex);
        if (mPlatformFrameDeliveryEnabled &&
            frame.token == mLatestPlatformFrameToken &&
            frame.token.epoch == mPlatformImageGeneration) {
          frame.state = PlatformFrameState::Ready;
        } else {
          frame.state = PlatformFrameState::Released;
          retire = true;
        }
        break;
      }
    }
    if (retire) {
      SchedulePlatformFrameRetirement();
    }
  }
  return false;
}

bool
EmbedLiteCompositorBridgeParent::ReleasePlatformFrame(
  const PlatformFrameRelease& release)
{
  LOGT("EmbedLiteCompositorBridgeParent::ReleasePlatformFrame");
  if (!release.token.IsValid()) {
    return false;
  }
  if (release.fenceHandleType == PlatformFrameFenceHandleType::NoHandle &&
      release.fenceHandle) {
    return false;
  }
  if (release.fenceHandleType == PlatformFrameFenceHandleType::EGLSync &&
      !release.fenceHandle) {
    return false;
  }

  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    auto found = mPlatformFrames.end();
    for (auto it = mPlatformFrames.begin(); it != mPlatformFrames.end(); ++it) {
      if (it->token == release.token) {
        found = it;
        break;
      }
    }
    if (found == mPlatformFrames.end() ||
        found->state != PlatformFrameState::Acquired) {
      return false;
    }

    if (release.fenceHandleType == PlatformFrameFenceHandleType::EGLSync) {
      if (found->releaseFenceHandleType !=
            PlatformFrameFenceHandleType::EGLSync ||
          found->surface->mDesc.type != SharedSurfaceType::EGLImageShare) {
        return false;
      }
      SharedSurface_EGLImage* eglImageSurf =
        static_cast<SharedSurface_EGLImage*>(found->surface.get());
      if (eglImageSurf->GetConsumerFenceStatus(
            static_cast<EGLSync>(release.fenceHandle)) ==
          ConsumerFenceStatus::Invalid) {
        return false;
      }
      found->consumerFence = release.fenceHandle;
    } else if (release.fenceHandleType !=
               PlatformFrameFenceHandleType::NoHandle) {
      return false;
    }
    found->state = PlatformFrameState::Released;
  }

  SchedulePlatformFrameRetirement();
  return true;
}

bool
EmbedLiteCompositorBridgeParent::SetPlatformFrameDeliveryEnabled(bool enabled)
{
  LOGT("EmbedLiteCompositorBridgeParent::SetPlatformFrameDeliveryEnabled");
  MutexAutoLock imageLock(mPlatformImageMutex);
  MutexAutoLock lock(mRenderMutex);
  if (mPlatformFrameDeliveryEnabled == enabled) {
    return true;
  }
  if (enabled) {
    if (mPlatformFrameDeliveryStopPending || !mPlatformFrameListener) {
      return false;
    }
  } else {
    for (const PlatformFrameRecord& frame : mPlatformFrames) {
      if (frame.state == PlatformFrameState::Acquiring ||
          frame.state == PlatformFrameState::Acquired) {
        return false;
      }
    }

    const PlatformFrameToken readyToken = mLatestPlatformFrameToken;
    const uint64_t generation = mPlatformImageGeneration;
    MarkReadyPlatformFramesReleased();
    mPlatformFrameDeliveryStopPending = true;
    mPlatformFrameDeliveryEnabled = false;
    ++mPlatformImageGeneration;
    if (!mPlatformImageGeneration) {
      ++mPlatformImageGeneration;
    }
    if (!SchedulePlatformFrameRetirement()) {
      mPlatformFrameDeliveryStopPending = false;
      mPlatformFrameDeliveryEnabled = true;
      mPlatformImageGeneration = generation;
      if (readyToken.IsValid()) {
        for (PlatformFrameRecord& frame : mPlatformFrames) {
          if (frame.token == readyToken &&
              frame.state == PlatformFrameState::Released &&
              !frame.consumerFence) {
            frame.state = PlatformFrameState::Ready;
            mLatestPlatformFrameToken = readyToken;
            break;
          }
        }
      }
      return false;
    }
    return true;
  }

  mPlatformFrameDeliveryEnabled = true;
  ++mPlatformImageGeneration;
  if (!mPlatformImageGeneration) {
    ++mPlatformImageGeneration;
  }
  return true;
}

bool
EmbedLiteCompositorBridgeParent::SetPlatformFrameListener(
  EmbedLitePlatformFrameListener* listener)
{
  MutexAutoLock imageLock(mPlatformImageMutex);
  if (mPlatformFrameDeliveryEnabled || mPlatformFrameDeliveryStopPending ||
      !mPlatformFrames.empty()) {
    return false;
  }
  mPlatformFrameListener = listener;
  return true;
}

void
EmbedLiteCompositorBridgeParent::ClearPlatformImage()
{
  LOGT("EmbedLiteCompositorBridgeParent::ClearPlatformImage");
  bool retire = false;
  {
    MutexAutoLock imageLock(mPlatformImageMutex);
    MutexAutoLock lock(mRenderMutex);
    mFrontBuffer.reset();
    ++mPlatformImageGeneration;
    if (!mPlatformImageGeneration) {
      ++mPlatformImageGeneration;
    }
    retire = !mPlatformFrames.empty();
    MarkReadyPlatformFramesReleased();
  }
  if (retire) {
    SchedulePlatformFrameRetirement();
  }
}

void
EmbedLiteCompositorBridgeParent::SuspendRendering()
{
  LOGT("EmbedLiteCompositorBridgeParent::SuspendRendering");
  if (nsIThread* thread = CompositorThread()) {
    MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      thread,
      NewRunnableMethod("EmbedLiteCompositorBridgeParent::PauseComposition",
                        this,
                        &EmbedLiteCompositorBridgeParent::PauseComposition)));
  }
}

void
EmbedLiteCompositorBridgeParent::ResumeRendering()
{
  LOGT("EmbedLiteCompositorBridgeParent::ResumeRendering");
  EnsureSurfaceSizeFromWindow();
  int x;
  int y;
  int width;
  int height;
  {
    MutexAutoLock lock(mRenderMutex);
    x = mSurfaceOrigin.x;
    y = mSurfaceOrigin.y;
    width = mEGLSurfaceSize.width;
    height = mEGLSurfaceSize.height;
  }
  if (width > 0 && height > 0 && CompositorThread()) {
    MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      CompositorThread(),
      NewRunnableMethod<int, int, int, int>(
        "EmbedLiteCompositorBridgeParent::ResumeCompositionAndResize",
        this,
        &EmbedLiteCompositorBridgeParent::ResumeCompositionAndResize,
        x,
        y,
        width,
        height)));
    CompositorBridgeParent::ScheduleRenderOnCompositorThread(wr::RenderReasons::NONE);
  }
  ScheduleForcedRenderOnCompositorThread(wr::RenderReasons::WIDGET);
}

void
EmbedLiteCompositorBridgeParent::ScheduleForcedRenderOnCompositorThread(
    wr::RenderReasons aReasons)
{
  if (CompositorThreadHolder::IsInCompositorThread()) {
    ScheduleForcedRender(aReasons);
    return;
  }

  if (CompositorThread()) {
    CompositorThread()->Dispatch(NewRunnableMethod<wr::RenderReasons>(
      "EmbedLiteCompositorBridgeParent::ScheduleForcedRender",
      this,
      &EmbedLiteCompositorBridgeParent::ScheduleForcedRender,
      aReasons));
  }
}

void
EmbedLiteCompositorBridgeParent::ScheduleForcedRender(wr::RenderReasons aReasons)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  EnsureSurfaceSizeFromWindow();
  if (WebRenderBridgeParent* wrBridge = GetWrBridge()) {
    wrBridge->FlushRenderingWithInvalidation(aReasons);
    return;
  }

  ScheduleComposition(aReasons);
}

} // namespace embedlite
} // namespace mozilla
