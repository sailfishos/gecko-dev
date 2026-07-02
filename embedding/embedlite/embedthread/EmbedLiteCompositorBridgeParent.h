/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorBridgeParent_h
#define mozilla_layers_EmbedLiteCompositorBridgeParent_h

#include "base/task.h" // for CancelableRunnable
#include "mozilla/Mutex.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "Units.h"

#include <functional>
#include <memory>

namespace mozilla {

namespace gl {
class GLContext;
class SharedSurface;
} // gl

namespace embedlite {

class EmbedLiteWindowListener;

class EmbedLiteCompositorBridgeParent : public mozilla::layers::CompositorBridgeParent
{
public:
  EmbedLiteCompositorBridgeParent(uint32_t windowId,
                                  mozilla::layers::CompositorManagerParent *aManager,
                                  uint32_t aNamespace,
                                  CSSToLayoutDeviceScale aScale,
                                  const TimeDuration &aVsyncRate,
                                  const CompositorOptions &aOptions,
                                  bool aRenderToEGLSurface,
                                  const gfx::IntSize &aSurfaceSize,
                                  uint64_t aInnerWindowId);

  void SetWebRenderGLContext(mozilla::gl::GLContext* aGL) override;
  bool CompositeToDefaultTarget(mozilla::layers::WebRenderBridgeParent* aWrBridge,
                                VsyncId aId,
                                wr::RenderReasons aReasons) override;
  void SetSurfaceRect(int x, int y, int width, int height);
  void* GetPlatformImage(int* width, int* height);
  void GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback);
  void ClearPlatformImage();
  void SuspendRendering();
  void ResumeRendering();
  void ScheduleForcedRenderOnCompositorThread(wr::RenderReasons aReasons);

  bool PresentOffscreenSurface();
  void WebRenderComposited();

  bool GetScrollableRect(CSSRect &scrollableRect);

protected:
  friend class EmbedLitePuppetWidget;

  virtual ~EmbedLiteCompositorBridgeParent();

private:
  void EnsureSurfaceSizeFromWindow();
  void PrepareOffscreen();
  void ScheduleForcedRender(wr::RenderReasons aReasons);

  uint32_t mWindowId;
  RefPtr<mozilla::gl::GLContext> mGLContext;
  std::shared_ptr<mozilla::gl::SharedSurface> mFrontBuffer;
  RefPtr<CancelableRunnable> mCurrentCompositeTask;
  ScreenIntPoint mSurfaceOrigin;
  Mutex mRenderMutex;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteCompositorBridgeParent);
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorBridgeParent_h
