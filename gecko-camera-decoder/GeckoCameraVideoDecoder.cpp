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

#define LOG(...) DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)
#define LOGEX(_this, ...) \
  DDMOZ_LOGEX(_this, sPDMLog, mozilla::LogLevel::Debug, __VA_ARGS__)

namespace mozilla {

GeckoCameraVideoDecoder::GeckoCameraVideoDecoder(
    gecko::codec::CodecManager* manager,
    const CreateDecoderParams& aParams)
    : mCodecManager(manager),
      mParams(aParams),
      mInfo(aParams.VideoConfig()),
      mImageContainer(aParams.mImageContainer),
      mImageAllocator(aParams.mKnowsCompositor),
      mMutex("GeckoCameraVideoDecoder::mMutex"),
      mTaskQueue(TaskQueue::Create(
          GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER), "GeckoCameraVideoDecoder")),
      mIsH264(MP4Decoder::IsH264(aParams.mConfig.mMimeType)),
      mMaxRefFrames(mIsH264 ? H264::HasSPS(aParams.VideoConfig().mExtraData)
                                  ? H264::ComputeMaxRefFrames(
                                        aParams.VideoConfig().mExtraData)
                                  : 16
                            : 0),
      mIsShutDown(false),
      mError(false),
      mDecodeTimer(new MediaTimer()),
      mCommandTaskQueue(CreateMediaDecodeTaskQueue("GeckoCameraVideoDecoder")) {
  MOZ_COUNT_CTOR(GeckoCameraVideoDecoder);
  LOG("GeckoCameraVideoDecoder - mMaxRefFrames=%d", mMaxRefFrames);
}

RefPtr<MediaDataDecoder::InitPromise> GeckoCameraVideoDecoder::Init() {
  MediaResult rv = CreateDecoder();
  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(mParams.mConfig.GetType(), __func__);
  }
  return InitPromise::CreateAndReject(rv, __func__);
}

RefPtr<ShutdownPromise> GeckoCameraVideoDecoder::Shutdown() {
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
      LOG("Shutdown");
      mIsShutDown = true;
      mDecodeTimer->Cancel();
      mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
      mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
      mDecoderDrained = false;

      std::shared_ptr<gecko::codec::VideoDecoder> decoder;
      {
        MutexAutoLock lock(mMutex);
        decoder = mDecoder;
        mDecoder.reset();
        mInputFrames.clear();
        mReorderQueue.Clear();
      }

      if (decoder) {
        decoder->setListener(nullptr);
        decoder->stop();
      }
      return mTaskQueue->BeginShutdown();
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

    // Throw an error if the decoder is blocked for more than a second.
    const TimeDuration decodeTimeout = TimeDuration::FromMilliseconds(1000);
    mDecodeTimer->WaitFor(decodeTimeout, __func__)
        ->Then(
            // To ublock decode(), drain the decoder on a separate thread from
            // the decoder pool. gecko-camera must handle this without issue.
            mCommandTaskQueue, __func__,
            [self = RefPtr<GeckoCameraVideoDecoder>(this), this]() {
              LOG("Decode is blocked for too long");
              mError = true;
              std::shared_ptr<gecko::codec::VideoDecoder> decoder;
              {
                MutexAutoLock lock(mMutex);
                decoder = mDecoder;
              }
              if (!mIsShutDown && decoder) {
                decoder->drain();
              }
            },
            [] {});
    ProcessDecode(sample);
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

    decoder->drain();
    return p;
  });
}

RefPtr<MediaDataDecoder::FlushPromise> GeckoCameraVideoDecoder::Flush() {
  RefPtr<GeckoCameraVideoDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this] {
    LOG("Flush");
    mDecoderDrained = false;
    mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    if (mDecoder) {
      mDecoder->flush();
    }
    MutexAutoLock lock(mMutex);
    mReorderQueue.Clear();
    mInputFrames.clear();
    // Clear a decoder error that may occur during flushing.
    mError = false;
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

MediaDataDecoder::ConversionRequired GeckoCameraVideoDecoder::NeedsConversion()
    const {
  return mIsH264 ? ConversionRequired::kNeedAnnexB
                 : ConversionRequired::kNeedNone;
}

nsCString GeckoCameraVideoDecoder::GetCodecName() const {
  const nsACString& mimeType = mParams.mConfig.mMimeType;
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

void GeckoCameraVideoDecoder::onDecodedYCbCrFrame(const gecko::camera::YCbCrFrame *frame) {
  MOZ_ASSERT(frame, "YCbCrFrame is null");

  LOG("onDecodedFrame %llu", frame->timestampUs);

  if (mIsShutDown) {
    LOG("Decoder shuts down");
    return;
  }

  RefPtr<MediaRawData> inputFrame;
  {
    MutexAutoLock lock(mMutex);
    auto iter = mInputFrames.find(frame->timestampUs);
    if (iter == mInputFrames.end()) {
      LOG("Couldn't find input frame with timestamp %llu", frame->timestampUs);
      return;
    }
    inputFrame = iter->second;
    mInputFrames.erase(iter);
  }


  VideoData::YCbCrBuffer buffer;
  // Y plane.
  buffer.mPlanes[0].mData = const_cast<uint8_t*>(frame->y);
  buffer.mPlanes[0].mStride = frame->yStride;
  buffer.mPlanes[0].mWidth = frame->width;
  buffer.mPlanes[0].mHeight = frame->height;
  buffer.mPlanes[0].mSkip = 0;
  // Cb plane.
  buffer.mPlanes[1].mData = const_cast<uint8_t*>(frame->cb);
  buffer.mPlanes[1].mStride = frame->cStride;
  buffer.mPlanes[1].mWidth = (frame->width + 1) / 2;
  buffer.mPlanes[1].mHeight = (frame->height + 1) / 2;
  buffer.mPlanes[1].mSkip = frame->chromaStep - 1;
  // Cr plane.
  buffer.mPlanes[2].mData = const_cast<uint8_t*>(frame->cr);
  buffer.mPlanes[2].mStride = frame->cStride;
  buffer.mPlanes[2].mWidth = (frame->width + 1) / 2;
  buffer.mPlanes[2].mHeight = (frame->height + 1) / 2;
  buffer.mPlanes[2].mSkip = frame->chromaStep - 1;

  buffer.mYUVColorSpace = DefaultColorSpace({frame->width, frame->height});
  buffer.mColorDepth = gfx::ColorDepth::COLOR_8;
  buffer.mColorRange = gfx::ColorRange::LIMITED;
  buffer.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

  gfx::IntRect pictureRegion(0, 0, frame->width, frame->height);
  RefPtr<MediaData> data = VideoData::CreateAndCopyData(
      mInfo, mImageContainer, inputFrame->mOffset,
      inputFrame->mTime, inputFrame->mDuration,
      buffer, inputFrame->mKeyframe, inputFrame->mTimecode,
      pictureRegion, mImageAllocator);
  if (!data) {
    NS_ERROR("Couldn't create VideoData for frame");
    return;
  }

  MutexAutoLock lock(mMutex);
  mReorderQueue.Push(std::move(data));
}

void GeckoCameraVideoDecoder::onDecodedGraphicBuffer(std::shared_ptr<gecko::camera::GraphicBuffer> buffer)
{
  std::shared_ptr<const gecko::camera::YCbCrFrame> frame = buffer->mapYCbCr();
  if (frame) {
    onDecodedYCbCrFrame(frame.get());
  } else {
    NS_ERROR("Couldn't map GraphicBuffer");
  }
}

void GeckoCameraVideoDecoder::onDecoderError(std::string errorDescription) {
  LOG("Decoder error %s", errorDescription.c_str());
  mError = true;

  RefPtr<GeckoCameraVideoDecoder> self = this;
  nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "GeckoCameraVideoDecoder::onDecoderError", [self]() {
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
  LOG("Decoder EOS");

  RefPtr<GeckoCameraVideoDecoder> self = this;
  nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "GeckoCameraVideoDecoder::onDecoderEOS",
      [self]() {
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

MediaResult GeckoCameraVideoDecoder::CreateDecoder()
{
  gecko::codec::VideoDecoderMetadata metadata;
  memset(&metadata, 0, sizeof(metadata));

  metadata.codecType = CodecTypeFromMime(mParams.mConfig.mMimeType);

  metadata.width = mInfo.mImage.width;
  metadata.height = mInfo.mImage.height;
  metadata.framerate = 0;

  if (mIsH264) {
    metadata.codecSpecific = mInfo.mExtraData->Elements();
    metadata.codecSpecificSize = mInfo.mExtraData->Length();
  }

  if (!mCodecManager->createVideoDecoder(metadata.codecType, mDecoder)) {
    LOG("Cannot create decoder");
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Create decoder failed"));
  }

  if (!mDecoder->init(metadata)) {
    LOG("Cannot initialize decoder");
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Init decoder failed"));
  }

  mDecoder->setListener(this);
  return NS_OK;
}

void GeckoCameraVideoDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  if (mIsShutDown) {
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    return;
  }

  if (mError) {
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }

  if (!mDecoder) {
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    return;
  }

  {
    MutexAutoLock lock(mMutex);
    mInputFrames[aSample->mTime.ToMicroseconds()] = aSample;
  }

  mDecoderDrained = false;
  // Will block here if decode queue is full.
  bool ok = mDecoder->decode(const_cast<uint8_t*>(aSample->Data()),
      aSample->Size(),
      aSample->mTime.ToMicroseconds(),
      aSample->mKeyframe ? gecko::codec::KeyFrame : gecko::codec::DeltaFrame,
      nullptr, nullptr);
  if (!ok) {
    LOG("Couldn't pass frame to decoder");
    NS_WARNING("Couldn't pass frame to decoder");
    mDecodePromise.Reject(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
    return;
  }
  LOG("The frame %lld sent to the decoder", aSample->mTime.ToMicroseconds());

  MutexAutoLock lock(mMutex);
  LOG("%llu decoded frames queued",
      static_cast<unsigned long long>(mReorderQueue.Length()));
  DecodedData results;
  while (mReorderQueue.Length() > mMaxRefFrames) {
    results.AppendElement(mReorderQueue.Pop());
  }
  mDecodePromise.Resolve(std::move(results), __func__);
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
