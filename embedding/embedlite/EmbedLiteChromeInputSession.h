/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_CHROME_INPUT_SESSION_H
#define EMBED_LITE_CHROME_INPUT_SESSION_H

#include <stdint.h>

namespace mozilla {
namespace embedlite {

class EmbedLiteChromeInputSessionListener
{
public:
  // The strings are borrowed and only valid for the duration of the callback.
  virtual void OnInputContextChanged(
    int32_t aEnabled, int32_t aOpen,
    const char16_t* aInputType, const char16_t* aInputMode,
    const char16_t* aActionHint, int32_t aCause,
    int32_t aFocusChange) = 0;

  // The session is no longer usable after this callback returns.
  virtual void ChromeInputSessionDestroyed() = 0;

protected:
  virtual ~EmbedLiteChromeInputSessionListener() = default;
};

// A borrowed input facade for a chrome-hosted window. EmbedLiteWindow retains
// ownership; callers must not retain this pointer beyond window destruction.
class EmbedLiteChromeInputSession
{
public:
  // This does not take ownership. Passing nullptr detaches the listener
  // synchronously and prevents subsequent callbacks.
  virtual void SetInputListener(
    EmbedLiteChromeInputSessionListener* aListener) = 0;

  // Text strings are UTF-8. Replacement offsets are signed UTF-16 code-unit
  // offsets from the current selection start. A true result means that IPC
  // accepted the request.
  virtual bool SendTextEvent(const char* aCommit, const char* aPreEdit,
                             int32_t aReplacementStart,
                             int32_t aReplacementLength) = 0;
  // As above, but aReplacementOffset is an absolute UTF-16 code-unit offset.
  // This avoids querying a potentially stale remote selection after an
  // acknowledged content-process edit.
  virtual bool SendTextEventAtOffset(const char* aCommit,
                                     const char* aPreEdit,
                                     uint32_t aReplacementOffset,
                                     int32_t aReplacementLength) = 0;
  virtual bool SendKeyPress(int32_t aDomKeyCode, int32_t aModifiers,
                            int32_t aCharCode) = 0;
  virtual bool SendKeyRelease(int32_t aDomKeyCode, int32_t aModifiers,
                              int32_t aCharCode) = 0;

protected:
  virtual ~EmbedLiteChromeInputSession() = default;
};

} // namespace embedlite
} // namespace mozilla

#endif
