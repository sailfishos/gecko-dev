/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindowParent.h"

#include <math.h>
#include <map>
#include <utility>

#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedInputData.h"
#include "EmbedLiteWindow.h"
#include "EmbedLog.h"
#include "base/message_loop.h"

#include "InputData.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPoint.h"
#include "mozilla/DataMutex.h"
#include "mozilla/layers/APZThreadUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace embedlite {

namespace {

struct WindowRegistry
{
  std::map<uint32_t, RefPtr<EmbedLiteWindowParent>> windows;
  uint32_t currentWindowId = 0;
};

MOZ_RUNINIT static StaticDataMutex<WindowRegistry> sWindowRegistry(
  "EmbedLiteWindowParent::sWindowRegistry");

} // namespace

EmbedLiteWindowParent::EmbedLiteWindowParent(
    const uint16_t &width, const uint16_t &height, const uint32_t &id,
    EmbedLiteWindowListener *aListener, bool aChromeHosted)
  : mId(id)
  , mListener(aListener)
  , mWindow(nullptr)
  , mChromeSessionListener(nullptr)
  , mChromeHosted(aChromeHosted)
  , mInitialized(false)
  , mDestroying(false)
  , mHasLocation(false)
  , mHasLoadStarted(false)
  , mHasLoadProgress(false)
  , mHasTitle(false)
  , mCanGoBack(false)
  , mCanGoForward(false)
  , mLoadProgress(0)
  , mLoadCurrent(0)
  , mLoadTotal(0)
  , mPlatformFrameListener(nullptr)
  , mCompositor(nullptr)
  , mSize(width, height)
  , mRotation(mozilla::ROTATION_0)
{
  MOZ_ASSERT(mListener);

  if (mChromeHosted) {
    // Keep APZ input handling on the embedder thread, matching the legacy
    // EmbedLiteView path. The Gecko main thread receives the transformed
    // event after APZ has selected its target.
    layers::APZThreadUtils::SetControllerThread(
      MessageLoop::current()->SerialEventTarget());
  }

  MOZ_COUNT_CTOR(EmbedLiteWindowParent);
}

EmbedLiteWindowParent::~EmbedLiteWindowParent()
{
  MOZ_ASSERT(mObservers.IsEmpty());

  MOZ_COUNT_DTOR(EmbedLiteWindowParent);
}

void EmbedLiteWindowParent::Register(EmbedLiteWindowParent* aParent)
{
  MOZ_RELEASE_ASSERT(aParent);
  RefPtr<EmbedLiteWindowParent> registered = aParent;
  {
    auto registry = sWindowRegistry.Lock();
    const auto result = registry->windows.emplace(
      aParent->mId, std::move(registered));
    MOZ_RELEASE_ASSERT(result.second);
    registry->currentWindowId = aParent->mId;
  }
}

void EmbedLiteWindowParent::Unregister(EmbedLiteWindowParent* aParent)
{
  MOZ_RELEASE_ASSERT(aParent);
  RefPtr<EmbedLiteWindowParent> registered;
  {
    auto registry = sWindowRegistry.Lock();
    const auto it = registry->windows.find(aParent->mId);
    MOZ_RELEASE_ASSERT(it != registry->windows.end());
    MOZ_RELEASE_ASSERT(it->second == aParent);
    registered = std::move(it->second);
    registry->windows.erase(it);
    if (registry->currentWindowId == aParent->mId) {
      registry->currentWindowId = 0;
    }
  }
  MOZ_RELEASE_ASSERT(registered == aParent);
}

RefPtr<EmbedLiteWindowParent> EmbedLiteWindowParent::From(const uint32_t id)
{
  RefPtr<EmbedLiteWindowParent> parent;
  {
    auto registry = sWindowRegistry.Lock();
    const auto it = registry->windows.find(id);
    if (it != registry->windows.end()) {
      parent = it->second;
    }
  }
  return parent;
}

RefPtr<EmbedLiteWindowParent> EmbedLiteWindowParent::Current()
{
  auto registry = sWindowRegistry.Lock();
  const auto it = registry->windows.find(registry->currentWindowId);
  if (it != registry->windows.end()) {
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

bool EmbedLiteWindowParent::CanSendChromeSessionCommand() const
{
  return mChromeHosted && mInitialized && !mDestroying;
}

void EmbedLiteWindowParent::SetListener(
    EmbedLiteChromeSessionListener* aListener)
{
  mChromeSessionListener = aListener;
  if (mChromeSessionListener && CanSendChromeSessionCommand()) {
    ReplayChromeSessionState();
  }
}

bool EmbedLiteWindowParent::LoadURL(const char* aURL, bool aFromExternal)
{
  return aURL && CanSendChromeSessionCommand() &&
    SendLoadURL(nsDependentCString(aURL), aFromExternal);
}

bool EmbedLiteWindowParent::GoBack(bool aRequireUserInteraction,
                                   bool aUserActivation)
{
  return CanSendChromeSessionCommand() &&
    SendGoBack(aRequireUserInteraction, aUserActivation);
}

bool EmbedLiteWindowParent::GoForward(bool aRequireUserInteraction,
                                      bool aUserActivation)
{
  return CanSendChromeSessionCommand() &&
    SendGoForward(aRequireUserInteraction, aUserActivation);
}

bool EmbedLiteWindowParent::StopLoad()
{
  return CanSendChromeSessionCommand() && SendStopLoad();
}

bool EmbedLiteWindowParent::Reload(bool aHardReload)
{
  return CanSendChromeSessionCommand() && SendReload(aHardReload);
}

bool EmbedLiteWindowParent::SetActive(bool aActive)
{
  return CanSendChromeSessionCommand() && SendSetActive(aActive);
}

bool EmbedLiteWindowParent::SetFocused(bool aFocused)
{
  return CanSendChromeSessionCommand() && SendSetFocused(aFocused);
}

bool EmbedLiteWindowParent::ReceiveInputEvent(const EmbedTouchInput& aEvent)
{
  if (!CanSendChromeSessionCommand() ||
      aEvent.type < EmbedTouchInput::MULTITOUCH_START ||
      aEvent.type >= EmbedTouchInput::MULTITOUCH_SENTINEL) {
    return false;
  }

  MultiTouchInput input(
    static_cast<MultiTouchInput::MultiTouchType>(aEvent.type),
    aEvent.timeStamp, TimeStamp::Now(), 0);
  for (const TouchData& touchData : aEvent.touches) {
    nsIntPoint point(int32_t(floorf(touchData.touchPoint.x)),
                     int32_t(floorf(touchData.touchPoint.y)));
    input.mTouches.AppendElement(SingleTouchData(
      touchData.identifier, ScreenIntPoint::FromUnknownPoint(point),
      ScreenSize(1, 1), 180.0f, touchData.pressure));
  }

  return SendReceiveInputEvent(input);
}

void EmbedLiteWindowParent::ReplayChromeSessionState()
{
  MOZ_ASSERT(mChromeSessionListener);

  if (mHasLocation) {
    mChromeSessionListener->OnLocationChanged(
      mLocation.get(), mCanGoBack, mCanGoForward);
  }
  if (mHasLoadStarted) {
    mChromeSessionListener->OnLoadStarted(mLoadStartedLocation.get());
  }
  if (mHasLoadProgress) {
    mChromeSessionListener->OnLoadProgress(mLoadProgress, mLoadCurrent,
                                           mLoadTotal);
  }
  if (mHasTitle) {
    mChromeSessionListener->OnTitleChanged(mTitle.get());
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

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvInitialized(const bool &success)
{
  MOZ_ASSERT(mWindow);
  if (success) {
    mInitialized = true;
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
  if (mDestroying) {
    return IPC_OK();
  }

  mDestroying = true;
  if (mChromeSessionListener) {
    mChromeSessionListener->ChromeSessionDestroyed();
    mChromeSessionListener = nullptr;
  }
  mWindow->Destroyed();
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnLocationChanged(const nsCString& aLocation,
                                             const bool& aCanGoBack,
                                             const bool& aCanGoForward)
{
  if (!CanSendChromeSessionCommand() ||
      (mHasLocation && mLocation == aLocation &&
       mCanGoBack == aCanGoBack && mCanGoForward == aCanGoForward)) {
    return IPC_OK();
  }

  mHasLocation = true;
  mLocation = aLocation;
  mCanGoBack = aCanGoBack;
  mCanGoForward = aCanGoForward;
  if (mChromeSessionListener) {
    mChromeSessionListener->OnLocationChanged(
      mLocation.get(), mCanGoBack, mCanGoForward);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnLoadStarted(const nsCString& aLocation)
{
  if (!CanSendChromeSessionCommand() ||
      (mHasLoadStarted && mLoadStartedLocation == aLocation)) {
    return IPC_OK();
  }

  mHasLoadStarted = true;
  mLoadStartedLocation = aLocation;
  mHasLoadProgress = false;
  if (mChromeSessionListener) {
    mChromeSessionListener->OnLoadStarted(mLoadStartedLocation.get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowParent::RecvOnLoadFinished()
{
  if (!CanSendChromeSessionCommand() || !mHasLoadStarted) {
    return IPC_OK();
  }

  mHasLoadStarted = false;
  if (mChromeSessionListener) {
    mChromeSessionListener->OnLoadFinished();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnLoadProgress(const int32_t& aProgress,
                                          const int64_t& aCurrent,
                                          const int64_t& aTotal)
{
  if (!CanSendChromeSessionCommand() ||
      (mHasLoadProgress && mLoadProgress == aProgress &&
       mLoadCurrent == aCurrent && mLoadTotal == aTotal)) {
    return IPC_OK();
  }

  mHasLoadProgress = true;
  mLoadProgress = aProgress;
  mLoadCurrent = aCurrent;
  mLoadTotal = aTotal;
  if (mChromeSessionListener) {
    mChromeSessionListener->OnLoadProgress(mLoadProgress, mLoadCurrent,
                                           mLoadTotal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnTitleChanged(const nsString& aTitle)
{
  if (!CanSendChromeSessionCommand() || (mHasTitle && mTitle == aTitle)) {
    return IPC_OK();
  }

  mHasTitle = true;
  mTitle = aTitle;
  if (mChromeSessionListener) {
    mChromeSessionListener->OnTitleChanged(mTitle.get());
  }
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
