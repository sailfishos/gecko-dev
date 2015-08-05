/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindow.h"

#include "mozilla/embedlite/PEmbedLiteWindowParent.h"
#include "EmbedLiteWindowBaseParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace embedlite {

namespace {

class FakeWindowListener : public EmbedLiteWindowListener {};
static FakeWindowListener sFakeWindowListener;

} // namespace

EmbedLiteWindow::EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent* parent, uint32_t id)
  : mApp(app)
  , mListener(nullptr)
  , mWindowParent(static_cast<EmbedLiteWindowBaseParent*>(parent))
  , mUniqueID(id)
{
  MOZ_COUNT_CTOR(EmbedLiteWindow);
  mWindowParent->SetEmbedAPIWindow(this);
}

EmbedLiteWindow::~EmbedLiteWindow()
{
  MOZ_COUNT_DTOR(EmbedLiteWindow);
  unused << mWindowParent->SendDestroy();
  mWindowParent->SetEmbedAPIWindow(nullptr);
  if (mListener) {
    mListener->WindowDestroyed();
  }
}


void EmbedLiteWindow::SetListener(EmbedLiteWindowListener* aListener)
{
  mListener = aListener;
}

EmbedLiteWindowListener* const EmbedLiteWindow::GetListener() const
{
  return mListener ? mListener : &sFakeWindowListener;
}

void EmbedLiteWindow::SetSize(int width, int height)
{
  unused << mWindowParent->SendSetSize(gfxSize(width, height));
}

uint32_t EmbedLiteWindow::GetUniqueID() const
{
  return mUniqueID;
}

void EmbedLiteWindow::SetContentOrientation(mozilla::ScreenRotation rotation)
{
  unused << mWindowParent->SendSetContentOrientation(rotation);
}

void EmbedLiteWindow::ScheduleUpdate()
{
  mWindowParent->ScheduleUpdate();
}

void EmbedLiteWindow::SuspendRendering()
{
  mWindowParent->SuspendRendering();
}

void EmbedLiteWindow::ResumeRendering()
{
  mWindowParent->ResumeRendering();
}

void* EmbedLiteWindow::GetPlatformImage(int* width, int* height)
{
  return mWindowParent->GetPlatformImage(width, height);
}

char* EmbedLiteWindow::GetImageAsURL(int aWidth, int aHeight)
{
  // copy from gfxASurface::WriteAsPNG_internal
  NS_ENSURE_TRUE(mWindowParent, nullptr);
  nsRefPtr<gfxImageSurface> img =
    new gfxImageSurface(gfxIntSize(aWidth, aHeight), gfxImageFormat::RGB24);
  mWindowParent->RenderToImage(img->Data(), img->Width(), img->Height(), img->Stride(), 24);
  nsCOMPtr<imgIEncoder> encoder =
    do_CreateInstance("@mozilla.org/image/encoder;2?type=image/png");
  NS_ENSURE_TRUE(encoder, nullptr);
  gfxIntSize size = img->GetSize();
  nsresult rv = encoder->InitFromData(img->Data(),
                                      size.width * size.height * 4,
                                      size.width,
                                      size.height,
                                      img->Stride(),
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      NS_LITERAL_STRING(""));
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  nsCOMPtr<nsIInputStream> imgStream;
  CallQueryInterface(encoder.get(), getter_AddRefs(imgStream));

  if (!imgStream) {
    return nullptr;
  }

  uint64_t bufSize64;
  rv = imgStream->Available(&bufSize64);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  if (bufSize64 > UINT32_MAX - 16) {
    return nullptr;
  }

  uint32_t bufSize = (uint32_t)bufSize64;

  // ...leave a little extra room so we can call read again and make sure we
  // got everything. 16 bytes for better padding (maybe)
  bufSize += 16;
  uint32_t imgSize = 0;
  char* imgData = (char*)moz_malloc(bufSize);
  if (!imgData) {
    return nullptr;
  }
  uint32_t numReadThisTime = 0;
  while ((rv = imgStream->Read(&imgData[imgSize],
                               bufSize - imgSize,
                               &numReadThisTime)) == NS_OK && numReadThisTime > 0) {
    imgSize += numReadThisTime;
    if (imgSize == bufSize) {
      // need a bigger buffer, just double
      bufSize *= 2;
      char* newImgData = (char*)moz_realloc(imgData, bufSize);
      if (!newImgData) {
        moz_free(imgData);
        return nullptr;
      }
      imgData = newImgData;
    }
  }

  // base 64, result will be NULL terminated
  nsCString encodedImg;
  rv = Base64Encode(Substring(imgData, imgSize), encodedImg);
  moz_free(imgData);
  if (NS_FAILED(rv)) { // not sure why this would fail
    return nullptr;
  }

  nsCString string("data:image/png;base64,");
  string.Append(encodedImg);

  return ToNewCString(string);
}

bool EmbedLiteWindow::RenderToImage(unsigned char* aData, int aWidth, int aHeight, int aStride, int aDepth)
{
  LOGF("data:%p, sz[%i,%i], stride:%i, depth:%i", aData, aWidth, aHeight, aStride, aDepth);
  return mWindowParent->RenderToImage(aData, aWidth, aHeight, aStride, aDepth);
}

} // nemsapace embedlite
} // namespace mozilla

