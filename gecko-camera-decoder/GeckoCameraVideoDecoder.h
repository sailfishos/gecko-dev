/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GeckoCameraVideoDecoder_h_)
#define GeckoCameraVideoDecoder_h_

#include <geckocamera-codec.h>

#include <map>
#include <memory>

#include "MediaInfo.h"
#include "MediaTimer.h"
#include "PlatformDecoderModule.h"
#include "ReorderQueue.h"
#include "mozilla/Atomics.h"

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
    // The droid MediaCodec backend does not reliably accept new input after
    // EOS without rebuilding the codec.
    return false;
  }

  // VideoDecoderListener
  virtual void onDecodedYCbCrFrame(
      const gecko::camera::YCbCrFrame* frame) override;
  virtual void onDecodedGraphicBuffer(
      std::shared_ptr<gecko::camera::GraphicBuffer> buffer) override;
  virtual void onDecoderError(std::string errorDescription) override;
  virtual void onDecoderEOS() override;

  static gecko::codec::CodecType CodecTypeFromMime(const nsACString& aMimeType);

 private:
  ~GeckoCameraVideoDecoder() { MOZ_COUNT_DTOR(GeckoCameraVideoDecoder); }

  MediaResult CreateDecoder();

  enum class DecodeState : uint32_t { Idle, Active, TimedOut };
  using DecodeStateAtomic = Atomic<DecodeState, ReleaseAcquire>;

  void ProcessDecode(MediaRawData* aSample,
                     const std::shared_ptr<DecodeStateAtomic>& aDecodeState);
  void ProcessDecodedYCbCrFrame(const gecko::camera::YCbCrFrame* aFrame,
                                uint64_t aDecoderGeneration);
  void ReportError(std::string aErrorDescription,
                   uint64_t aDecoderGeneration);
  static void ReleaseInput(void* aData);
  void DrainComplete();
  gecko::codec::CodecManager* mCodecManager;
  const VideoInfo mInfo;
  const RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mImageAllocator;
  Mutex mMutex;
  Mutex mCodecControlMutex;
  uint64_t mDecoderGeneration = 0;
  const RefPtr<TaskQueue> mTaskQueue;
  bool mIsH264;
  const uint32_t mMaxRefFrames;
  ReorderQueue mReorderQueue;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  Atomic<bool, ReleaseAcquire> mIsShutDown;
  Atomic<bool, ReleaseAcquire> mError;
  Atomic<bool, ReleaseAcquire> mIgnoreCallbacks;
  bool mDecoderDrained = false;
  const RefPtr<MediaTimer<TimeStamp>> mDecodeTimer;
  const RefPtr<TaskQueue> mCommandTaskQueue;
  std::shared_ptr<gecko::codec::VideoDecoder> mDecoder;
  std::multimap<uint64_t, RefPtr<MediaRawData>> mInputFrames;
};

}  // namespace mozilla

#endif  // !defined(GeckoCameraVideoDecoder_h_)
