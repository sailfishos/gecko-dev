/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorBridgeParent.h"
#include "BasicLayers.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteWindow.h"
#include "EmbedLiteWindowParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureClientSharedSurface.h" // for SharedSurfaceTextureClient
#include "mozilla/Preferences.h"
#include "gfxUtils.h"
#include "nsRefreshDriver.h"

#include "math.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "VsyncSource.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace embedlite {

static const int sDefaultPaintInterval = nsRefreshDriver::DefaultInterval();

EmbedLiteCompositorBridgeParent::EmbedLiteCompositorBridgeParent(uint32_t windowId,
                                                                 CompositorManagerParent* aManager,
                                                                 CSSToLayoutDeviceScale aScale,
                                                                 const TimeDuration &aVsyncRate,
                                                                 const CompositorOptions &aOptions,
                                                                 bool aRenderToEGLSurface,
                                                                 const gfx::IntSize &aSurfaceSize)
  : CompositorBridgeParent(aManager, aScale, aVsyncRate, aOptions, aRenderToEGLSurface, aSurfaceSize)
  , mWindowId(windowId)
  , mCurrentCompositeTask(nullptr)
  , mSurfaceOrigin(0, 0)
  , mRenderMutex("EmbedLiteCompositorBridgeParent render mutex")
{
  if (mWindowId == 0) {
    mWindowId = EmbedLiteWindowParent::Current();
  }
  EmbedLiteWindowParent* parentWindow = EmbedLiteWindowParent::From(mWindowId);
  LOGT("this:%p, window:%p, sz[%i,%i]", this, parentWindow, aSurfaceSize.width, aSurfaceSize.height);


  // TODO: Switch this to use a static pref
  // See https://firefox-source-docs.mozilla.org/modules/libpref/index.html#static-prefs
  // Example: https://phabricator.services.mozilla.com/D40340
  //Preferences::AddBoolVarCache(&mUseExternalGLContext,
  //                             "embedlite.compositor.external_gl_context", false);
  mUseExternalGLContext = false; // "embedlite.compositor.external_gl_context"
  parentWindow->SetCompositor(this);

  // Post open parent?
  //
}

EmbedLiteCompositorBridgeParent::~EmbedLiteCompositorBridgeParent()
{
  LOGT();
}

PLayerTransactionParent*
EmbedLiteCompositorBridgeParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                              const LayersId& aId)
{
  PLayerTransactionParent* p =
    CompositorBridgeParent::AllocPLayerTransactionParent(aBackendHints, aId);

  EmbedLiteWindowParent *parentWindow = EmbedLiteWindowParent::From(mWindowId);
  if (parentWindow) {
    parentWindow->GetListener()->CompositorCreated();
  }

  if (!mUseExternalGLContext) {
    // Prepare Offscreen rendering context
    PrepareOffscreen();
  }
  return p;
}

bool EmbedLiteCompositorBridgeParent::DeallocPLayerTransactionParent(PLayerTransactionParent *aLayers)
{
    bool deallocated = CompositorBridgeParent::DeallocPLayerTransactionParent(aLayers);
    LOGT();
    return deallocated;
}

void
EmbedLiteCompositorBridgeParent::PrepareOffscreen()
{
  fprintf(stderr, "=============== Preparing offscreen rendering context ===============\n");

  const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );

  if (context->IsOffscreen()) {
    GLScreenBuffer* screen = context->Screen();
    if (screen) {
      UniquePtr<SurfaceFactory> factory;

      layers::TextureFlags flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
      if (!screen->mCaps.premultAlpha) {
          flags |= layers::TextureFlags::NON_PREMULTIPLIED;
      }

      if (context->GetContextType() == GLContextType::EGL) {
        // [Basic/OGL Layers, OMTC] WebGL layer init.
        factory = SurfaceFactory_EGLImage::Create(context, screen->mCaps, nullptr, flags);
      } else {
        // [Basic Layers, OMTC] WebGL layer init.
        // Well, this *should* work...
        factory = MakeUnique<SurfaceFactory_GLTexture>(context, screen->mCaps, nullptr, flags);
      }
      if (factory) {
        screen->Morph(std::move(factory));
      }
    }
  }
}

void
EmbedLiteCompositorBridgeParent::CompositeToDefaultTarget(VsyncId aId)
{
  const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  if (!context->IsCurrent()) {
    context->MakeCurrent(true);
  }
  NS_ENSURE_TRUE(context->IsCurrent(), );

  if (context->IsOffscreen()) {
    MutexAutoLock lock(mRenderMutex);
    if (context->OffscreenSize() != mEGLSurfaceSize && !context->ResizeOffscreen(mEGLSurfaceSize)) {
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
}

void
EmbedLiteCompositorBridgeParent::PresentOffscreenSurface()
{
  const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  NS_ENSURE_TRUE(context->IsOffscreen(), );

  // RenderGL is called always from Gecko compositor thread.
  // GLScreenBuffer::PublishFrame does swap buffers and that
  // cannot happen while reading previous frame on EmbedLiteCompositorBridgeParent::GetPlatformImage
  // (potentially from another thread).
  MutexAutoLock lock(mRenderMutex);

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);

  if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
    NS_ERROR("Failed to publish context frame");
  }
}

bool EmbedLiteCompositorBridgeParent::GetScrollableRect(CSSRect &scrollableRect)
{
  const CompositorBridgeParent::LayerTreeState *state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);

  mozilla::layers::LayerMetricsWrapper layerMetricsWrapper = state->mLayerManager->GetRootContentLayer();
  const FrameMetrics &fm = layerMetricsWrapper.Metrics();
  scrollableRect = fm.GetScrollableRect();
  return true;
}

void EmbedLiteCompositorBridgeParent::SetSurfaceRect(int x, int y, int width, int height)
{
  if (width > 0 && height > 0 && (mEGLSurfaceSize.width != width ||
                                  mEGLSurfaceSize.height != height ||
                                  mSurfaceOrigin.x != x ||
                                  mSurfaceOrigin.y != y)) {
    MutexAutoLock lock(mRenderMutex);
    mSurfaceOrigin.MoveTo(x, y);
    SetEGLSurfaceRect(x, y, width, height);
  }
}

void
EmbedLiteCompositorBridgeParent::GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback)
{
  MutexAutoLock lock(mRenderMutex);
  const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );
  NS_ENSURE_TRUE(context->IsOffscreen(), );

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);
  NS_ENSURE_TRUE(screen->Front(),);
  SharedSurface* sharedSurf = screen->Front()->Surf();
  NS_ENSURE_TRUE(sharedSurf, );

  sharedSurf->ProducerReadAcquire();
  // See ProducerAcquireImpl() & ProducerReleaseImpl()
  // See sha1 b66e705f3998791c137f8fce908ec0835b84afbe from gecko-mirror

  if (sharedSurf->mType == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = SharedSurface_EGLImage::Cast(sharedSurf);
    callback(eglImageSurf->mImage, sharedSurf->mSize.width, sharedSurf->mSize.height);
  }
  sharedSurf->ProducerReadRelease();
}

void*
EmbedLiteCompositorBridgeParent::GetPlatformImage(int* width, int* height)
{
  MutexAutoLock lock(mRenderMutex);
  const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, nullptr);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, nullptr);
  NS_ENSURE_TRUE(context->IsOffscreen(), nullptr);

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);
  NS_ENSURE_TRUE(screen->Front(), nullptr);
  SharedSurface* sharedSurf = screen->Front()->Surf();
  NS_ENSURE_TRUE(sharedSurf, nullptr);
  // sharedSurf->WaitSync();
  // ProducerAcquireImpl & ProducerReleaseImpl ?

  *width = sharedSurf->mSize.width;
  *height = sharedSurf->mSize.height;

  if (sharedSurf->mType == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = SharedSurface_EGLImage::Cast(sharedSurf);
    return eglImageSurf->mImage;
  }

  return nullptr;
}

void
EmbedLiteCompositorBridgeParent::SuspendRendering()
{
  CompositorBridgeParent::SchedulePauseOnCompositorThread();
}

void
EmbedLiteCompositorBridgeParent::ResumeRendering()
{
  if (mEGLSurfaceSize.width > 0 && mEGLSurfaceSize.height > 0) {
    CompositorBridgeParent::ScheduleResumeOnCompositorThread(mSurfaceOrigin.x,
                                                             mSurfaceOrigin.y,
                                                             mEGLSurfaceSize.width,
                                                             mEGLSurfaceSize.height);
    CompositorBridgeParent::ScheduleRenderOnCompositorThread();
  }
}

} // namespace embedlite
} // namespace mozilla

