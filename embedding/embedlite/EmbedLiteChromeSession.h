/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_CHROME_SESSION_H
#define EMBED_LITE_CHROME_SESSION_H

#include <stdint.h>

namespace mozilla {
namespace embedlite {

class EmbedTouchInput;

class EmbedLiteChromeSessionListener
{
public:
  // The strings passed to these callbacks are borrowed and only valid for the
  // duration of the callback.
  virtual void OnLocationChanged(const char* aLocation, bool aCanGoBack,
                                 bool aCanGoForward) {}
  virtual void OnLoadStarted(const char* aLocation) {}
  virtual void OnLoadFinished() {}
  virtual void OnLoadProgress(int32_t aProgress, int64_t aCurrent,
                              int64_t aTotal) {}
  virtual void OnTitleChanged(const char16_t* aTitle) {}

  // The session is no longer usable after this callback returns.
  virtual void ChromeSessionDestroyed() {}

protected:
  virtual ~EmbedLiteChromeSessionListener() = default;
};

// A borrowed facade for the chrome-hosted window's remote browser. The
// EmbedLiteWindow retains ownership; callers must not retain this pointer
// beyond the window's destruction callback.
class EmbedLiteChromeSession
{
public:
  // This does not take ownership. Passing nullptr detaches the listener
  // synchronously and prevents subsequent callbacks.
  virtual void SetListener(EmbedLiteChromeSessionListener* aListener) = 0;

  // A true result means the request was accepted for IPC delivery.
  virtual bool LoadURL(const char* aURL, bool aFromExternal) = 0;
  virtual bool GoBack(bool aRequireUserInteraction, bool aUserActivation) = 0;
  virtual bool GoForward(bool aRequireUserInteraction,
                         bool aUserActivation) = 0;
  virtual bool StopLoad() = 0;
  virtual bool Reload(bool aHardReload) = 0;
  virtual bool SetActive(bool aActive) = 0;
  virtual bool SetFocused(bool aFocused) = 0;
  virtual bool ReceiveInputEvent(const EmbedTouchInput& aEvent) = 0;

protected:
  virtual ~EmbedLiteChromeSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif
