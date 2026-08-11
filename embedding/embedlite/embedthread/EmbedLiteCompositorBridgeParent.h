/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorBridgeParent_h
#define mozilla_layers_EmbedLiteCompositorBridgeParent_h

#include "base/task.h" // for CancelableRunnable
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "Units.h"
#include "EmbedLiteWindow.h"

#include <functional>
#include <list>
#include <memory>

namespace mozilla {

namespace gl {
class GLContext;
class SharedSurface;
} // gl

namespace embedlite {

class EmbedLiteWindowListener;
class PlatformFrameReadyEvent;
class PlatformFrameRetirementEvent;

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
  bool WithPlatformImage(const PlatformImageCallback& callback);
  void ClearPlatformImage();
  bool AcquirePlatformFrame(const PlatformFrameToken& token,
                            const PlatformFrameCallback& callback);
  bool ReleasePlatformFrame(const PlatformFrameRelease& release);
  bool SetPlatformFrameDeliveryEnabled(bool enabled);
  bool SetPlatformFrameListener(EmbedLitePlatformFrameListener* listener);
  void SuspendRendering();
  void ResumeRendering();
  void ScheduleForcedRenderOnCompositorThread(wr::RenderReasons aReasons);

  bool PresentOffscreenSurface(PlatformFrameToken* aToken = nullptr);
  void WebRenderComposited();

  bool GetScrollableRect(CSSRect &scrollableRect);

protected:
  friend class EmbedLitePuppetWidget;

  virtual ~EmbedLiteCompositorBridgeParent();

private:
  friend class PlatformFrameReadyEvent;
  friend class PlatformFrameRetirementEvent;

  enum class PlatformFrameState : uint8_t {
    Ready,
    Acquiring,
    Acquired,
    Released
  };

  struct PlatformFrameRecord {
    PlatformFrameToken token;
    RefPtr<mozilla::gl::GLContext> context;
    std::shared_ptr<mozilla::gl::SharedSurface> surface;
    PlatformImageDescriptor image;
    PlatformFrameFenceHandleType releaseFenceHandleType;
    void* consumerFence;
    PlatformFrameState state;
  };

  void EnsureSurfaceSizeFromWindow();
  void SchedulePlatformImageRetry();
  void MarkReadyPlatformFramesReleased();
  void PostLatestPlatformFrameReadyEvent();
  void NotifyLatestPlatformFrameReady();
  bool SchedulePlatformFrameRetirement();
  void PostPlatformFrameRetirementEvent();
  bool SchedulePlatformFrameRetirementRetry(uint32_t aDelayMs);
  void RetireReleasedPlatformFrames();
  void ScheduleForcedRender(wr::RenderReasons aReasons);

  uint32_t mWindowId;
  RefPtr<mozilla::gl::GLContext> mGLContext;
  std::shared_ptr<mozilla::gl::SharedSurface> mFrontBuffer;
  RefPtr<CancelableRunnable> mCurrentCompositeTask;
  ScreenIntPoint mSurfaceOrigin;
  Mutex mRenderMutex;
  // Serialize borrowed-image callbacks without blocking frame release.
  Mutex mPlatformImageCallbackMutex;
  // Acquire this before mRenderMutex when both are needed.
  Mutex mPlatformImageMutex;
  uint64_t mPlatformImageGeneration;
  uint64_t mNextPlatformFrameSequence;
  PlatformFrameToken mLatestPlatformFrameToken;
  std::list<PlatformFrameRecord> mPlatformFrames;
  EmbedLitePlatformFrameListener* mPlatformFrameListener;
  Atomic<bool> mPlatformFrameDeliveryEnabled;
  bool mPlatformFrameDeliveryStopPending;
  Atomic<bool> mPlatformImageRetryPending;
  Atomic<bool> mPlatformFrameRetirementPending;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteCompositorBridgeParent);
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorBridgeParent_h
