/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteWebRenderBridgeParent.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteWindow.h"
#include "EmbedLiteWindowParent.h"
//#include "mozilla/layers/CompositorOGL.h"
//#include "mozilla/layers/TextureClientSharedSurface.h" // for SharedSurfaceTextureClient
#include "mozilla/StaticPrefs_embedlite.h"             // for StaticPrefs::embedlite_*()
#include "mozilla/Preferences.h"
#include "gfxUtils.h"
#include "nsRefreshDriver.h"

#include "math.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for SwapChain, SwapChainPresenter
#include "ScopedGLHelpers.h"            // for ScopedScissorRect
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "VsyncSource.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace embedlite {

static const int sDefaultPaintInterval = nsRefreshDriver::DefaultInterval();

EmbedLiteWebRenderBridgeParent::EmbedLiteWebRenderBridgeParent(mozilla::layers::CompositorBridgeParentBase* aCompositorBridge,
                                 const wr::PipelineId& aPipelineId,
                                 widget::CompositorWidget* aWidget,
                                 mozilla::layers::CompositorVsyncScheduler* aScheduler,
                                 RefPtr<wr::WebRenderAPI>&& aApi,
                                 RefPtr<mozilla::layers::AsyncImagePipelineManager>&& aImageMgr,
                                 TimeDuration aVsyncRate)
  : WebRenderBridgeParent(aCompositorBridge, aPipelineId, aWidget, aScheduler, std::move(aApi), std::move(aImageMgr), aVsyncRate)
/*
  , mWindowId(windowId)
  , mCurrentCompositeTask(nullptr)
  , mSurfaceOrigin(0, 0)
*/
  , mRenderMutex("EmbedLiteWebRenderBridgeParent render mutex")
{
  LOGT("EmbedLiteWebRenderBridgeParent::EmbedLiteWebRenderBridgeParent");
/*
  if (mWindowId == 0) {
    mWindowId = EmbedLiteWindowParent::Current();
  }
  EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId);
  LOGT("this:%p, window:%p, sz[%i,%i]", this, parentWindow, aSurfaceSize.width, aSurfaceSize.height);

  parentWindow->SetCompositor(this);
*/
  // Post open parent?
  //
}

EmbedLiteWebRenderBridgeParent::~EmbedLiteWebRenderBridgeParent()
{
  LOGT("EmbedLiteWebRenderBridgeParent::~EmbedLiteWebRenderBridgeParent");
  LOGT();
}

void
EmbedLiteWebRenderBridgeParent::PrepareOffscreen()
{
  LOGT("EmbedLiteWebRenderBridgeParent::PrepareOffscreen");
  fprintf(stderr, "=============== Preparing offscreen rendering context ===============\n");
//FIXME
/*
  const WebRenderBridgeParent::LayerTreeState* state = WebRenderBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );

  // TODO: The switch from GLSCreenBuffer to SwapChain needs completing
  // See: https://phabricator.services.mozilla.com/D75055
  if (context->IsOffscreen()) {
    GLScreenBuffer* screen = context->Screen();
    if (screen) {
      UniquePtr<SurfaceFactory> factory;

      layers::TextureFlags flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;

      if (context->GetContextType() == GLContextType::EGL) {
        // [Basic/OGL Layers, OMTC] WebGL layer init.
        factory = SurfaceFactory_EGLImage::Create(context, nullptr, flags);
      } else {
        NS_ERROR("Only EGL context type is supported for offscreen rendering");
      }

      if (factory) {
        screen->Morph(std::move(factory));
      }
    }
  }
*/
}

void
EmbedLiteWebRenderBridgeParent::CompositeToDefaultTarget(VsyncId aId, wr::RenderReasons aReasons)
{
  LOGT("EmbedLiteWebRenderBridgeParent::CompositeToDefaultTarget");
//FIXME
/*
  const WebRenderBridgeParent::LayerTreeState* state = WebRenderBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  if (!context->IsCurrent()) {
    context->MakeCurrent(true);
  }
  NS_ENSURE_TRUE(context->IsCurrent(), );

  if (context->IsOffscreen()) {
    MutexAutoLock lock(mRenderMutex);
    if (context->Screen()->Size() != mEGLSurfaceSize && !context->ResizeScreenBuffer(mEGLSurfaceSize)) {
      return;
    }
  }

  {
    ScopedScissorRect autoScissor(context);
    GLenum oldTexUnit;
    context->GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &oldTexUnit);
    CompositeToTarget(aId, nullptr);
    context->fActiveTexture(oldTexUnit);
  }
*/
}

void
EmbedLiteWebRenderBridgeParent::PresentOffscreenSurface()
{
  LOGT("EmbedLiteWebRenderBridgeParent::PresentOffscreenSurface");
//FIXME
/*
  const WebRenderBridgeParent::LayerTreeState* state = WebRenderBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  NS_ENSURE_TRUE(context->IsOffscreen(), );

  // RenderGL is called always from Gecko compositor thread.
  // GLScreenBuffer::PublishFrame does swap buffers and that
  // cannot happen while reading previous frame on EmbedLiteWebRenderBridgeParent::GetPlatformImage
  // (potentially from another thread).
  MutexAutoLock lock(mRenderMutex);

  // TODO: The switch from GLSCreenBuffer to SwapChain needs completing
  // See: https://phabricator.services.mozilla.com/D75055
  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);

  if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
    NS_ERROR("Failed to publish context frame");
  }
*/
}

bool EmbedLiteWebRenderBridgeParent::GetScrollableRect(CSSRect &scrollableRect)
{
  LOGT("EmbedLiteWebRenderBridgeParent::GetScrollableRect");
//FIXME
/*
  const CompositorBridgeParent::LayerTreeState *state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);
  mozilla::layers::LayerMetricsWrapper layerMetricsWrapper = state->mLayerManager->GetRootContentLayer();
  const FrameMetrics &fm = layerMetricsWrapper.Metrics();
  scrollableRect = fm.GetScrollableRect();
*/
  return true;
}

void EmbedLiteWebRenderBridgeParent::SetSurfaceRect(int x, int y, int width, int height)
{
  LOGT("EmbedLiteWebRenderBridgeParent::SetSurfaceRect");
//FIXME
/*
  if (width > 0 && height > 0 && (mEGLSurfaceSize.width != width ||
                                  mEGLSurfaceSize.height != height ||
                                  mSurfaceOrigin.x != x ||
                                  mSurfaceOrigin.y != y)) {
    MutexAutoLock lock(mRenderMutex);
    mSurfaceOrigin.MoveTo(x, y);
    SetEGLSurfaceRect(x, y, width, height);
  }
*/
}

void
EmbedLiteWebRenderBridgeParent::GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback)
{
  LOGT("EmbedLiteWebRenderBridgeParent::GetPlatformImage cb");
//FIXME
/*
  MutexAutoLock lock(mRenderMutex);
  const WebRenderBridgeParent::LayerTreeState* state = WebRenderBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  NS_ENSURE_TRUE(context->IsOffscreen(), );

  // TODO: The switch from GLSCreenBuffer to SwapChain needs completing
  // See: https://phabricator.services.mozilla.com/D75055
  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);
  NS_ENSURE_TRUE(screen->Front(),);
  SharedSurface* sharedSurf = screen->Front()->Surf();
  NS_ENSURE_TRUE(sharedSurf, );

  sharedSurf->ProducerReadAcquire();
  // See ProducerAcquireImpl() & ProducerReleaseImpl()
  // See sha1 b66e705f3998791c137f8fce908ec0835b84afbe from gecko-mirror

  if (sharedSurf->mDesc.type == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = (SharedSurface_EGLImage*)sharedSurf;
    callback(eglImageSurf->mImage, sharedSurf->mDesc.size.width, sharedSurf->mDesc.size.height);
  }
  sharedSurf->ProducerReadRelease();
*/
}

void*
EmbedLiteWebRenderBridgeParent::GetPlatformImage(int* width, int* height)
{
  LOGT("EmbedLiteWebRenderBridgeParent::GetPlatformImage w h");
//FIXME
/*
  MutexAutoLock lock(mRenderMutex);
  const WebRenderBridgeParent::LayerTreeState* state = WebRenderBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, nullptr);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, nullptr);
  NS_ENSURE_TRUE(context->IsOffscreen(), nullptr);

  // TODO: The switch from GLSCreenBuffer to SwapChain needs completing
  // See: https://phabricator.services.mozilla.com/D75055
  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);
  NS_ENSURE_TRUE(screen->Front(), nullptr);
  SharedSurface* sharedSurf = screen->Front()->Surf();
  NS_ENSURE_TRUE(sharedSurf, nullptr);
  // sharedSurf->WaitSync();
  // ProducerAcquireImpl & ProducerReleaseImpl ?

  *width = sharedSurf->mDesc.size.width;
  *height = sharedSurf->mDesc.size.height;

  if (sharedSurf->mDesc.type == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = (SharedSurface_EGLImage*)sharedSurf;
    return eglImageSurf->mImage;
  }
*/
  return nullptr;
}

void
EmbedLiteWebRenderBridgeParent::SuspendRendering()
{
  LOGT("EmbedLiteWebRenderBridgeParent::SuspendRendering");
//FIXME
//  WebRenderBridgeParent::SchedulePauseOnCompositorThread();
}

void
EmbedLiteWebRenderBridgeParent::ResumeRendering()
{
  LOGT("EmbedLiteWebRenderBridgeParent::ResumeRendering");
//FIXME
/*
  if (mEGLSurfaceSize.width > 0 && mEGLSurfaceSize.height > 0) {
    WebRenderBridgeParent::ScheduleResumeOnCompositorThread(mSurfaceOrigin.x,
                                                             mSurfaceOrigin.y,
                                                             mEGLSurfaceSize.width,
                                                             mEGLSurfaceSize.height);
//FIXME NONE ok?
    WebRenderBridgeParent::ScheduleRenderOnCompositorThread(wr::RenderReasons::NONE);
  }
*/
}

} // namespace embedlite
} // namespace mozilla

