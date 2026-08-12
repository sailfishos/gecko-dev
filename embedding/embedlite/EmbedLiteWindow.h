/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_WINDOW_H
#define EMBED_LITE_WINDOW_H

#include <stdint.h>

#include "mozilla/Types.h"
#include "nsRect.h"
#include <functional>

namespace mozilla {
namespace embedlite {

// NB: these must match up with pseudo-enum in nsIScreen.idl.
enum ScreenRotation {
  ROTATION_0 = 0,
  ROTATION_90,
  ROTATION_180,
  ROTATION_270,

  ROTATION_COUNT
};

class EmbedLiteApp;
class PEmbedLiteWindowParent;
class EmbedLiteWindowParent;
class EmbedLiteChromeSession;

enum class PlatformImageHandleType : uint8_t {
  EGLImage
};

enum class PlatformImageTextureTarget : uint8_t {
  Texture2D,
  ExternalOES
};

struct PlatformImageDescriptor final {
  PlatformImageHandleType handleType;
  void* handle;
  PlatformImageTextureTarget textureTarget;
  int32_t width;
  int32_t height;
};

using PlatformImageCallback =
  std::function<void(const PlatformImageDescriptor&)>;

struct PlatformFrameToken final {
  uint64_t epoch;
  uint64_t sequence;

  bool IsValid() const { return epoch != 0 && sequence != 0; }
  bool operator==(const PlatformFrameToken& other) const {
    return epoch == other.epoch && sequence == other.sequence;
  }
};

enum class PlatformFrameFenceHandleType : uint8_t {
  NoHandle,
  EGLSync
};

struct PlatformFrameDescriptor final {
  PlatformFrameToken token;
  PlatformImageDescriptor image;
  PlatformFrameFenceHandleType releaseFenceHandleType;
};

struct PlatformFrameRelease final {
  PlatformFrameToken token;
  PlatformFrameFenceHandleType fenceHandleType;
  void* fenceHandle;
};

using PlatformFrameCallback =
  std::function<bool(const PlatformFrameDescriptor&)>;

class EmbedLitePlatformFrameListener
{
public:
  // A new tokenized platform frame is available. This is called directly
  // from Gecko's render thread. Consumers must schedule acquisition on their
  // own render thread rather than acquiring from this callback.
  virtual void PlatformFrameReady(const PlatformFrameToken&) = 0;

  // All tokenized frames have been retired after delivery was disabled.
  // This is called directly from Gecko's render thread.
  virtual void PlatformFrameDeliveryStopped() = 0;

protected:
  virtual ~EmbedLitePlatformFrameListener() = default;
};

class EmbedLiteWindowListener
{
public:
  // Window was initialized and is ready to process API calls.
  virtual void WindowInitialized() {}

  // Window was fully destroyed.
  virtual void WindowDestroyed() {}

  // Notify embedder that gecko compositor for a given window has been created.
  // This function will be called on from a thread on which the compositor was
  // created.
  virtual void CompositorCreated() {}

  // Notify embedder that gecko has finished compositing current frame.
  // This function is called directly from gecko compositor thread.
  virtual void CompositingFinished() {}

  // Will be always called from the compositor thread.
  virtual void DrawOverlay(const nsIntRect& aRect) {}

  // Will be always called from the compositor thread.
  virtual bool PreRender() { return true; }
};

class EmbedLiteChromeWindowListener
{
public:
  // Called when the opt-in Gecko chrome AppWindow could not be created. The
  // regular WindowDestroyed callback follows after asynchronous teardown.
  virtual void ChromeWindowInitializationFailed() = 0;

protected:
  virtual ~EmbedLiteChromeWindowListener() = default;
};

class EmbedLiteWindow {
public:
  EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent*, uint32_t id);
  EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent*, uint32_t id,
                  bool chromeHosted);

  // PEmbedLiteWindow:
  virtual void SetSize(int width, int height);

  virtual uint32_t GetUniqueID() const;
  bool IsChromeHosted() const;

  // Returns a borrowed session only for windows created with
  // CreateChromeWindow(). The pointer becomes invalid when the window is
  // destroyed and does not change EmbedLiteWindow's legacy ABI.
  MOZ_EXPORT EmbedLiteChromeSession* GetChromeSession();

  virtual void SetContentOrientation(mozilla::embedlite::ScreenRotation);
  virtual void ScheduleUpdate();
  virtual void SuspendRendering();
  virtual void ResumeRendering();
  // The descriptor and its handle are borrowed for the duration of the
  // synchronous callback. Consumers must import the handle before returning.
  virtual bool WithPlatformImage(const PlatformImageCallback& callback);
  virtual void ClearPlatformImage();

  // Acquiring a frame pins its exact image until ReleasePlatformFrame. The
  // consumer must release every accepted frame before destroying the window.
  MOZ_EXPORT bool AcquirePlatformFrame(
    const PlatformFrameToken& token,
    const PlatformFrameCallback& callback);
  // Release is valid only after AcquirePlatformFrame returns true. Fence
  // ownership transfers to Gecko only when this returns true. NoHandle
  // requires a null handle and means that no GPU reads remain outstanding.
  // An EGLSync must be created on the compatible EGLDisplay after the last
  // sampling draw, and the consumer must flush its GL context before release.
  MOZ_EXPORT bool ReleasePlatformFrame(
    const PlatformFrameRelease& release);
  // Token notifications are disabled by default so legacy consumers retain
  // their existing frame and teardown behavior. After disabling an enabled
  // stream, release every acquired token and wait for
  // PlatformFrameDeliveryStopped before destroying the window or listener.
  // Disable while the compositor and WebRender bridge are still alive. If
  // rendering infrastructure is already shutting down, do not time out or
  // destroy the window, listener, or consumer resources; keep them alive
  // until PlatformFrameDeliveryStopped arrives.
  MOZ_EXPORT bool SetPlatformFrameDeliveryEnabled(bool enabled);
  // The listener is not owned and must remain alive until delivery stops.
  MOZ_EXPORT bool SetPlatformFrameListener(
    EmbedLitePlatformFrameListener* listener);

protected:
  friend class EmbedLiteApp;

  virtual ~EmbedLiteWindow();
  // Request the window to be destroyed. Once this async process is done
  // EmbedLiteWindowListener::WindowDestroyed will be called. This interface
  // should only be used by EmbedLiteApp. EmbedLite users should destroy
  // EmbedLiteWindowss by calling EmbedLiteApp::DestroyWindow.
  void Destroy();

private:
  friend class EmbedLiteWindowParent;

  // EmbedLiteWindowss are supposed to be destroyed through EmbedLiteApp::DestroyWindow.
  void Destroyed();

  EmbedLiteApp* mApp;
  EmbedLiteWindowParent* mWindowParent;
  const uint32_t mUniqueID;
};

} // namespace embedlite
} // namespace mozilla

#endif
