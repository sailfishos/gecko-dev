/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindowParent.h"

#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteWindow.h"
#include "EmbedLog.h"

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPoint.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace embedlite {

namespace {

static std::map<uint32_t, EmbedLiteWindowParent*> sWindowMap;
static uint32_t sCurrentWindowId;

} // namespace

EmbedLiteWindowParent::EmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, EmbedLiteWindowListener *aListener)
  : mId(id)
  , mListener(aListener)
  , mWindow(nullptr)
  , mPlatformFrameListener(nullptr)
  , mCompositor(nullptr)
  , mSize(width, height)
  , mRotation(mozilla::ROTATION_0)
{
  MOZ_ASSERT(sWindowMap.find(id) == sWindowMap.end());
  MOZ_ASSERT(mListener);
  sWindowMap[id] = this;
  sCurrentWindowId = id;

  MOZ_COUNT_CTOR(EmbedLiteWindowParent);
}

EmbedLiteWindowParent::~EmbedLiteWindowParent()
{
  MOZ_ASSERT(sWindowMap.find(mId) != sWindowMap.end());
  sWindowMap.erase(sWindowMap.find(mId));
  if (mId == sCurrentWindowId) {
    sCurrentWindowId = 0;
  }

  MOZ_ASSERT(mObservers.IsEmpty());

  MOZ_COUNT_DTOR(EmbedLiteWindowParent);
}

EmbedLiteWindowParent* EmbedLiteWindowParent::From(const uint32_t id)
{
  std::map<uint32_t, EmbedLiteWindowParent*>::const_iterator it = sWindowMap.find(id);
  if (it != sWindowMap.end()) {
    return it->second;
  }
  return nullptr;
}

uint32_t EmbedLiteWindowParent::Current()
{
  return sCurrentWindowId;
}

void EmbedLiteWindowParent::AddObserver(EmbedLiteWindowParentObserver* obs)
{
  mObservers.AppendElement(obs);
}

void EmbedLiteWindowParent::RemoveObserver(EmbedLiteWindowParentObserver* obs)
{
  mObservers.RemoveElement(obs);
}

bool EmbedLiteWindowParent::ScheduleUpdate()
{
  if (mCompositor) {
    LOGT("EmbedLiteWindowParent::ScheduleUpdate");
    mCompositor->ScheduleForcedRenderOnCompositorThread(wr::RenderReasons::WIDGET);
    return true;
  }
  return false;
}

void EmbedLiteWindowParent::SuspendRendering()
{
  if (mCompositor) {
    mCompositor->SuspendRendering();
  }
}

void EmbedLiteWindowParent::ResumeRendering()
{
  if (mCompositor) {
    mCompositor->ResumeRendering();
  }
}

bool EmbedLiteWindowParent::WithPlatformImage(const PlatformImageCallback& callback)
{
  if (mCompositor) {
    return mCompositor->WithPlatformImage(callback);
  }
  return false;
}

void EmbedLiteWindowParent::ClearPlatformImage()
{
  if (mCompositor) {
    mCompositor->ClearPlatformImage();
  }
}

bool EmbedLiteWindowParent::AcquirePlatformFrame(
    const PlatformFrameToken& token,
    const PlatformFrameCallback& callback)
{
  return mCompositor && mCompositor->AcquirePlatformFrame(token, callback);
}

bool EmbedLiteWindowParent::ReleasePlatformFrame(
    const PlatformFrameRelease& release)
{
  return mCompositor && mCompositor->ReleasePlatformFrame(release);
}

bool EmbedLiteWindowParent::SetPlatformFrameDeliveryEnabled(bool enabled)
{
  return mCompositor &&
    mCompositor->SetPlatformFrameDeliveryEnabled(enabled);
}

bool EmbedLiteWindowParent::SetPlatformFrameListener(
    EmbedLitePlatformFrameListener* listener)
{
  if (mCompositor && !mCompositor->SetPlatformFrameListener(listener)) {
    return false;
  }
  mPlatformFrameListener = listener;
  return true;
}

void EmbedLiteWindowParent::SetEmbedAPIWindow(EmbedLiteWindow* window)
{
  mWindow = window;
}

void EmbedLiteWindowParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvInitialized(const bool &success)
{
  MOZ_ASSERT(mWindow);
  if (success) {
    mListener->WindowInitialized();
  } else if (EmbedLiteChromeWindowListener* chromeListener =
               dynamic_cast<EmbedLiteChromeWindowListener*>(mListener)) {
    chromeListener->ChromeWindowInitializationFailed();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowParent::RecvDestroyed()
{
  MOZ_ASSERT(mWindow);
  mWindow->Destroyed();
  return IPC_OK();
}

void EmbedLiteWindowParent::SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor)
{
  LOGT("compositor:%p, observers:%d", aCompositor, static_cast<int>(mObservers.Length()));
  MOZ_ASSERT(!mCompositor);

  mCompositor = aCompositor;

  if (mPlatformFrameListener) {
    MOZ_ALWAYS_TRUE(
      mCompositor->SetPlatformFrameListener(mPlatformFrameListener));
  }

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->CompositorCreated();
  }
}

} // namespace embedlite
} // namespace mozilla
