/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindowParent.h"

#include <cmath>
#include <cstring>
#include <map>
#include <utility>

#include "EmbedLiteChromeContentEventOrder.h"
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
#include "mozilla/Unused.h"
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
  , mChromeTabSessionListener(nullptr)
  , mChromeContentSessionListener(nullptr)
  , mChromeInputSessionListener(nullptr)
  , mChromeHosted(aChromeHosted)
  , mInitialized(false)
  , mDestroying(false)
  , mHasLocation(false)
  , mHasLoadStarted(false)
  , mHasLoadProgress(false)
  , mHasTitle(false)
  , mHasTabSnapshot(false)
  , mHasInputContext(false)
  , mRestoreTabsSent(false)
  , mProjectedTabId(0)
  , mCanGoBack(false)
  , mCanGoForward(false)
  , mLoadProgress(0)
  , mLoadCurrent(0)
  , mLoadTotal(0)
  , mInputEnabled(0)
  , mInputOpen(0)
  , mInputCause(0)
  , mInputFocusChange(0)
  , mHasContentState(false)
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

bool EmbedLiteWindowParent::CanTargetContentTab(uint64_t aTabId) const
{
  if (!aTabId || !CanSendChromeSessionCommand() || !mHasTabSnapshot) {
    return false;
  }
  for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
    if (tab.id() == aTabId) {
      return !tab.discarded() && !tab.closing();
    }
  }
  return false;
}

void EmbedLiteWindowParent::SetListener(
    EmbedLiteChromeSessionListener* aListener)
{
  mChromeSessionListener = aListener;
  if (mChromeSessionListener && CanSendChromeSessionCommand()) {
    ReplayChromeSessionState();
  }
}

void EmbedLiteWindowParent::SetTabListener(
    EmbedLiteChromeTabSessionListener* aListener)
{
  if (mChromeTabSessionListener != aListener &&
      !mPendingBeforeUnloadPrompts.empty()) {
    if (CanSendChromeSessionCommand()) {
      for (const auto& prompt : mPendingBeforeUnloadPrompts) {
        Unused << SendResolveBeforeUnloadPrompt(
          prompt.first, prompt.second, false);
      }
    }
    mPendingBeforeUnloadPrompts.clear();
  }
  mChromeTabSessionListener = aListener;
  if (mChromeTabSessionListener && CanSendChromeSessionCommand()) {
    ReplayTabSnapshot();
  }
}

void EmbedLiteWindowParent::SetContentListener(
    EmbedLiteChromeContentSessionListener* aListener)
{
  mChromeContentSessionListener = aListener;
  if (aListener && mHasContentState && CanSendChromeSessionCommand()) {
    const auto& state = mContentState;
    aListener->OnContentStateChanged(EmbedLiteChromeContentState{
      state.tabId(), state.persistentId(), state.revision(),
      state.locationRevision(), state.securityStatus().get(),
      state.securityState(), state.fullscreen(),
      state.firstPaint(), state.firstPaintX(), state.firstPaintY(),
      state.scrollWidth(), state.scrollHeight(), state.scrollX(),
      state.scrollY(), state.viewportX(), state.viewportY(),
      state.viewportWidth(), state.viewportHeight()});
  }
}

bool EmbedLiteWindowParent::LoadFrameScript(const char* aURI)
{
  if (!aURI || !*aURI || std::strlen(aURI) > kMaxContentDataLength ||
      !mChromeHosted || mDestroying) {
    return false;
  }
  const nsDependentCString uri(aURI);
  if (mContentRegistrations.HasFrameScript(uri)) {
    return true;
  }
  MOZ_ALWAYS_TRUE(mContentRegistrations.AddFrameScript(uri));
  if (!mInitialized) {
    return true;
  }
  if (SendLoadContentFrameScript(uri)) {
    return true;
  }
  MOZ_ALWAYS_TRUE(mContentRegistrations.RemoveFrameScript(uri));
  return false;
}

bool EmbedLiteWindowParent::AddMessageListener(const char* aName)
{
  if (!aName || !*aName || std::strlen(aName) > kMaxContentNameLength ||
      !mChromeHosted || mDestroying) {
    return false;
  }
  const nsDependentCString name(aName);
  if (mContentRegistrations.HasMessageListener(name)) {
    return true;
  }
  MOZ_ALWAYS_TRUE(mContentRegistrations.AddMessageListener(name));
  if (!mInitialized) {
    return true;
  }
  if (SendAddContentMessageListener(name)) {
    return true;
  }
  MOZ_ALWAYS_TRUE(mContentRegistrations.RemoveMessageListener(name));
  return false;
}

bool EmbedLiteWindowParent::RemoveMessageListener(const char* aName)
{
  if (!aName || !*aName || std::strlen(aName) > kMaxContentNameLength ||
      !mChromeHosted || mDestroying) {
    return false;
  }
  const nsDependentCString name(aName);
  if (!mContentRegistrations.HasMessageListener(name)) {
    return true;
  }
  if (mInitialized && !SendRemoveContentMessageListener(name)) {
    return false;
  }
  MOZ_ALWAYS_TRUE(mContentRegistrations.RemoveMessageListener(name));
  return true;
}

bool EmbedLiteWindowParent::SendAsyncMessage(
    uint64_t aTabId, const char16_t* aName, const char16_t* aJSON)
{
  return aTabId && aName && *aName && aJSON &&
    NS_strlen(aName) <= kMaxContentNameLength &&
    NS_strlen(aJSON) <= kMaxContentDataLength &&
    CanTargetContentTab(aTabId) && SendSendContentAsyncMessage(
      aTabId, nsDependentString(aName), nsDependentString(aJSON));
}

bool EmbedLiteWindowParent::SendMouseEvent(
    uint64_t aTabId, EmbedLiteChromeMouseType aType, int32_t aX, int32_t aY,
    uint64_t aTime, uint32_t aButton, uint32_t aButtons,
    uint32_t aModifiers, uint32_t aClickCount)
{
  return aTabId && aType <= EmbedLiteChromeMouseType::Up &&
    aButton <= 4 && aButtons <= 0x1f && aClickCount <= 3 &&
    mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSendContentMouseEvent(aTabId, static_cast<uint8_t>(aType), aX, aY,
                              aTime, aButton, aButtons, aModifiers,
                              aClickCount);
}

bool EmbedLiteWindowParent::SendWheelEvent(
    uint64_t aTabId, int32_t aX, int32_t aY, uint64_t aTime,
    double aDeltaX, double aDeltaY, uint32_t aDeltaMode,
    uint32_t aModifiers)
{
  return aTabId && std::isfinite(aDeltaX) && std::isfinite(aDeltaY) &&
    aDeltaMode <= 2 && mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSendContentWheelEvent(aTabId, aX, aY, aTime, aDeltaX, aDeltaY,
                              aDeltaMode, aModifiers);
}

bool EmbedLiteWindowParent::ScrollTo(uint64_t aTabId, int32_t aX, int32_t aY)
{ return CanTargetContentTab(aTabId) &&
    SendContentScrollTo(aTabId, aX, aY); }
bool EmbedLiteWindowParent::ScrollBy(uint64_t aTabId, int32_t aX, int32_t aY)
{ return CanTargetContentTab(aTabId) &&
    SendContentScrollBy(aTabId, aX, aY); }
bool EmbedLiteWindowParent::ZoomToRect(uint64_t aTabId, float aX, float aY,
                                      float aWidth, float aHeight)
{ return aTabId && std::isfinite(aX) && std::isfinite(aY) &&
    std::isfinite(aWidth) && aWidth >= 0 && std::isfinite(aHeight) &&
    aHeight >= 0 && mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendContentZoomToRect(aTabId, aX, aY, aWidth, aHeight); }
bool EmbedLiteWindowParent::SetDesktopMode(uint64_t aTabId, bool aValue)
{ return mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSetContentDesktopMode(aTabId, aValue); }
bool EmbedLiteWindowParent::SetThrottlePainting(uint64_t aTabId, bool aValue)
{
  return CanTargetContentTab(aTabId) &&
    SendSetContentThrottlePainting(aTabId, aValue);
}
bool EmbedLiteWindowParent::SuspendTimeouts(uint64_t aTabId)
{ return CanTargetContentTab(aTabId) &&
    SendSuspendContentTimeouts(aTabId); }
bool EmbedLiteWindowParent::ResumeTimeouts(uint64_t aTabId)
{ return CanTargetContentTab(aTabId) &&
    SendResumeContentTimeouts(aTabId); }
bool EmbedLiteWindowParent::SetHttpUserAgent(uint64_t aTabId,
                                             const char16_t* aUserAgent)
{
  return aTabId && aUserAgent &&
    NS_strlen(aUserAgent) <= kMaxContentNameLength &&
    CanTargetContentTab(aTabId) && SendSetContentHttpUserAgent(
      aTabId, nsDependentString(aUserAgent));
}
bool EmbedLiteWindowParent::SetMargins(uint64_t aTabId, int32_t aTop,
    int32_t aRight, int32_t aBottom, int32_t aLeft)
{ return mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSetContentMargins(aTabId, aTop, aRight, aBottom, aLeft); }
bool EmbedLiteWindowParent::SetSafeAreaInsets(uint64_t aTabId, int32_t aTop,
    int32_t aRight, int32_t aBottom, int32_t aLeft)
{ return mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSetContentSafeAreaInsets(aTabId, aTop, aRight, aBottom, aLeft); }
bool EmbedLiteWindowParent::SetDynamicToolbarHeight(uint64_t aTabId,
                                                     int32_t aHeight)
{
  return aHeight >= 0 && mTabSnapshot.selectedTabId() == aTabId &&
    CanTargetContentTab(aTabId) &&
    SendSetContentDynamicToolbarHeight(aTabId, aHeight);
}
bool EmbedLiteWindowParent::SetScreenProperties(int32_t aDepth,
                                                float aDensity, float aDpi)
{ return aDepth > 0 && std::isfinite(aDensity) && aDensity > 0 &&
    std::isfinite(aDpi) && aDpi > 0 && CanSendChromeSessionCommand() &&
    SendSetContentScreenProperties(aDepth, aDensity, aDpi); }

void EmbedLiteWindowParent::SetInputListener(
    EmbedLiteChromeInputSessionListener* aListener)
{
  mChromeInputSessionListener = aListener;
  if (mChromeInputSessionListener && CanSendChromeSessionCommand()) {
    ReplayChromeInputContext();
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

bool EmbedLiteWindowParent::SendTextEvent(
    const char* aCommit, const char* aPreEdit,
    int32_t aReplacementStart, int32_t aReplacementLength)
{
  return aCommit && aPreEdit && aReplacementLength >= 0 &&
    CanSendChromeSessionCommand() &&
    SendHandleTextEvent(nsDependentCString(aCommit),
                        nsDependentCString(aPreEdit),
                        aReplacementStart, aReplacementLength);
}

bool EmbedLiteWindowParent::SendKeyPress(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  return aDomKeyCode >= 0 && aCharCode >= 0 && aCharCode <= UINT16_MAX &&
    CanSendChromeSessionCommand() &&
    SendHandleKeyPressEvent(aDomKeyCode, aModifiers, aCharCode);
}

bool EmbedLiteWindowParent::SendKeyRelease(
    int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
  return aDomKeyCode >= 0 && aCharCode >= 0 && aCharCode <= UINT16_MAX &&
    CanSendChromeSessionCommand() &&
    SendHandleKeyReleaseEvent(aDomKeyCode, aModifiers, aCharCode);
}

bool EmbedLiteWindowParent::RestoreTabs(
    const EmbedLiteChromeRestoredTab* aTabs, uint32_t aTabCount,
    int32_t aSelectedTabIndex)
{
  if (!mChromeHosted || mDestroying || mRestoreTabsSent ||
      (!aTabCount && aSelectedTabIndex != -1) ||
      (aTabCount &&
       (!aTabs || aSelectedTabIndex < 0 ||
        static_cast<uint32_t>(aSelectedTabIndex) >= aTabCount))) {
    return false;
  }

  AutoTArray<EmbedLiteChromeTabRestoreData, 8> tabs;
  tabs.SetCapacity(aTabCount);
  for (uint32_t index = 0; index < aTabCount; ++index) {
    const EmbedLiteChromeRestoredTab& tab = aTabs[index];
    if (!tab.persistentId || !tab.history || !tab.historyCount ||
        tab.selectedHistoryIndex < 0 ||
        static_cast<uint32_t>(tab.selectedHistoryIndex) >=
          tab.historyCount) {
      return false;
    }
    for (uint32_t other = 0; other < index; ++other) {
      if (aTabs[other].persistentId == tab.persistentId) {
        return false;
      }
    }

    EmbedLiteChromeTabRestoreData data;
    data.persistentId() = tab.persistentId;
    data.selectedHistoryIndex() = tab.selectedHistoryIndex;
    data.history().SetCapacity(tab.historyCount);
    for (uint32_t historyIndex = 0; historyIndex < tab.historyCount;
         ++historyIndex) {
      const EmbedLiteChromeHistoryEntry& history =
        tab.history[historyIndex];
      if (!history.location || !history.location[0]) {
        return false;
      }

      EmbedLiteChromeHistoryData historyData;
      historyData.location().Assign(history.location);
      if (history.title) {
        historyData.title().Assign(history.title);
      }
      data.history().AppendElement(std::move(historyData));
    }
    tabs.AppendElement(std::move(data));
  }

  if (!SendRestoreTabs(tabs, aSelectedTabIndex)) {
    return false;
  }
  mRestoreTabsSent = true;
  return true;
}

bool EmbedLiteWindowParent::NewTab(const char* aURL,
                                   uint64_t aPersistentId,
                                   bool aFromExternal,
                                   bool aInBackground)
{
  return aURL && aURL[0] && aPersistentId &&
    CanSendChromeSessionCommand() &&
    SendNewTab(nsDependentCString(aURL), aPersistentId, aFromExternal,
               aInBackground);
}

bool EmbedLiteWindowParent::AssociateTab(uint64_t aTabId,
                                         uint64_t aPersistentId)
{
  return aTabId && aPersistentId && CanSendChromeSessionCommand() &&
    SendAssociateTab(aTabId, aPersistentId);
}

bool EmbedLiteWindowParent::SelectTab(uint64_t aTabId)
{
  return aTabId && CanSendChromeSessionCommand() && SendSelectTab(aTabId);
}

bool EmbedLiteWindowParent::CloseTab(uint64_t aTabId)
{
  if (!aTabId || !CanSendChromeSessionCommand() ||
      mPendingTabCloses.count(aTabId) || !SendCloseTab(aTabId)) {
    return false;
  }
  mPendingTabCloses.insert(aTabId);
  return true;
}

bool EmbedLiteWindowParent::ResolveBeforeUnloadPrompt(
    uint64_t aRequestId, uint64_t aTabId, bool aPermit)
{
  const auto prompt = mPendingBeforeUnloadPrompts.find(aRequestId);
  if (!aRequestId || !aTabId ||
      prompt == mPendingBeforeUnloadPrompts.end() ||
      prompt->second != aTabId || !CanSendChromeSessionCommand()) {
    return false;
  }

  if (!SendResolveBeforeUnloadPrompt(aRequestId, aTabId, aPermit)) {
    return false;
  }
  mPendingBeforeUnloadPrompts.erase(prompt);
  return true;
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

void EmbedLiteWindowParent::ReplayChromeInputContext()
{
  if (!mChromeInputSessionListener || !mHasInputContext) {
    return;
  }

  mChromeInputSessionListener->OnInputContextChanged(
    mInputEnabled, mInputOpen, mInputType.get(), mInputMode.get(),
    mActionHint.get(), mInputCause, mInputFocusChange);
}

void EmbedLiteWindowParent::ReplayTabSnapshot()
{
  if (!mChromeTabSessionListener || !mHasTabSnapshot) {
    return;
  }

  AutoTArray<EmbedLiteChromeTabSnapshot, 8> tabs;
  tabs.SetCapacity(mTabSnapshot.tabs().Length());
  for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
    tabs.AppendElement(EmbedLiteChromeTabSnapshot{
      tab.id(), tab.persistentId(), tab.locationRevision(),
      tab.location().get(), tab.title().get(), tab.loading(), tab.closing(),
      tab.discarded(), tab.canGoBack(), tab.canGoForward(), tab.progress(),
      tab.current(), tab.total()});
  }
  mChromeTabSessionListener->OnTabsChanged(
    mTabSnapshot.revision(), mTabSnapshot.selectedTabId(), tabs.Elements(),
    static_cast<uint32_t>(tabs.Length()));
}

void EmbedLiteWindowParent::UpdateSelectedChromeSessionState()
{
  const EmbedLiteChromeTabData* selected = nullptr;
  for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
    if (tab.id() == mTabSnapshot.selectedTabId()) {
      selected = &tab;
      break;
    }
  }

  if (!selected) {
    if (mHasLoadStarted && mChromeSessionListener) {
      mChromeSessionListener->OnLoadFinished();
    }
    mHasLocation = false;
    mHasLoadStarted = false;
    mHasLoadProgress = false;
    mHasTitle = false;
    mProjectedTabId = 0;
    return;
  }

  if (mProjectedTabId != selected->id()) {
    if (mHasLoadStarted && mChromeSessionListener) {
      mChromeSessionListener->OnLoadFinished();
    }
    mProjectedTabId = selected->id();
    mHasLocation = false;
    mHasLoadStarted = false;
    mHasLoadProgress = false;
    mHasTitle = false;
  }

  const bool locationChanged =
    !mHasLocation || mLocation != selected->location() ||
    mCanGoBack != selected->canGoBack() ||
    mCanGoForward != selected->canGoForward();
  mHasLocation = true;
  mLocation = selected->location();
  mCanGoBack = selected->canGoBack();
  mCanGoForward = selected->canGoForward();
  if (locationChanged && mChromeSessionListener) {
    mChromeSessionListener->OnLocationChanged(
      mLocation.get(), mCanGoBack, mCanGoForward);
  }

  if (selected->loading()) {
    const bool loadStarted =
      !mHasLoadStarted || mLoadStartedLocation != selected->location();
    mHasLoadStarted = true;
    mLoadStartedLocation = selected->location();
    if (loadStarted && mChromeSessionListener) {
      mChromeSessionListener->OnLoadStarted(mLoadStartedLocation.get());
    }

    const bool progressChanged =
      !mHasLoadProgress || mLoadProgress != selected->progress() ||
      mLoadCurrent != selected->current() || mLoadTotal != selected->total();
    mHasLoadProgress = true;
    mLoadProgress = selected->progress();
    mLoadCurrent = selected->current();
    mLoadTotal = selected->total();
    if (progressChanged && mChromeSessionListener) {
      mChromeSessionListener->OnLoadProgress(
        mLoadProgress, mLoadCurrent, mLoadTotal);
    }
  } else {
    if (mHasLoadStarted && mChromeSessionListener) {
      mChromeSessionListener->OnLoadFinished();
    }
    mHasLoadStarted = false;
    mHasLoadProgress = false;
  }

  const bool titleChanged = !mHasTitle || mTitle != selected->title();
  mHasTitle = true;
  mTitle = selected->title();
  if (titleChanged && mChromeSessionListener) {
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
  if (mDestroying) {
    return;
  }

  mDestroying = true;
  mPendingBeforeUnloadPrompts.clear();
  mPendingTabCloses.clear();
  if (mChromeSessionListener) {
    mChromeSessionListener->ChromeSessionDestroyed();
    mChromeSessionListener = nullptr;
  }
  if (mChromeTabSessionListener) {
    mChromeTabSessionListener->ChromeTabSessionDestroyed();
    mChromeTabSessionListener = nullptr;
  }
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->ChromeContentSessionDestroyed();
    mChromeContentSessionListener = nullptr;
  }
  if (mChromeInputSessionListener) {
    mChromeInputSessionListener->ChromeInputSessionDestroyed();
    mChromeInputSessionListener = nullptr;
  }
  mHasInputContext = false;
  mInputType.Truncate();
  mInputMode.Truncate();
  mActionHint.Truncate();
  if (mWindow) {
    mWindow->Destroyed();
  }
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvInitialized(const bool &success)
{
  MOZ_ASSERT(mWindow);
  if (success) {
    mInitialized = true;
    for (const nsCString& script : mContentRegistrations.FrameScripts()) {
      if (!SendLoadContentFrameScript(script)) {
        return IPC_FAIL(this, "Failed to replay chrome content frame script");
      }
    }
    for (const nsCString& name :
         mContentRegistrations.MessageListeners()) {
      if (!SendAddContentMessageListener(name)) {
        return IPC_FAIL(this, "Failed to replay chrome content listener");
      }
    }
    ReplayChromeInputContext();
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
  mPendingBeforeUnloadPrompts.clear();
  mPendingTabCloses.clear();
  if (mChromeSessionListener) {
    mChromeSessionListener->ChromeSessionDestroyed();
    mChromeSessionListener = nullptr;
  }
  if (mChromeTabSessionListener) {
    mChromeTabSessionListener->ChromeTabSessionDestroyed();
    mChromeTabSessionListener = nullptr;
  }
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->ChromeContentSessionDestroyed();
    mChromeContentSessionListener = nullptr;
  }
  if (mChromeInputSessionListener) {
    mChromeInputSessionListener->ChromeInputSessionDestroyed();
    mChromeInputSessionListener = nullptr;
  }
  mHasInputContext = false;
  mInputType.Truncate();
  mInputMode.Truncate();
  mActionHint.Truncate();
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

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnTabSnapshot(
    const EmbedLiteChromeSessionData& aSnapshot)
{
  if (!CanSendChromeSessionCommand()) {
    return IPC_OK();
  }
  if (!aSnapshot.revision()) {
    return IPC_FAIL(this, "Invalid chrome tab snapshot revision");
  }
  if (mHasTabSnapshot &&
      aSnapshot.revision() <= mTabSnapshot.revision()) {
    return IPC_OK();
  }

  bool selectedFound = false;
  for (uint32_t index = 0; index < aSnapshot.tabs().Length(); ++index) {
    const EmbedLiteChromeTabData& tab = aSnapshot.tabs()[index];
    if (!tab.id()) {
      return IPC_FAIL(this, "Invalid zero chrome tab id");
    }
    if (tab.discarded() && tab.loading()) {
      return IPC_FAIL(this, "Discarded chrome tab cannot be loading");
    }
    if (tab.id() == aSnapshot.selectedTabId()) {
      selectedFound = true;
    }
    for (uint32_t other = 0; other < index; ++other) {
      if (aSnapshot.tabs()[other].id() == tab.id()) {
        return IPC_FAIL(this, "Duplicate chrome tab id");
      }
      if (tab.persistentId() &&
          aSnapshot.tabs()[other].persistentId() == tab.persistentId()) {
        return IPC_FAIL(this, "Duplicate persistent chrome tab id");
      }
    }
  }
  if ((aSnapshot.tabs().IsEmpty() && aSnapshot.selectedTabId()) ||
      (!aSnapshot.tabs().IsEmpty() && !selectedFound)) {
    return IPC_FAIL(this, "Invalid selected chrome tab id");
  }

  mHasTabSnapshot = true;
  mTabSnapshot = aSnapshot;
  bool contentStateMatchesSelection = false;
  if (mHasContentState) {
    for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
      if (tab.id() == mTabSnapshot.selectedTabId()) {
        contentStateMatchesSelection =
          mContentState.tabId() == tab.id() &&
          mContentState.persistentId() == tab.persistentId() &&
          mContentState.locationRevision() == tab.locationRevision();
        break;
      }
    }
  }
  if (!contentStateMatchesSelection) {
    mHasContentState = false;
  }
  ReplayTabSnapshot();
  UpdateSelectedChromeSessionState();
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnBeforeUnloadPrompt(
    const EmbedLiteChromeBeforeUnloadData& aPrompt)
{
  if (!CanSendChromeSessionCommand()) {
    return IPC_OK();
  }
  if (!aPrompt.requestId() || !aPrompt.tabId() ||
      mPendingBeforeUnloadPrompts.count(aPrompt.requestId())) {
    return IPC_FAIL(this, "Invalid chrome beforeunload prompt");
  }

  if (!mChromeTabSessionListener) {
    Unused << SendResolveBeforeUnloadPrompt(
      aPrompt.requestId(), aPrompt.tabId(), false);
    return IPC_OK();
  }

  mPendingBeforeUnloadPrompts.emplace(
    aPrompt.requestId(), aPrompt.tabId());
  const EmbedLiteChromeBeforeUnloadPrompt prompt = {
    aPrompt.requestId(), aPrompt.tabId(), aPrompt.persistentId(),
    aPrompt.title().get(), aPrompt.text().get(),
    aPrompt.leaveLabel().get(), aPrompt.stayLabel().get()
  };
  mChromeTabSessionListener->OnBeforeUnloadPrompt(prompt);
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnTabCloseResult(
    const uint64_t& aTabId, const bool& aClosed)
{
  if (mDestroying) {
    return IPC_OK();
  }
  if (!aTabId) {
    return IPC_FAIL(this, "Invalid zero chrome tab close result id");
  }
  const auto pending = mPendingTabCloses.find(aTabId);
  if (pending == mPendingTabCloses.end()) {
    return IPC_FAIL(this, "Unsolicited chrome tab close result");
  }
  mPendingTabCloses.erase(pending);
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->OnTabCloseResult(aTabId, aClosed);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnContentStateChanged(
    const EmbedLiteChromeContentStateData& aState)
{
  if (!CanSendChromeSessionCommand()) {
    return IPC_OK();
  }
  const EmbedLiteChromeTabData* selected = nullptr;
  if (mHasTabSnapshot) {
    for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
      if (tab.id() == mTabSnapshot.selectedTabId()) {
        selected = &tab;
        break;
      }
    }
  }
  if (!aState.revision() || !std::isfinite(aState.viewportX()) ||
      !std::isfinite(aState.viewportY()) ||
      !std::isfinite(aState.viewportWidth()) ||
      aState.viewportWidth() < 0 ||
      !std::isfinite(aState.viewportHeight()) ||
      aState.viewportHeight() < 0) {
    return IPC_FAIL(this, "Malformed chrome content state");
  }
  if (!selected || aState.tabId() != selected->id() ||
      aState.persistentId() != selected->persistentId() ||
      aState.locationRevision() != selected->locationRevision() ||
      aState.securityStatus().Length() > kMaxContentDataLength ||
      aState.revision() <= mContentState.revision()) {
    return IPC_OK();
  }
  mContentState = aState;
  mHasContentState = true;
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->OnContentStateChanged(
      EmbedLiteChromeContentState{
        aState.tabId(), aState.persistentId(), aState.revision(),
        aState.locationRevision(), aState.securityStatus().get(),
        aState.securityState(),
        aState.fullscreen(), aState.firstPaint(), aState.firstPaintX(),
        aState.firstPaintY(), aState.scrollWidth(), aState.scrollHeight(),
        aState.scrollX(), aState.scrollY(), aState.viewportX(),
        aState.viewportY(), aState.viewportWidth(),
        aState.viewportHeight()});
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnContentAsyncMessage(
    const uint64_t& aTabId, const uint64_t& aPersistentId,
    const uint64_t& aLocationRevision,
    const nsString& aName, const nsString& aJSON)
{
  bool validTab = false;
  if (mHasTabSnapshot) {
    for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
      if (IsChromeContentNotificationCurrent(
            aTabId, aPersistentId, aLocationRevision,
            tab.id(), tab.persistentId(), tab.locationRevision())) {
        validTab = true;
        break;
      }
    }
  }
  if (!validTab ||
      !IsChromeContentNotificationBounded(
        aName.Length(), aJSON.Length())) {
    return IPC_OK();
  }
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->RecvAsyncMessage(
      aTabId, aPersistentId, aLocationRevision,
      aName.get(), aJSON.get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnContentWindowCloseRequested(
    const uint64_t& aTabId, const uint64_t& aPersistentId)
{
  bool validTab = false;
  if (mHasTabSnapshot) {
    for (const EmbedLiteChromeTabData& tab : mTabSnapshot.tabs()) {
      if (tab.id() == aTabId && tab.persistentId() == aPersistentId) {
        validTab = true;
        break;
      }
    }
  }
  if (!validTab) {
    return IPC_OK();
  }
  if (mChromeContentSessionListener) {
    mChromeContentSessionListener->OnWindowCloseRequested(
      aTabId, aPersistentId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowParent::RecvOnInputContextChanged(
    const int32_t& aEnabled, const int32_t& aOpen,
    const nsString& aInputType, const nsString& aInputMode,
    const nsString& aActionHint, const int32_t& aCause,
    const int32_t& aFocusChange)
{
  if (!mChromeHosted || mDestroying) {
    return IPC_OK();
  }

  mHasInputContext = true;
  mInputEnabled = aEnabled;
  mInputOpen = aOpen;
  mInputType = aInputType;
  mInputMode = aInputMode;
  mActionHint = aActionHint;
  mInputCause = aCause;
  mInputFocusChange = aFocusChange;
  if (CanSendChromeSessionCommand()) {
    ReplayChromeInputContext();
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
