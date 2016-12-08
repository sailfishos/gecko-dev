/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorParent.h"
#include "BasicLayers.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteWindow.h"
#include "EmbedLiteWindowBaseParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/Preferences.h"
#include "gfxUtils.h"
#include "nsRefreshDriver.h"

#include "math.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "TextureClientSharedSurface.h" // for SharedSurfaceTextureClient
#include "ClientLayerManager.h"         // for ClientLayerManager, etc

using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace embedlite {

static const int sDefaultPaintInterval = nsRefreshDriver::DefaultInterval();

EmbedLiteCompositorParent::EmbedLiteCompositorParent(nsIWidget* widget,
                                                     uint32_t windowId,
                                                     bool aRenderToEGLSurface,
                                                     int aSurfaceWidth,
                                                     int aSurfaceHeight)
  : CompositorParent(widget, aRenderToEGLSurface, aSurfaceWidth, aSurfaceHeight)
  , mWindowId(windowId)
  , mCurrentCompositeTask(nullptr)
  , mSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mRenderMutex("EmbedLiteCompositorParent render mutex")
{
  EmbedLiteWindowBaseParent* parentWindow = EmbedLiteWindowBaseParent::From(mWindowId);
  LOGT("this:%p, window:%p, sz[%i,%i]", this, parentWindow, aSurfaceWidth, aSurfaceHeight);
  Preferences::AddBoolVarCache(&mUseExternalGLContext,
                               "embedlite.compositor.external_gl_context", false);
  parentWindow->SetCompositor(this);
}

EmbedLiteCompositorParent::~EmbedLiteCompositorParent()
{
  LOGT();
}

PLayerTransactionParent*
EmbedLiteCompositorParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                        const uint64_t& aId,
                                                        TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                        bool* aSuccess)
{
  PLayerTransactionParent* p =
    CompositorParent::AllocPLayerTransactionParent(aBackendHints,
                                                   aId,
                                                   aTextureFactoryIdentifier,
                                                   aSuccess);

  EmbedLiteWindow* win = EmbedLiteApp::GetInstance()->GetWindowByID(mWindowId);
  if (win) {
    win->GetListener()->CompositorCreated();
  }

  if (!mUseExternalGLContext) {
    // Prepare Offscreen rendering context
    PrepareOffscreen();
  }
  return p;
}

void
EmbedLiteCompositorParent::PrepareOffscreen()
{

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );

  if (context->IsOffscreen()) {
    GLScreenBuffer* screen = context->Screen();
    if (screen) {
      UniquePtr<SurfaceFactory> factory;
      if (context->GetContextType() == GLContextType::EGL) {
        // [Basic/OGL Layers, OMTC] WebGL layer init.
        auto forwarder = state->mLayerManager->AsShadowForwarder();

        layers::TextureFlags flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;

        printf("=============== caps.premultAlpha: %d ptr: %p\n", screen->mCaps.premultAlpha, forwarder);

        if (!caps.premultAlpha) {
            flags |= layers::TextureFlags::NON_PREMULTIPLIED;
        }
        factory = SurfaceFactory_EGLImage::Create(context, screen->mCaps, forwarder, flags);
      } else {
        // [Basic Layers, OMTC] WebGL layer init.
        // Well, this *should* work...
        GLContext* nullConsGL = nullptr; // Bug 1050044.
        factory = MakeUnique<SurfaceFactory_GLTexture>(context, nullConsGL, screen->mCaps);
      }
      if (factory) {
        screen->Morph(Move(factory));
      }
    }
  }
}

void
EmbedLiteCompositorParent::UpdateTransformState()
{
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  CompositorOGL *compositor = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor());
  NS_ENSURE_TRUE(compositor, );

  GLContext* context = compositor->gl();
  NS_ENSURE_TRUE(context, );

  if (context->IsOffscreen() && context->OffscreenSize() != mSurfaceSize && context->ResizeOffscreen(mSurfaceSize)) {
    ScheduleRenderOnCompositorThread();
  }
}

void
EmbedLiteCompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  if (Invalidate()) {
    task->Cancel();
    CancelCurrentCompositeTask();
  } else {
    CompositorParent::ScheduleTask(task, time);
  }
}

bool
EmbedLiteCompositorParent::Invalidate()
{
  if (!mUseExternalGLContext) {
    UpdateTransformState();
    mCurrentCompositeTask = NewRunnableMethod(this, &EmbedLiteCompositorParent::RenderGL, TimeStamp::Now());
    MessageLoop::current()->PostDelayedTask(FROM_HERE, mCurrentCompositeTask, sDefaultPaintInterval);
    return true;
  }

  return false;
}

bool EmbedLiteCompositorParent::RenderGL(TimeStamp aScheduleTime)
{
//  mLastCompose = aScheduleTime;
  if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, false);
  if (!context->IsCurrent()) {
    context->MakeCurrent(true);
  }
  NS_ENSURE_TRUE(context->IsCurrent(), false);

  {
    ScopedScissorRect autoScissor(context);
    GLenum oldTexUnit;
    context->GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &oldTexUnit);
    CompositeToTarget(nullptr);
    context->fActiveTexture(oldTexUnit);
  }

  if (context->IsOffscreen()) {
    // RenderGL is called always from Gecko compositor thread.
    // GLScreenBuffer::PublishFrame does swap buffers and that
    // cannot happen while reading previous frame on EmbedLiteCompositorParent::GetPlatformImage
    // (potentially from another thread).
    MutexAutoLock lock(mRenderMutex);
    GLScreenBuffer* screen = context->Screen();
    MOZ_ASSERT(screen);

    if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
      NS_ERROR("Failed to publish context frame");
      return false;
    }
    // Temporary hack, we need two extra paints in order to get initial picture
    static int sInitialPaintCount = 0;
    if (sInitialPaintCount < 2) {
      ScheduleRenderOnCompositorThread();
      sInitialPaintCount++;
    }
  }

  return false;
}

void EmbedLiteCompositorParent::SetSurfaceSize(int width, int height)
{
  if (width > 0 && height > 0 && (mSurfaceSize.width != width || mSurfaceSize.height != height)) {
    SetEGLSurfaceSize(width, height);
    mSurfaceSize = gfx::IntSize(width, height);
  }
}

void*
EmbedLiteCompositorParent::GetPlatformImage(int* width, int* height)
{
  MutexAutoLock lock(mRenderMutex);
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, nullptr);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, nullptr);
  NS_ENSURE_TRUE(context->IsOffscreen(), nullptr);

  GLScreenBuffer* screen = context->Screen();
  MOZ_ASSERT(screen);
  NS_ENSURE_TRUE(screen->Front(), nullptr);
  SharedSurface* sharedSurf = screen->Front()->Surf();
  NS_ENSURE_TRUE(sharedSurf, nullptr);
  sharedSurf->WaitSync();

  *width = sharedSurf->mSize.width;
  *height = sharedSurf->mSize.height;

  if (sharedSurf->mType == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = SharedSurface_EGLImage::Cast(sharedSurf);
    return eglImageSurf->mImage;
  }

  return nullptr;
}

void
EmbedLiteCompositorParent::SuspendRendering()
{
  CompositorParent::SchedulePauseOnCompositorThread();
}

void
EmbedLiteCompositorParent::ResumeRendering()
{
  if (mSurfaceSize.width > 0 && mSurfaceSize.height > 0) {
    CompositorParent::ScheduleResumeOnCompositorThread(mSurfaceSize.width,
                                                       mSurfaceSize.height);
    CompositorParent::ScheduleRenderOnCompositorThread();
  }
}

} // namespace embedlite
} // namespace mozilla

