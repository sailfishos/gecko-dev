/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteWebRenderBridgeParent_h
#define mozilla_layers_EmbedLiteWebRenderBridgeParent_h

#include "Layers.h"
#include "base/task.h" // for CancelableRunnable
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WidgetUtils.h"
//#include "mozilla/layers/WebRenderBridgeChild.h"

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"
#include "mozilla/layers/WebRenderBridgeParent.h"

#include <functional>

namespace mozilla {

namespace embedlite {

class EmbedLiteWindowListener;

class EmbedLiteWebRenderBridgeParent : public mozilla::layers::WebRenderBridgeParent
{
public:
  EmbedLiteWebRenderBridgeParent(mozilla::layers::CompositorBridgeParentBase* aCompositorBridge,
                                 const wr::PipelineId& aPipelineId,
                                 widget::CompositorWidget* aWidget,
                                 mozilla::layers::CompositorVsyncScheduler* aScheduler,
                                 RefPtr<wr::WebRenderAPI>&& aApi,
                                 RefPtr<mozilla::layers::AsyncImagePipelineManager>&& aImageMgr,
                                 TimeDuration aVsyncRate);

  void SetSurfaceRect(int x, int y, int width, int height);
  void* GetPlatformImage(int* width, int* height);
  void GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback);
  void SuspendRendering();
  void ResumeRendering();

  void PresentOffscreenSurface();

  bool GetScrollableRect(CSSRect &scrollableRect);

protected:
  friend class EmbedLitePuppetWidget;

  virtual ~EmbedLiteWebRenderBridgeParent();
  void CompositeToDefaultTarget(VsyncId aId, wr::RenderReasons aReasons);

private:
  void PrepareOffscreen();

  uint32_t mWindowId;
/*
  RefPtr<CancelableRunnable> mCurrentCompositeTask;
  ScreenIntPoint mSurfaceOrigin;
  bool mUseExternalGLContext;
*/
  Mutex mRenderMutex;
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWebRenderBridgeParent);
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteWebRenderBridgeParent_h
