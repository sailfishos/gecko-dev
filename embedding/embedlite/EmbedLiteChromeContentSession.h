/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_CHROME_CONTENT_SESSION_H
#define EMBED_LITE_CHROME_CONTENT_SESSION_H

#include <stdint.h>

namespace mozilla {
namespace embedlite {

enum class EmbedLiteChromeMouseType : uint8_t
{
  Move,
  Down,
  Up
};

struct EmbedLiteChromeContentState
{
  // Strings are borrowed for the duration of OnContentStateChanged().
  uint64_t tabId;
  uint64_t persistentId;
  uint64_t revision;
  uint64_t locationRevision;
  const char* securityStatus;
  uint32_t securityState;
  bool fullscreen;
  bool firstPaint;
  int32_t firstPaintX;
  int32_t firstPaintY;
  uint32_t scrollWidth;
  uint32_t scrollHeight;
  int32_t scrollX;
  int32_t scrollY;
  double viewportX;
  double viewportY;
  double viewportWidth;
  double viewportHeight;
};

class EmbedLiteChromeContentSessionListener
{
public:
  virtual void OnContentStateChanged(
    const EmbedLiteChromeContentState& aState) {}
  // aLocationRevision correlates the payload with the exact tab snapshot
  // document that emitted it; embedders must drop a later stale delivery.
  virtual void RecvAsyncMessage(uint64_t aTabId, uint64_t aPersistentId,
                                uint64_t aLocationRevision,
                                const char16_t* aName,
                                const char16_t* aJSON) {}
  // Informational notification: Gecko removes this logical tab immediately.
  // The listener must not answer by calling CloseTab().
  virtual void OnWindowCloseRequested(uint64_t aTabId,
                                      uint64_t aPersistentId) {}
  // Completes a CloseTab() request. aClosed is true only after the logical
  // tab has actually been removed.
  virtual void OnTabCloseResult(uint64_t aTabId, bool aClosed) {}
  virtual void ChromeContentSessionDestroyed() {}

protected:
  virtual ~EmbedLiteChromeContentSessionListener() = default;
};

class EmbedLiteChromeContentSession
{
public:
  virtual void SetContentListener(
    EmbedLiteChromeContentSessionListener* aListener) = 0;

  // Registrations are window-wide, idempotent, and are replayed to browsers
  // created or rematerialized after the call.
  virtual bool LoadFrameScript(const char* aURI) = 0;
  virtual bool AddMessageListener(const char* aName) = 0;
  virtual bool RemoveMessageListener(const char* aName) = 0;
  virtual bool SendAsyncMessage(uint64_t aTabId, const char16_t* aName,
                                const char16_t* aJSON) = 0;

  virtual bool SendMouseEvent(uint64_t aTabId,
                              EmbedLiteChromeMouseType aType,
                              int32_t aX, int32_t aY, uint64_t aTime,
                              uint32_t aButton, uint32_t aButtons,
                              uint32_t aModifiers,
                              uint32_t aClickCount) = 0;
  virtual bool SendWheelEvent(uint64_t aTabId, int32_t aX, int32_t aY,
                              uint64_t aTime, double aDeltaX,
                              double aDeltaY, uint32_t aDeltaMode,
                              uint32_t aModifiers) = 0;
  virtual bool ScrollTo(uint64_t aTabId, int32_t aX, int32_t aY) = 0;
  virtual bool ScrollBy(uint64_t aTabId, int32_t aX, int32_t aY) = 0;
  virtual bool ZoomToRect(uint64_t aTabId, float aX, float aY,
                          float aWidth, float aHeight) = 0;

  virtual bool SetDesktopMode(uint64_t aTabId, bool aDesktopMode) = 0;
  virtual bool SetJavascriptEnabled(bool aEnabled) = 0;
  virtual bool SetThrottlePainting(uint64_t aTabId, bool aThrottle) = 0;
  virtual bool SuspendTimeouts(uint64_t aTabId) = 0;
  virtual bool ResumeTimeouts(uint64_t aTabId) = 0;
  virtual bool SetHttpUserAgent(uint64_t aTabId,
                                const char16_t* aUserAgent) = 0;
  virtual bool SetMargins(uint64_t aTabId, int32_t aTop, int32_t aRight,
                          int32_t aBottom, int32_t aLeft) = 0;
  virtual bool SetSafeAreaInsets(uint64_t aTabId, int32_t aTop,
                                 int32_t aRight, int32_t aBottom,
                                 int32_t aLeft) = 0;
  virtual bool SetDynamicToolbarHeight(uint64_t aTabId,
                                       int32_t aHeight) = 0;
  virtual bool SetScreenProperties(int32_t aDepth, float aDensity,
                                   float aDpi) = 0;

protected:
  virtual ~EmbedLiteChromeContentSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif
