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
  , mPlatformImageMutex("EmbedLiteCompositorBridgeParent platform image mutex")
  , mPlatformImageGeneration(0)
  , mPlatformImageRetryPending(false)
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
  LOGT();
}

void
EmbedLiteCompositorBridgeParent::SetWebRenderGLContext(GLContext* aGL)
{
  LOGT("gl:%p", aGL);
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  MutexAutoLock imageLock(mPlatformImageMutex);
  MutexAutoLock lock(mRenderMutex);
  if (mGLContext != aGL) {
    mFrontBuffer.reset();
    ++mPlatformImageGeneration;
    mGLContext = aGL;
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
EmbedLiteCompositorBridgeParent::PresentOffscreenSurface()
{
  LOGT("EmbedLiteCompositorBridgeParent::PresentOffscreenSurface");
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
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
  MutexAutoLock lock(mRenderMutex);
  if (context != mGLContext || generation != mPlatformImageGeneration) {
    return false;
  }
  mFrontBuffer = std::move(frontBuffer);
  return !!mFrontBuffer;
}

void
EmbedLiteCompositorBridgeParent::WebRenderComposited()
{
  if (!PresentOffscreenSurface()) {
    return;
  }

  if (EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId)) {
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
  MutexAutoLock lock(mRenderMutex);
  if (width > 0 && height > 0 && (mEGLSurfaceSize.width != width ||
                                  mEGLSurfaceSize.height != height ||
                                  mSurfaceOrigin.x != x ||
                                  mSurfaceOrigin.y != y)) {
    mSurfaceOrigin.MoveTo(x, y);
    SetEGLSurfaceRect(x, y, width, height);
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
        parentWindow->GetListener()->CompositingFinished();
      }
    });
  if (NS_FAILED(compositorThread->DelayedDispatch(retry.forget(), 16))) {
    mPlatformImageRetryPending = false;
  }
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

void
EmbedLiteCompositorBridgeParent::ClearPlatformImage()
{
  LOGT("EmbedLiteCompositorBridgeParent::ClearPlatformImage");
  MutexAutoLock imageLock(mPlatformImageMutex);
  MutexAutoLock lock(mRenderMutex);
  mFrontBuffer.reset();
  ++mPlatformImageGeneration;
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
