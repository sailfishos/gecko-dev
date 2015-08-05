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
#include "gfxUtils.h"
#include "nsRefreshDriver.h"

#include "math.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
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
  , mRotation(ROTATION_0)
  , mUseScreenRotation(false)
  , mCurrentCompositeTask(nullptr)
  , mLastViewSize(aSurfaceWidth, aSurfaceHeight)
{
  EmbedLiteWindowBaseParent* parentWindow = EmbedLiteWindowBaseParent::From(mWindowId);
  LOGT("this:%p, window:%p, sz[%i,%i]", this, parentWindow, aSurfaceWidth, aSurfaceHeight);
  parentWindow->SetCompositor(this);
  // Workaround for MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
  mLastCompose = TimeStamp::Now();
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

  // Prepare Offscreen rendering context
  PrepareOffscreen();
  return p;
}

void
EmbedLiteCompositorParent::PrepareOffscreen()
{
  EmbedLiteWindow* win = EmbedLiteApp::GetInstance()->GetWindowByID(mWindowId);
  if (win) {
    win->GetListener()->CompositorCreated();
  }

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
        factory = SurfaceFactory_EGLImage::Create(context, screen->mCaps);
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

  if (mUseScreenRotation) {
    LOGNI("SetScreenRotation is not fully implemented");
    // compositor->SetScreenRotation(mRotation);
    // state->mLayerManager->SetWorldTransform(mWorldTransform);
  }

  if (context->IsOffscreen() && context->OffscreenSize() != mLastViewSize) {
    context->ResizeOffscreen(mLastViewSize);
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
  UpdateTransformState();

  EmbedLiteWindow* win = EmbedLiteApp::GetInstance()->GetWindowByID(mWindowId);
  if (win && !win->GetListener()->Invalidate()) {
    mCurrentCompositeTask = NewRunnableMethod(this, &EmbedLiteCompositorParent::RenderGL, TimeStamp::Now());
    MessageLoop::current()->PostDelayedTask(FROM_HERE, mCurrentCompositeTask, sDefaultPaintInterval);
    return true;
  }

  return false;
}

bool EmbedLiteCompositorParent::RenderToContext(gfx::DrawTarget* aTarget)
{
  LOGF();
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());

  NS_ENSURE_TRUE(state->mLayerManager, false);
  if (!state->mLayerManager->GetRoot()) {
    // Nothing to paint yet, just return silently
    return false;
  }
  IntSize size(aTarget->GetSize());
  nsIntRect boundRect(0, 0, size.width, size.height);
  CompositeToTarget(aTarget, &boundRect);
  return true;
}

bool EmbedLiteCompositorParent::RenderGL(TimeStamp aScheduleTime)
{
  mLastCompose = aScheduleTime;
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
    GLScreenBuffer* screen = context->Screen();
    MOZ_ASSERT(screen);
    if (screen->Size().IsEmpty() || !screen->PublishFrame(screen->Size())) {
      NS_ERROR("Failed to publish context frame");
      return false;
    }
  }

  EmbedLiteWindow* win = EmbedLiteApp::GetInstance()->GetWindowByID(mWindowId);
  if (win) {
    win->GetListener()->CompositingFinished();
  }

  return false;
}

void EmbedLiteCompositorParent::SetSurfaceSize(int width, int height)
{
  mLastViewSize.SizeTo(width, height);
  SetEGLSurfaceSize(width, height);
}

void EmbedLiteCompositorParent::SetScreenRotation(const mozilla::ScreenRotation &rotation)
{
  if (mRotation != rotation) {
    gfx::Matrix rotationMartix;
    switch (rotation) {
    case mozilla::ROTATION_90:
        // Pi / 2
        rotationMartix.PreRotate(M_PI_2l);
        rotationMartix.PreTranslate(0.0, -mLastViewSize.height);
        break;
    case mozilla::ROTATION_270:
        // 3 / 2 * Pi
        rotationMartix.PreRotate(M_PI_2l * 3);
        rotationMartix.PreTranslate(-mLastViewSize.width, 0.0);
        break;
    case mozilla::ROTATION_180:
        // Pi
        rotationMartix.PreRotate(M_PIl);
        rotationMartix.PreTranslate(-mLastViewSize.width, -mLastViewSize.height);
        break;
    default:
        break;
    }

    mWorldTransform = rotationMartix;
    mRotation = rotation;
    mUseScreenRotation = true;
    CancelCurrentCompositeTask();
    ScheduleRenderOnCompositorThread();
  }
}

void*
EmbedLiteCompositorParent::GetPlatformImage(int* width, int* height)
{
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
  CompositorParent::ScheduleResumeOnCompositorThread(mLastViewSize.width, mLastViewSize.height);
}

} // namespace embedlite
} // namespace mozilla

