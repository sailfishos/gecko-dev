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

} // namespace

EmbedLiteWindowParent::EmbedLiteWindowParent(const uint16_t& width, const uint16_t& height, const uint32_t& id)
  : mId(id)
  , mWindow(nullptr)
  , mCompositor(nullptr)
  , mSize(width, height)
  , mRotation(mozilla::ROTATION_0)
{
  MOZ_ASSERT(sWindowMap.find(id) == sWindowMap.end());
  sWindowMap[id] = this;

  MOZ_COUNT_CTOR(EmbedLiteWindowParent);
}

EmbedLiteWindowParent::~EmbedLiteWindowParent()
{
  MOZ_ASSERT(sWindowMap.find(mId) != sWindowMap.end());
  sWindowMap.erase(sWindowMap.find(mId));

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
    mCompositor->ScheduleRenderOnCompositorThread();
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

void* EmbedLiteWindowParent::GetPlatformImage(int* width, int* height)
{
  if (mCompositor) {
    return mCompositor->GetPlatformImage(width, height);
  }
  return nullptr;
}

void EmbedLiteWindowParent::GetPlatformImage(const std::function<void(void *image, int width, int height)> &callback)
{
    if (mCompositor) {
        mCompositor->GetPlatformImage(callback);
    }
}

void EmbedLiteWindowParent::SetEmbedAPIWindow(EmbedLiteWindow* window)
{
  mWindow = window;
}

void EmbedLiteWindowParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

mozilla::ipc::IPCResult EmbedLiteWindowParent::RecvInitialized()
{
  MOZ_ASSERT(mWindow);
  mWindow->GetListener()->WindowInitialized();
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
  LOGT("compositor:%p, observers:%d", aCompositor, mObservers.Length());
  MOZ_ASSERT(!mCompositor);

  mCompositor = aCompositor;

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->CompositorCreated();
  }
}

} // namespace embedlite
} // namespace mozilla
