/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GeckoCameraVideoDecoder_h_)
#define GeckoCameraVideoDecoder_h_

#include <map>
#include <geckocamera-codec.h>

#include "MediaInfo.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"
#include "PlatformDecoderModule.h"
#include "ReorderQueue.h"
#include "MediaTimer.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(GeckoCameraVideoDecoder, MediaDataDecoder);

class GeckoCameraVideoDecoder final
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<GeckoCameraVideoDecoder>,
      public gecko::codec::VideoDecoderListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoCameraVideoDecoder, final);

  GeckoCameraVideoDecoder(gecko::codec::CodecManager* manager,
      const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;

  RefPtr<ShutdownPromise> Shutdown() override;

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;

  RefPtr<DecodePromise> Drain() override;

  RefPtr<FlushPromise> Flush() override;

  ConversionRequired NeedsConversion() const override;

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override {
    return true;
  }

  nsCString GetDescriptionName() const override {
    return "gecko-camera video decoder"_ns;
  }

  nsCString GetCodecName() const override;

  bool SupportDecoderRecycling() const override {
    return true;
  }

  // VideoDecoderListener
  virtual void onDecodedYCbCrFrame(const gecko::camera::YCbCrFrame *frame) override;
  virtual void onDecodedGraphicBuffer(std::shared_ptr<gecko::camera::GraphicBuffer> buffer) override;
  virtual void onDecoderError(std::string errorDescription) override;
  virtual void onDecoderEOS() override;

  static gecko::codec::CodecType CodecTypeFromMime(const nsACString& aMimeType);

 private:
  ~GeckoCameraVideoDecoder() {
    MOZ_COUNT_DTOR(GeckoCameraVideoDecoder);
  }

  MediaResult CreateDecoder();
  void ProcessDecode(MediaRawData* aSample);
  void DrainComplete();
  gecko::codec::CodecManager* mCodecManager;
  const CreateDecoderParams mParams;
  const VideoInfo mInfo;
  const RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mImageAllocator;
  Mutex mMutex;
  const RefPtr<TaskQueue> mTaskQueue;
  bool mIsH264;
  const uint32_t mMaxRefFrames;
  ReorderQueue mReorderQueue;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  Atomic<bool, ReleaseAcquire> mIsShutDown;
  Atomic<bool, ReleaseAcquire> mError;
  bool mDecoderDrained = false;
  const RefPtr<MediaTimer> mDecodeTimer;
  const RefPtr<TaskQueue> mCommandTaskQueue;
  std::string mErrorDescription;
  std::shared_ptr<gecko::codec::VideoDecoder> mDecoder;
  std::map<uint64_t,RefPtr<MediaRawData>> mInputFrames;
};

}  // namespace mozilla

#endif  // !defined(GeckoCameraVideoDecoder_h_)
