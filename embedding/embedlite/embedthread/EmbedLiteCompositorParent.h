/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorParent_h
#define mozilla_layers_EmbedLiteCompositorParent_h

#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/WidgetUtils.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/CompositorChild.h"
#include "Layers.h"
#include "EmbedLiteViewThreadParent.h"

namespace mozilla {

namespace gl {
class GLContext;
}
namespace layers {
class LayerManagerComposite;
}

namespace embedlite {

class EmbedLiteCompositorParent : public mozilla::layers::CompositorParent
{
public:
  EmbedLiteCompositorParent(nsIWidget* aWidget,
                            bool aRenderToEGLSurface,
                            int aSurfaceWidth, int aSurfaceHeight,
                            uint32_t id);

  bool RenderToContext(gfx::DrawTarget* aTarget);
  bool RenderGL();
  void SetSurfaceSize(int width, int height);
  void* GetPlatformImage(int* width, int* height);
  void SuspendRendering();
  void ResumeRendering();

  void DrawWindowUnderlay(mozilla::layers::LayerManagerComposite *aManager, nsIntRect aRect);
  void DrawWindowOverlay(mozilla::layers::LayerManagerComposite *aManager, nsIntRect aRect);
  bool PreRender(layers::LayerManagerComposite* aManager);
  void ClearCompositorSurface(nscolor);

protected:
  virtual ~EmbedLiteCompositorParent();
  virtual PLayerTransactionParent*
  AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                               const uint64_t& aId,
                               TextureFactoryIdentifier* aTextureFactoryIdentifier,
                               bool* aSuccess) MOZ_OVERRIDE;
  virtual void ScheduleTask(CancelableTask*, int) MOZ_OVERRIDE;

private:
  void PrepareOffscreen();
  bool Invalidate();
  void UpdateTransformState();

  static void ClearCompositorSurfaceImpl(mozilla::gl::GLContext*, nscolor);

  uint32_t mId;
  CancelableTask* mCurrentCompositeTask;
  gfx::IntSize mLastViewSize;
  short mInitialPaintCount;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorParent_h
