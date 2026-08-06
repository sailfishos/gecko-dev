/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"
#include "mozilla/Unused.h"

#include "GeckoCameraVideoDecoder.h"
#include "AnnexB.h"
#include "H264.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "nsThreadUtils.h"

#undef LOG
#undef LOGEX
#define LOG(...) DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)
#define LOGEX(_this, ...) \
  DDMOZ_LOGEX(_this, sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)

namespace mozilla {

GeckoCameraVideoDecoder::GeckoCameraVideoDecoder(
    gecko::codec::CodecManager* manager,
    const CreateDecoderParams& aParams)
    : mCodecManager(manager),
      mInfo(aParams.VideoConfig()),
      mImageContainer(aParams.mImageContainer),
      mImageAllocator(aParams.mKnowsCompositor),
      mMutex("GeckoCameraVideoDecoder::mMutex"),
      mCodecControlMutex("GeckoCameraVideoDecoder::mCodecControlMutex"),
      mTaskQueue(TaskQueue::Create(
          GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
          "GeckoCameraVideoDecoder")),
      mIsH264(MP4Decoder::IsH264(mInfo.mMimeType)),
      mMaxRefFrames(mIsH264 ? H264::HasSPS(mInfo.mExtraData)
                                  ? H264::ComputeMaxRefFrames(mInfo.mExtraData)
                                  : 16
                            : 0),
      mIsShutDown(false),
      mError(false),
      mIgnoreCallbacks(false),
      mDecodeTimer(new MediaTimer<TimeStamp>()),
      mCommandTaskQueue(CreateMediaDecodeTaskQueue("GeckoCameraVideoDecoder")) {
  MOZ_COUNT_CTOR(GeckoCameraVideoDecoder);
  LOG("GeckoCameraVideoDecoder - mMaxRefFrames=%u", mMaxRefFrames);
}

RefPtr<MediaDataDecoder::InitPromise> GeckoCameraVideoDecoder::Init() {
  MediaResult rv = CreateDecoder();
  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  }
  return InitPromise::CreateAndReject(rv, __func__);
}

RefPtr<ShutdownPromise> GeckoCameraVideoDecoder::Shutdown() {
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(
      mTaskQueue, __func__, [self, this]() -> RefPtr<ShutdownPromise> {
    LOG("Shutdown");
    mIsShutDown = true;
    mDecodeTimer->Cancel();
    mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mDecoderDrained = false;

    std::shared_ptr<gecko::codec::VideoDecoder> decoder;
    {
      MutexAutoLock lock(mMutex);
      ++mDecoderGeneration;
      decoder = mDecoder;
      mDecoder.reset();
      mInputFrames.clear();
      mReorderQueue.Clear();
    }

    if (decoder) {
      MutexAutoLock lock(mCodecControlMutex);
      decoder->stop();
      decoder->setListener(nullptr);
    }

    return mCommandTaskQueue->BeginShutdown()->Then(
        mTaskQueue, __func__,
        [self](bool) { return self->mTaskQueue->BeginShutdown(); },
        [self](bool) { return self->mTaskQueue->BeginShutdown(); });
  });
}

RefPtr<MediaDataDecoder::DecodePromise> GeckoCameraVideoDecoder::Decode(
    MediaRawData* aSample) {
  LOG("mp4 input sample %p pts %lld duration %lld us%s %zu bytes", aSample,
      aSample->mTime.ToMicroseconds(), aSample->mDuration.ToMicroseconds(),
      aSample->mKeyframe ? " keyframe" : "", aSample->Size());

  RefPtr<MediaRawData> sample = aSample;
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this, sample] {
    RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
    auto decodeState = std::make_shared<DecodeStateAtomic>(DecodeState::Active);
    ProcessDecode(sample, decodeState);
    mDecodeTimer->Cancel();
    return p;
  });
}

RefPtr<MediaDataDecoder::DecodePromise> GeckoCameraVideoDecoder::Drain() {
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this] {
    LOG("Drain");
    if (!mDrainPromise.IsEmpty()) {
      RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
      return p;
    }

    RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);

    if (mIsShutDown) {
      mDrainPromise.Reject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
      return p;
    }

    if (mError) {
      mDrainPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
      return p;
    }

    std::shared_ptr<gecko::codec::VideoDecoder> decoder;
    {
      MutexAutoLock lock(mMutex);
      decoder = mDecoder;
    }

    if (!decoder) {
      DrainComplete();
      return p;
    }

    if (mDecoderDrained) {
      LOG("Drain already complete");
      DrainComplete();
      return p;
    }

    {
      MutexAutoLock lock(mCodecControlMutex);
      decoder->drain();
    }
    return p;
  });
}

RefPtr<MediaDataDecoder::FlushPromise> GeckoCameraVideoDecoder::Flush() {
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this] {
    LOG("Flush");
    mDecodeTimer->Cancel();
    mDecoderDrained = false;
    mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

    std::shared_ptr<gecko::codec::VideoDecoder> decoder;
    mIgnoreCallbacks = true;
    {
      MutexAutoLock lock(mMutex);
      ++mDecoderGeneration;
      decoder = mDecoder;
    }
    if (decoder) {
      MutexAutoLock lock(mCodecControlMutex);
      decoder->flush();
    }

    {
      MutexAutoLock lock(mMutex);
      mReorderQueue.Clear();
      mInputFrames.clear();
      // Clear a decoder error that may occur during flushing.
      mError = false;
    }
    mIgnoreCallbacks = false;
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

MediaDataDecoder::ConversionRequired GeckoCameraVideoDecoder::NeedsConversion()
    const {
  return mIsH264 ? ConversionRequired::kNeedAnnexB
                 : ConversionRequired::kNeedNone;
}

nsCString GeckoCameraVideoDecoder::GetCodecName() const {
  const nsACString& mimeType = mInfo.mMimeType;
  if (MP4Decoder::IsH264(mimeType)) {
    return "h264"_ns;
  }
  if (VPXDecoder::IsVP8(mimeType)) {
    return "vp8"_ns;
  }
  if (VPXDecoder::IsVP9(mimeType)) {
    return "vp9"_ns;
  }
  return "unknown"_ns;
}

void GeckoCameraVideoDecoder::onDecodedYCbCrFrame(
    const gecko::camera::YCbCrFrame* aFrame) {
  uint64_t decoderGeneration;
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks) {
      return;
    }
    decoderGeneration = mDecoderGeneration;
  }
  ProcessDecodedYCbCrFrame(aFrame, decoderGeneration);
}

void GeckoCameraVideoDecoder::ProcessDecodedYCbCrFrame(
    const gecko::camera::YCbCrFrame* aFrame,
    uint64_t aDecoderGeneration) {
  MOZ_ASSERT(aFrame, "YCbCrFrame is null");

  LOG("onDecodedFrame %llu",
      static_cast<unsigned long long>(aFrame->timestampUs));

  RefPtr<MediaRawData> inputFrame;
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks ||
        aDecoderGeneration != mDecoderGeneration) {
      return;
    }
    auto iter = mInputFrames.find(aFrame->timestampUs);
    if (iter == mInputFrames.end()) {
      LOG("Couldn't find input frame with timestamp %llu",
          static_cast<unsigned long long>(aFrame->timestampUs));
      return;
    }
    inputFrame = iter->second;
    mInputFrames.erase(iter);
  }

  VideoData::YCbCrBuffer buffer;
  // Y plane.
  buffer.mPlanes[0].mData = const_cast<uint8_t*>(aFrame->y);
  buffer.mPlanes[0].mStride = aFrame->yStride;
  buffer.mPlanes[0].mWidth = aFrame->width;
  buffer.mPlanes[0].mHeight = aFrame->height;
  buffer.mPlanes[0].mSkip = 0;
  // Cb plane.
  buffer.mPlanes[1].mData = const_cast<uint8_t*>(aFrame->cb);
  buffer.mPlanes[1].mStride = aFrame->cStride;
  buffer.mPlanes[1].mWidth = (aFrame->width + 1) / 2;
  buffer.mPlanes[1].mHeight = (aFrame->height + 1) / 2;
  buffer.mPlanes[1].mSkip = aFrame->chromaStep - 1;
  // Cr plane.
  buffer.mPlanes[2].mData = const_cast<uint8_t*>(aFrame->cr);
  buffer.mPlanes[2].mStride = aFrame->cStride;
  buffer.mPlanes[2].mWidth = (aFrame->width + 1) / 2;
  buffer.mPlanes[2].mHeight = (aFrame->height + 1) / 2;
  buffer.mPlanes[2].mSkip = aFrame->chromaStep - 1;

  buffer.mYUVColorSpace = DefaultColorSpace({aFrame->width, aFrame->height});
  buffer.mColorDepth = gfx::ColorDepth::COLOR_8;
  buffer.mColorRange = gfx::ColorRange::LIMITED;
  buffer.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

  auto result = VideoData::CreateAndCopyData(
      mInfo, mImageContainer, inputFrame->mOffset,
      inputFrame->mTime, inputFrame->mDuration,
      buffer, inputFrame->mKeyframe, inputFrame->mTimecode,
      mInfo.ScaledImageRect(aFrame->width, aFrame->height), mImageAllocator);
  if (result.isErr()) {
    MediaResult error = result.unwrapErr();
    ReportError(error.Message().get(), aDecoderGeneration);
    return;
  }
  RefPtr<VideoData> data = result.unwrap();

  MutexAutoLock lock(mMutex);
  if (mIsShutDown || mIgnoreCallbacks ||
      aDecoderGeneration != mDecoderGeneration) {
    return;
  }
  mReorderQueue.Push(std::move(data));
}

void GeckoCameraVideoDecoder::onDecodedGraphicBuffer(
    std::shared_ptr<gecko::camera::GraphicBuffer> buffer) {
  uint64_t decoderGeneration;
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks) {
      return;
    }
    decoderGeneration = mDecoderGeneration;
  }

  std::shared_ptr<const gecko::camera::YCbCrFrame> frame = buffer->mapYCbCr();
  if (frame) {
    ProcessDecodedYCbCrFrame(frame.get(), decoderGeneration);
  } else {
    ReportError("Couldn't map GraphicBuffer", decoderGeneration);
  }
}

void GeckoCameraVideoDecoder::onDecoderError(std::string errorDescription) {
  uint64_t decoderGeneration;
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks) {
      return;
    }
    decoderGeneration = mDecoderGeneration;
  }
  ReportError(std::move(errorDescription), decoderGeneration);
}

void GeckoCameraVideoDecoder::ReportError(std::string aErrorDescription,
                                          uint64_t aDecoderGeneration) {
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks ||
        aDecoderGeneration != mDecoderGeneration) {
      return;
    }
    mError = true;
    mInputFrames.clear();
  }

  LOG("Decoder error %s", aErrorDescription.c_str());

  RefPtr<GeckoCameraVideoDecoder> self = this;
  nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "GeckoCameraVideoDecoder::onDecoderError",
      [self, decoderGeneration = aDecoderGeneration]() {
        {
          MutexAutoLock lock(self->mMutex);
          if (self->mIsShutDown || self->mIgnoreCallbacks ||
              decoderGeneration != self->mDecoderGeneration) {
            return;
          }
        }
        self->mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                            __func__);
        self->mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                           __func__);
      }));
  if (NS_FAILED(rv)) {
    LOG("Couldn't dispatch decoder error");
  }
  Unused << rv;
}

void GeckoCameraVideoDecoder::onDecoderEOS() {
  uint64_t decoderGeneration;
  {
    MutexAutoLock lock(mMutex);
    if (mIsShutDown || mIgnoreCallbacks) {
      return;
    }
    decoderGeneration = mDecoderGeneration;
  }

  LOG("Decoder EOS");

  RefPtr<GeckoCameraVideoDecoder> self = this;
  nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "GeckoCameraVideoDecoder::onDecoderEOS",
      [self, decoderGeneration]() {
        {
          MutexAutoLock lock(self->mMutex);
          if (self->mIsShutDown || self->mIgnoreCallbacks ||
              decoderGeneration != self->mDecoderGeneration) {
            return;
          }
        }
        if (!self->mDrainPromise.IsEmpty()) {
          self->mDecoderDrained = true;
          self->DrainComplete();
        }
      }));
  if (NS_FAILED(rv)) {
    LOG("Couldn't dispatch decoder EOS");
  }
  Unused << rv;
}

gecko::codec::CodecType GeckoCameraVideoDecoder::CodecTypeFromMime(
    const nsACString& aMimeType) {
  if (MP4Decoder::IsH264(aMimeType)) {
    return gecko::codec::VideoCodecH264;
  } else if (VPXDecoder::IsVP8(aMimeType)) {
    return gecko::codec::VideoCodecVP8;
  } else if (VPXDecoder::IsVP9(aMimeType)) {
    return gecko::codec::VideoCodecVP9;
  }
  return gecko::codec::VideoCodecUnknown;
}

MediaResult GeckoCameraVideoDecoder::CreateDecoder() {
  gecko::codec::VideoDecoderMetadata metadata;
  memset(&metadata, 0, sizeof(metadata));

  metadata.codecType = CodecTypeFromMime(mInfo.mMimeType);
  if (metadata.codecType == gecko::codec::VideoCodecUnknown) {
    return MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                       RESULT_DETAIL("Unsupported codec"));
  }

  metadata.width = mInfo.mImage.width;
  metadata.height = mInfo.mImage.height;
  metadata.framerate = 0;

  if (mIsH264 && mInfo.mExtraData) {
    metadata.codecSpecific = mInfo.mExtraData->Elements();
    metadata.codecSpecificSize = mInfo.mExtraData->Length();
  }

  std::shared_ptr<gecko::codec::VideoDecoder> decoder;
  if (!mCodecManager->createVideoDecoder(metadata.codecType, decoder) ||
      !decoder) {
    LOG("Cannot create decoder");
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Create decoder failed"));
  }

  if (!decoder->init(metadata)) {
    LOG("Cannot initialize decoder");
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Init decoder failed"));
  }

  decoder->setListener(this);
  {
    MutexAutoLock lock(mMutex);
    mDecoder = std::move(decoder);
  }
  return NS_OK;
}

void GeckoCameraVideoDecoder::ProcessDecode(
    MediaRawData* aSample,
    const std::shared_ptr<DecodeStateAtomic>& aDecodeState) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  if (mIsShutDown) {
    aDecodeState->exchange(DecodeState::Idle);
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    return;
  }

  if (mError) {
    aDecodeState->exchange(DecodeState::Idle);
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }

  std::shared_ptr<gecko::codec::VideoDecoder> decoder;
  {
    MutexAutoLock lock(mMutex);
    decoder = mDecoder;
  }
  if (!decoder) {
    aDecodeState->exchange(DecodeState::Idle);
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    return;
  }

  if (mDecoderDrained) {
    LOG("Recreating decoder after drain");
    mIgnoreCallbacks = true;
    {
      MutexAutoLock lock(mMutex);
      ++mDecoderGeneration;
      decoder = mDecoder;
      mDecoder.reset();
      mInputFrames.clear();
      mReorderQueue.Clear();
    }

    if (decoder) {
      MutexAutoLock lock(mCodecControlMutex);
      decoder->stop();
      decoder->setListener(nullptr);
    }
    mDecoderDrained = false;

    MediaResult rv = CreateDecoder();
    mIgnoreCallbacks = false;
    if (NS_FAILED(rv)) {
      mError = true;
      aDecodeState->exchange(DecodeState::Idle);
      mDecodePromise.Reject(rv, __func__);
      return;
    }

    {
      MutexAutoLock lock(mMutex);
      decoder = mDecoder;
    }
  }

  const uint64_t timestamp =
      static_cast<uint64_t>(aSample->mTime.ToMicroseconds());
  {
    MutexAutoLock lock(mMutex);
    mInputFrames.emplace(timestamp, aSample);
  }

  // If decode blocks for more than a second, drain it from the command queue
  // to release the full input queue.
  const TimeDuration decodeTimeout = TimeDuration::FromMilliseconds(1000);
  mDecodeTimer->WaitFor(decodeTimeout, __func__)
      ->Then(
          mCommandTaskQueue, __func__,
          [self = RefPtr<GeckoCameraVideoDecoder>(this),
           decodeState = aDecodeState]() {
            MutexAutoLock controlLock(self->mCodecControlMutex);
            if (!decodeState->compareExchange(DecodeState::Active,
                                              DecodeState::TimedOut)) {
              return;
            }
            LOGEX(self, "Decode is blocked for too long");
            self->mError = true;
            std::shared_ptr<gecko::codec::VideoDecoder> decoder;
            {
              MutexAutoLock lock(self->mMutex);
              decoder = self->mDecoder;
            }
            if (!self->mIsShutDown && decoder) {
              decoder->drain();
            }
          },
          [] {});

  mDecoderDrained = false;
  auto* inputHolder = new RefPtr<MediaRawData>(aSample);
  // Will block here if decode queue is full.
  bool ok = decoder->decode(
      aSample->Data(), aSample->Size(), timestamp,
      aSample->mKeyframe ? gecko::codec::KeyFrame
                         : gecko::codec::DeltaFrame,
      &GeckoCameraVideoDecoder::ReleaseInput, inputHolder);
  if (!ok) {
    delete inputHolder;
  }
  DecodeState decodeState = aDecodeState->exchange(DecodeState::Idle);
  if (decodeState == DecodeState::TimedOut) {
    // The timeout callback owns the codec-control mutex until it has marked
    // the error and completed the unblock drain.
    MutexAutoLock controlBarrier(mCodecControlMutex);
  }

  auto removeInputFrame = [this, timestamp, aSample]() {
    MutexAutoLock lock(mMutex);
    auto [begin, end] = mInputFrames.equal_range(timestamp);
    for (auto iter = begin; iter != end; ++iter) {
      if (iter->second == aSample) {
        mInputFrames.erase(iter);
        break;
      }
    }
  };

  if (!ok) {
    LOG("Couldn't pass frame to decoder");
    NS_WARNING("Couldn't pass frame to decoder");
    removeInputFrame();
    mDecodePromise.Reject(
        decodeState == DecodeState::TimedOut || mError
            ? NS_ERROR_DOM_MEDIA_FATAL_ERR
            : NS_ERROR_DOM_MEDIA_DECODE_ERR,
        __func__);
    return;
  }
  if (decodeState == DecodeState::TimedOut || mError) {
    removeInputFrame();
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }
  LOG("The frame %llu sent to the decoder",
      static_cast<unsigned long long>(timestamp));

  MutexAutoLock lock(mMutex);
  LOG("%llu decoded frames queued",
      static_cast<unsigned long long>(mReorderQueue.Length()));
  DecodedData results;
  while (mReorderQueue.Length() > mMaxRefFrames) {
    results.AppendElement(mReorderQueue.Pop());
  }
  mDecodePromise.Resolve(std::move(results), __func__);
}

/* static */
void GeckoCameraVideoDecoder::ReleaseInput(void* aData) {
  delete static_cast<RefPtr<MediaRawData>*>(aData);
}

void GeckoCameraVideoDecoder::DrainComplete() {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  if (mDrainPromise.IsEmpty()) {
    return;
  }

  DecodedData samples;
  {
    MutexAutoLock lock(mMutex);
    mInputFrames.clear();
    while (!mReorderQueue.IsEmpty()) {
      samples.AppendElement(mReorderQueue.Pop());
    }
  }
  mDrainPromise.Resolve(std::move(samples), __func__);
}

}  // namespace mozilla

#undef LOG
#undef LOGEX
