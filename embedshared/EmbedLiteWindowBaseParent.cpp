/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindowBaseParent.h"

#include "EmbedLiteCompositorParent.h"
#include "EmbedLiteWindow.h"
#include "EmbedLog.h"

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPoint.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace embedlite {

namespace {

static std::map<uint32_t, EmbedLiteWindowBaseParent*> sWindowMap;

} // namespace

EmbedLiteWindowBaseParent::EmbedLiteWindowBaseParent(const uint16_t& width, const uint16_t& height, const uint32_t& id)
  : mId(id)
  , mWindow(nullptr)
  , mCompositor(nullptr)
  , mSize(width, height)
  , mRotation(mozilla::ROTATION_0)
{
  MOZ_ASSERT(sWindowMap.find(id) == sWindowMap.end());
  sWindowMap[id] = this;

  MOZ_COUNT_CTOR(EmbedLiteWindowBaseParent);
}

EmbedLiteWindowBaseParent::~EmbedLiteWindowBaseParent()
{
  MOZ_ASSERT(sWindowMap.find(mId) != sWindowMap.end());
  sWindowMap.erase(sWindowMap.find(mId));

  MOZ_ASSERT(mObservers.IsEmpty());

  MOZ_COUNT_DTOR(EmbedLiteWindowBaseParent);
}

EmbedLiteWindowBaseParent* EmbedLiteWindowBaseParent::From(const uint32_t id)
{
  std::map<uint32_t, EmbedLiteWindowBaseParent*>::const_iterator it = sWindowMap.find(id);
  if (it != sWindowMap.end()) {
    return it->second;
  }
  return nullptr;
}

void EmbedLiteWindowBaseParent::AddObserver(EmbedLiteWindowParentObserver* obs)
{
  mObservers.AppendElement(obs);
}

void EmbedLiteWindowBaseParent::RemoveObserver(EmbedLiteWindowParentObserver* obs)
{
  mObservers.RemoveElement(obs);
}

bool EmbedLiteWindowBaseParent::ScheduleUpdate()
{
  if (mCompositor) {
    mCompositor->ScheduleRenderOnCompositorThread();
    return true;
  }
  return false;
}

void EmbedLiteWindowBaseParent::SuspendRendering()
{
  if (mCompositor) {
    mCompositor->SuspendRendering();
  }
}

void EmbedLiteWindowBaseParent::ResumeRendering()
{
  if (mCompositor) {
    mCompositor->ResumeRendering();
  }
}

void* EmbedLiteWindowBaseParent::GetPlatformImage(int* width, int* height)
{
  if (mCompositor) {
    return mCompositor->GetPlatformImage(width, height);
  }
  return nullptr;
}

void EmbedLiteWindowBaseParent::SetEmbedAPIWindow(EmbedLiteWindow* window)
{
  mWindow = window;
}

void EmbedLiteWindowBaseParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

bool EmbedLiteWindowBaseParent::RecvInitialized()
{
  MOZ_ASSERT(mWindow);
  mWindow->GetListener()->WindowInitialized();
  return true;
}

bool EmbedLiteWindowBaseParent::RecvDestroyed()
{
  MOZ_ASSERT(mWindow);
  mWindow->Destroyed();
  return true;
}

void EmbedLiteWindowBaseParent::SetCompositor(EmbedLiteCompositorParent* aCompositor)
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
