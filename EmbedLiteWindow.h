/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_WINDOW_H
#define EMBED_LITE_WINDOW_H

#include <stdint.h>

#include "nsRect.h"

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
class EmbedLiteWindowBaseParent;

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
  virtual void DrawUnderlay() {}

  // Will be always called from the compositor thread.
  virtual void DrawOverlay(const nsIntRect& aRect) {}

  // Will be always called from the compositor thread.
  virtual bool PreRender() { return true; }

  // Request GL implementation specific surface and context objects from the
  // platform. This can be EGLSurface / EGLContext in case of EGL, or
  // GLXContext / GLXDrawable in case of GLX.
  //
  // This function will only be called when embedlite.compositor.external_gl_context
  // preference is set to true.
  //
  // This funtion will be called directly from gecko Compositor thread. The embedder
  // must ensure this function will be thread safe.
  virtual bool RequestGLContext(void*& surface, void*& context) { return false; }
};

class EmbedLiteWindow {
public:
  EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent*, uint32_t id);

  virtual void SetListener(EmbedLiteWindowListener* aListener);
  virtual EmbedLiteWindowListener* const GetListener() const;

  // PEmbedLiteWindow:
  virtual void SetSize(int width, int height);

  virtual uint32_t GetUniqueID() const;

  virtual void SetContentOrientation(mozilla::embedlite::ScreenRotation);
  virtual void ScheduleUpdate();
  virtual void SuspendRendering();
  virtual void ResumeRendering();
  virtual void* GetPlatformImage(int* width, int* height);

protected:
  friend class EmbedLiteApp;

  virtual ~EmbedLiteWindow();
  // Request the window to be destroyed. Once this async process is done
  // EmbedLiteWindowListener::WindowDestroyed will be called. This interface
  // should only be used by EmbedLiteApp. EmbedLite users should destroy
  // EmbedLiteWindowss by calling EmbedLiteApp::DestroyWindow.
  void Destroy();

private:
  friend class EmbedLiteWindowBaseParent;

  // EmbedLiteWindowss are supposed to be destroyed through EmbedLiteApp::DestroyWindow.
  void Destroyed();

  EmbedLiteApp* mApp;
  EmbedLiteWindowListener* mListener;
  EmbedLiteWindowBaseParent* mWindowParent;
  const uint32_t mUniqueID;
};

} // namespace embedlite
} // namespace mozilla

#endif
