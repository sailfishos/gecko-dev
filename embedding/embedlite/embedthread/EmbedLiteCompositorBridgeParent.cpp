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
                                                                 CSSToLayoutDeviceScale aScale,
                                                                 const TimeDuration &aVsyncRate,
                                                                 const CompositorOptions &aOptions,
                                                                 bool aRenderToEGLSurface,
                                                                 const gfx::IntSize &aSurfaceSize,
                                                                 uint64_t aInnerWindowId)
  : CompositorBridgeParent(aManager, aScale, aVsyncRate, aOptions, aRenderToEGLSurface, aSurfaceSize, aInnerWindowId)
  , mWindowId(windowId)
  , mCurrentCompositeTask(nullptr)
  , mSurfaceOrigin(0, 0)
  , mRenderMutex("EmbedLiteCompositorBridgeParent render mutex")
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
  EnsureSurfaceSizeFromWindow();
  mGLContext = aGL;
  if (mGLContext && !mGLContext->Screen()) {
    PrepareOffscreen();
  }
}

void
EmbedLiteCompositorBridgeParent::EnsureSurfaceSizeFromWindow()
{
  if (!mEGLSurfaceSize.IsEmpty()) {
    return;
  }

  EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId);
  if (!parentWindow) {
    return;
  }

  const int width = static_cast<int>(parentWindow->mSize.width);
  const int height = static_cast<int>(parentWindow->mSize.height);
  if (width <= 0 || height <= 0) {
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

void
EmbedLiteCompositorBridgeParent::PrepareOffscreen()
{
  LOGT("EmbedLiteCompositorBridgeParent::PrepareOffscreen");

  GLContext* context = mGLContext;
  NS_ENSURE_TRUE(context, );
  NS_ENSURE_TRUE(!mEGLSurfaceSize.IsEmpty(), );

  if (!context->IsCurrent()) {
    context->MakeCurrent(true);
  }
  NS_ENSURE_TRUE(context->IsCurrent(), );

  MutexAutoLock lock(mRenderMutex);
  if (!context->Screen()) {
    NS_ENSURE_TRUE(context->CreateOffscreenDefaultFb(mEGLSurfaceSize), );
  } else if (context->Screen()->Size() != mEGLSurfaceSize) {
    NS_ENSURE_TRUE(context->ResizeScreenBuffer(mEGLSurfaceSize), );
  }

  if (context->GetContextType() == GLContextType::EGL) {
    if (UniquePtr<SurfaceFactory> factory = SurfaceFactory_EGLImage::Create(*context)) {
      context->Screen()->Morph(std::move(factory));
    }
  } else {
    NS_ERROR("Only EGL context type is supported for offscreen rendering");
  }
}

bool
EmbedLiteCompositorBridgeParent::PresentOffscreenSurface()
{
  LOGT("EmbedLiteCompositorBridgeParent::PresentOffscreenSurface");
  GLContext* context = mGLContext;
  if (!context || !context->Screen()) {
    mFrontBuffer.reset();
    return false;
  }

  // RenderGL is called always from Gecko compositor thread.
  // GLScreenBuffer::PublishFrame does swap buffers and that
  // cannot happen while reading previous frame on EmbedLiteCompositorBridgeParent::GetPlatformImage
  // (potentially from another thread).
  MutexAutoLock lock(mRenderMutex);

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);

  if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
    NS_ERROR("Failed to publish context frame");
    mFrontBuffer.reset();
    return false;
  }
  mFrontBuffer = screen->FrontBuffer();
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
  bool changed = false;
  if (width > 0 && height > 0 && (mEGLSurfaceSize.width != width ||
                                  mEGLSurfaceSize.height != height ||
                                  mSurfaceOrigin.x != x ||
                                  mSurfaceOrigin.y != y)) {
    MutexAutoLock lock(mRenderMutex);
    mSurfaceOrigin.MoveTo(x, y);
    SetEGLSurfaceRect(x, y, width, height);
    changed = true;
  }

  if (changed && mGLContext) {
    PrepareOffscreen();
  }
}

void
EmbedLiteCompositorBridgeParent::GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback)
{
  LOGT("EmbedLiteCompositorBridgeParent::GetPlatformImage cb");
  MutexAutoLock lock(mRenderMutex);
  GLContext* context = mGLContext;
  if (!context || !context->Screen()) {
    return;
  }

  std::shared_ptr<SharedSurface> frontBuffer = mFrontBuffer;
  if (!frontBuffer) {
    return;
  }
  SharedSurface* sharedSurf = frontBuffer.get();
  NS_ENSURE_TRUE(sharedSurf, );

  sharedSurf->ProducerReadAcquire();
  // See ProducerAcquireImpl() & ProducerReleaseImpl()
  // See sha1 b66e705f3998791c137f8fce908ec0835b84afbe from gecko-mirror

  if (sharedSurf->mDesc.type == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = (SharedSurface_EGLImage*)sharedSurf;
    callback(eglImageSurf->mImage, sharedSurf->mDesc.size.width, sharedSurf->mDesc.size.height);
  }
  sharedSurf->ProducerReadRelease();
}

void
EmbedLiteCompositorBridgeParent::ClearPlatformImage()
{
  LOGT("EmbedLiteCompositorBridgeParent::ClearPlatformImage");
  MutexAutoLock lock(mRenderMutex);
  mFrontBuffer.reset();
}

void*
EmbedLiteCompositorBridgeParent::GetPlatformImage(int* width, int* height)
{
  LOGT("EmbedLiteCompositorBridgeParent::GetPlatformImage w h");
  MutexAutoLock lock(mRenderMutex);
  GLContext* context = mGLContext;
  NS_ENSURE_TRUE(context, nullptr);
  NS_ENSURE_TRUE(context->Screen(), nullptr);

  std::shared_ptr<SharedSurface> frontBuffer = mFrontBuffer;
  NS_ENSURE_TRUE(frontBuffer, nullptr);
  SharedSurface* sharedSurf = frontBuffer.get();
  NS_ENSURE_TRUE(sharedSurf, nullptr);
  // sharedSurf->WaitSync();
  // ProducerAcquireImpl & ProducerReleaseImpl ?

  *width = sharedSurf->mDesc.size.width;
  *height = sharedSurf->mDesc.size.height;

  if (sharedSurf->mDesc.type == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = (SharedSurface_EGLImage*)sharedSurf;
    return eglImageSurf->mImage;
  }

  return nullptr;
}

void
EmbedLiteCompositorBridgeParent::SuspendRendering()
{
  LOGT("EmbedLiteCompositorBridgeParent::SuspendRendering");
  CompositorBridgeParent::SchedulePauseOnCompositorThread();
}

void
EmbedLiteCompositorBridgeParent::ResumeRendering()
{
  LOGT("EmbedLiteCompositorBridgeParent::ResumeRendering");
  EnsureSurfaceSizeFromWindow();
  if (mEGLSurfaceSize.width > 0 && mEGLSurfaceSize.height > 0) {
    CompositorBridgeParent::ScheduleResumeOnCompositorThread(mSurfaceOrigin.x,
                                                             mSurfaceOrigin.y,
                                                             mEGLSurfaceSize.width,
                                                             mEGLSurfaceSize.height);
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
  if (mGLContext && !mGLContext->Screen()) {
    PrepareOffscreen();
  }
  if (WebRenderBridgeParent* wrBridge = GetWrBridge()) {
    wrBridge->ScheduleForcedGenerateFrame(aReasons);
    wrBridge->CompositeToTarget(VsyncId(), aReasons, nullptr, nullptr);
    wrBridge->FlushRendering(aReasons);
    return;
  }

  ScheduleComposition(aReasons);
}

} // namespace embedlite
} // namespace mozilla
