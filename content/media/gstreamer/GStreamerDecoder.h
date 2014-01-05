/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GStreamerDecoder_h_)
#define GStreamerDecoder_h_

#include "MediaDecoder.h"
#include "nsXPCOMStrings.h"

namespace mozilla {

class GStreamerReader;

class GStreamerDecoder : public MediaDecoder
{
public:
  virtual MediaDecoder* Clone() { return new GStreamerDecoder(); }
  virtual MediaDecoderStateMachine* CreateStateMachine();
  static bool CanHandleMediaType(const nsACString& aMIMEType, const nsAString* aCodecs);
  virtual nsresult Load(nsIStreamListener** aListener,
                        MediaDecoder* aCloneDonor) MOZ_OVERRIDE;
  virtual void ReleaseStateMachine();
  void Suspend() MOZ_OVERRIDE;
private:
  GStreamerReader* mReader;
};

} // namespace mozilla

#endif
