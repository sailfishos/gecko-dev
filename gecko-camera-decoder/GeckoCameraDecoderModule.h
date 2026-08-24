/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoCameraDecoderModule_h
#define mozilla_GeckoCameraDecoderModule_h

#include <geckocamera-codec.h>

#include "MP4Decoder.h"
#include "VPXDecoder.h"

#include "PlatformDecoderModule.h"

namespace mozilla {

class GeckoCameraDecoderModule : public PlatformDecoderModule {
  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  const char* Name() const override { return "GeckoCamera"; }

  static already_AddRefed<PlatformDecoderModule> Create();

  nsresult Startup() override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  static void Init();

 private:
  GeckoCameraDecoderModule() = default;
  virtual ~GeckoCameraDecoderModule() = default;

  static bool sInitialized;
  static gecko::codec::CodecManager* sCodecManager;
};

}  // namespace mozilla

#endif  // mozilla_GeckoCameraDecoderModule_h
