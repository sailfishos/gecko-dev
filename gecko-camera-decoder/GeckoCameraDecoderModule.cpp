/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoCameraDecoderModule.h"
#include "GeckoCameraVideoDecoder.h"

namespace mozilla {

bool GeckoCameraDecoderModule::sInitialized = false;
gecko::codec::CodecManager* GeckoCameraDecoderModule::sCodecManager = nullptr;

void GeckoCameraDecoderModule::Init() {
  if (sInitialized) {
    return;
  }
  sCodecManager = gecko_codec_manager();
  if (sCodecManager) {
    sInitialized = sCodecManager->init();
  }
}

nsresult GeckoCameraDecoderModule::Startup() {
  if (!sInitialized || !sCodecManager) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder> GeckoCameraDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (sInitialized && sCodecManager) {
    RefPtr<MediaDataDecoder> decoder =
        new GeckoCameraVideoDecoder(sCodecManager, aParams);
    return decoder.forget();
  }
  return nullptr;
}

already_AddRefed<MediaDataDecoder> GeckoCameraDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  return nullptr;
}

media::DecodeSupportSet GeckoCameraDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  const auto codecType = GeckoCameraVideoDecoder::CodecTypeFromMime(aMimeType);
  if (codecType != gecko::codec::VideoCodecUnknown && sInitialized &&
      sCodecManager && sCodecManager->videoDecoderAvailable(codecType)) {
    return media::DecodeSupport::HardwareDecode;
  }
  return media::DecodeSupportSet{};
}

/* static */
already_AddRefed<PlatformDecoderModule> GeckoCameraDecoderModule::Create() {
  return MakeAndAddRef<GeckoCameraDecoderModule>();
}

}  // namespace mozilla
