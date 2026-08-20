/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include <cmath>
#include <utility>

#include "nsWindow.h"
#include "EmbedLiteAppChild.h"
#include "EmbedLiteChromeSessionChild.h"
#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteWindowChild.h"
#include "mozilla/Unused.h"
#include "Hal.h"
#include "gfxPlatform.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsAppShellCID.h"
#include "nsIAppShellService.h"
#include "nsIAppWindow.h"
#include "nsIBaseWindow.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

using namespace mozilla::dom;

namespace mozilla {

namespace embedlite {

namespace {
static std::map<uint32_t, EmbedLiteWindowChild*> sWindowChildMap;

const char*
ScreenOrientationName(hal::ScreenOrientation aOrientation)
{
  switch (aOrientation) {
    case hal::ScreenOrientation::PortraitPrimary:
      return "portrait-primary";
    case hal::ScreenOrientation::LandscapePrimary:
      return "landscape-primary";
    case hal::ScreenOrientation::PortraitSecondary:
      return "portrait-secondary";
    case hal::ScreenOrientation::LandscapeSecondary:
      return "landscape-secondary";
    default:
      return nullptr;
  }
}

void
SendContentOrientationChanged(uint32_t aWindowID, hal::ScreenOrientation aOrientation)
{
  const char* orientationName = ScreenOrientationName(aOrientation);
  if (!orientationName) {
    return;
  }

  EmbedLiteAppChild* app = EmbedLiteAppChild::GetInstance();
  if (!app) {
    return;
  }

  nsAutoCString json;
  json.AssignLiteral("{\"orientation\":\"");
  json.Append(orientationName);
  json.AppendLiteral("\"}");
  const NS_ConvertUTF8toUTF16 message(json);

  app->SendAsyncMessageToViewsForWindowID(aWindowID,
                                          u"embed:contentOrientationChanged",
                                          message.get());
}
} // namespace

EmbedLiteWindowChild::EmbedLiteWindowChild(
    const uint16_t &width, const uint16_t &height, const uint32_t &aId,
    EmbedLiteWindowListener *aListener, const bool &chromeHosted,
    const nsCString &initialContentURI)
  : mId(aId)
  , mListener(aListener)
  , mWidget(nullptr)
  , mBounds(0, 0, width, height)
  , mRotation(mozilla::ROTATION_0)
  , mInitialContentURI(initialContentURI)
  , mPendingSelectedTabIndex(-1)
  , mChromeHosted(chromeHosted)
  , mRestoreTabsReceived(false)
  , mInitialized(false)
  , mDestroyAfterInit(false)
  , mDestroying(false)
  , mDepth(32)
  , mDensity(1.0)
  , mDpi(96)
{
  MOZ_ASSERT(sWindowChildMap.find(aId) == sWindowChildMap.end());
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mChromeHosted || mInitialContentURI.IsEmpty());
  sWindowChildMap[aId] = this;

  MOZ_COUNT_CTOR(EmbedLiteWindowChild);

  mCreateWidgetTask = NewCancelableRunnableMethod("EmbedLiteWindowChild::CreateWidget",
                                                  this,
                                                  &EmbedLiteWindowChild::CreateWidget);
  MessageLoop::current()->PostTask(mCreateWidgetTask.forget());

  // Make sure gfx platform is initialized and ready to go.
  gfxPlatform::GetPlatform();
}

EmbedLiteWindowChild *EmbedLiteWindowChild::From(const uint32_t id)
{
  std::map<uint32_t, EmbedLiteWindowChild*>::const_iterator it = sWindowChildMap.find(id);
  if (it != sWindowChildMap.end()) {
    return it->second;
  }
  return nullptr;
}

bool EmbedLiteWindowChild::RequestChromeTabBeforeUnloadPrompt(
    BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aBrowsingContext || !aPromise) {
    return false;
  }

  for (const auto& window : sWindowChildMap) {
    EmbedLiteWindowChild* child = window.second;
    if (child->mChromeSession &&
        child->mChromeSession->RequestBeforeUnloadPrompt(
          aBrowsingContext, aTitle, aText, aLeaveLabel, aStayLabel,
          aPromise)) {
      return true;
    }
  }
  return false;
}

EmbedLiteWindowChild::~EmbedLiteWindowChild()
{
  DestroyChromeAppWindow();

  MOZ_ASSERT(sWindowChildMap.find(mId) != sWindowChildMap.end());
  sWindowChildMap.erase(sWindowChildMap.find(mId));

  MOZ_COUNT_DTOR(EmbedLiteWindowChild);

  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }
}

nsWindow *EmbedLiteWindowChild::GetWidget() const
{
  return static_cast<nsWindow*>(mWidget.get());
}

void EmbedLiteWindowChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  mDestroying = true;
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvDestroy()
{
  if (!mInitialized) {
    if (!mWidget) {
      mDestroyAfterInit = true;
      return IPC_OK();
    }
    // Chrome AppWindow initialization is asynchronous. Once the root widget
    // exists it is safe to cancel that initialization and tear it down.
    mInitialized = true;
  }

  if (mDestroying) {
    return IPC_OK();
  }
  mDestroying = true;

  LOGT("destroy");
  DestroyChromeAppWindow();
  if (mWidget) {
    mWidget->Destroy();
    mWidget = nullptr;
  }
  Unused << SendDestroyed();
  PEmbedLiteWindowChild::Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvLoadURL(const nsCString& aURL,
                                  const bool& aFromExternal)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->LoadURL(aURL, aFromExternal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvGoBack(const bool& aRequireUserInteraction,
                                 const bool& aUserActivation)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->GoBack(aRequireUserInteraction,
                                     aUserActivation);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvGoForward(const bool& aRequireUserInteraction,
                                    const bool& aUserActivation)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->GoForward(aRequireUserInteraction,
                                        aUserActivation);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvStopLoad()
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->StopLoad();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvReload(const bool& aHardReload)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->Reload(aHardReload);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvRestoreTabs(
    const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
    const int32_t& aSelectedTabIndex)
{
  if (mDestroying) {
    return IPC_OK();
  }
  if (mRestoreTabsReceived) {
    return IPC_FAIL(this, "Duplicate chrome tab restore batch");
  }

  mRestoreTabsReceived = true;
  if (mChromeSession) {
    if (!mChromeSession->RestoreTabs(aTabs, aSelectedTabIndex)) {
      return IPC_FAIL(this, "Invalid chrome tab restore batch");
    }
  } else {
    mPendingRestoreTabs.SetCapacity(aTabs.Length());
    for (const EmbedLiteChromeTabRestoreData& source : aTabs) {
      EmbedLiteChromeTabRestoreData tab;
      tab.persistentId() = source.persistentId();
      tab.selectedHistoryIndex() = source.selectedHistoryIndex();
      tab.history().SetCapacity(source.history().Length());
      for (const EmbedLiteChromeHistoryData& sourceEntry :
           source.history()) {
        EmbedLiteChromeHistoryData entry;
        entry.location() = sourceEntry.location();
        entry.title() = sourceEntry.title();
        tab.history().AppendElement(std::move(entry));
      }
      mPendingRestoreTabs.AppendElement(std::move(tab));
    }
    mPendingSelectedTabIndex = aSelectedTabIndex;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvNewTab(const nsCString& aURL,
                                 const uint64_t& aPersistentId,
                                 const bool& aFromExternal,
                                 const bool& aInBackground)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->NewTab(
      aURL, aPersistentId, aFromExternal, aInBackground);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvAssociateTab(const uint64_t& aTabId,
                                       const uint64_t& aPersistentId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->AssociateTab(aTabId, aPersistentId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvSelectTab(const uint64_t& aTabId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SelectTab(aTabId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvCloseTab(const uint64_t& aTabId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->CloseTab(aTabId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvResolveBeforeUnloadPrompt(
    const uint64_t& aRequestId, const uint64_t& aTabId,
    const bool& aPermit)
{
  if (mChromeSession && !mDestroying) {
    mChromeSession->ResolveBeforeUnloadPrompt(
      aRequestId, aTabId, aPermit);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvLoadContentFrameScript(
    const nsCString& aURI)
{ if (aURI.IsEmpty() || aURI.Length() > 1024 * 1024) return IPC_FAIL(this, "Invalid frame script URI"); if (mChromeSession && !mDestroying) Unused << mChromeSession->LoadFrameScript(aURI); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvAddContentMessageListener(
    const nsCString& aName)
{ if (aName.IsEmpty() || aName.Length() > 1024) return IPC_FAIL(this, "Invalid message name"); if (mChromeSession && !mDestroying) Unused << mChromeSession->AddMessageListener(aName); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvRemoveContentMessageListener(
    const nsCString& aName)
{ if (aName.IsEmpty() || aName.Length() > 1024) return IPC_FAIL(this, "Invalid message name"); if (mChromeSession && !mDestroying) Unused << mChromeSession->RemoveMessageListener(aName); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSendContentAsyncMessage(
    const uint64_t& aTabId, const nsString& aName, const nsString& aJSON)
{ if (!aTabId || aName.IsEmpty() || aName.Length() > 1024 || aJSON.Length() > 1024 * 1024) return IPC_FAIL(this, "Invalid content message"); if (mChromeSession && !mDestroying) Unused << mChromeSession->SendAsyncMessage(aTabId, aName, aJSON); return IPC_OK(); }

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSendContentMouseEvent(
    const uint64_t& aTabId, const uint8_t& aType, const int32_t& aX,
    const int32_t& aY, const uint64_t& aTime, const uint32_t& aButton,
    const uint32_t& aButtons, const uint32_t& aModifiers,
    const uint32_t& aClickCount)
{ if (!aTabId || aType > 2 || aButton > 4 || aButtons > 0x1f || aClickCount > 3) return IPC_FAIL(this, "Invalid content mouse event"); if (mChromeSession && !mDestroying) Unused << mChromeSession->SendMouseEvent(aTabId, aType, aX, aY, aTime, aButton, aButtons, aModifiers, aClickCount); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSendContentWheelEvent(
    const uint64_t& aTabId, const int32_t& aX, const int32_t& aY,
    const uint64_t& aTime, const double& aDeltaX, const double& aDeltaY,
    const uint32_t& aDeltaMode, const uint32_t& aModifiers)
{ if (!aTabId || !std::isfinite(aDeltaX) || !std::isfinite(aDeltaY) || aDeltaMode > 2) return IPC_FAIL(this, "Invalid content wheel event"); if (mChromeSession && !mDestroying) Unused << mChromeSession->SendWheelEvent(aTabId, aX, aY, aTime, aDeltaX, aDeltaY, aDeltaMode, aModifiers); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvContentScrollTo(
    const uint64_t& aTabId, const int32_t& aX, const int32_t& aY)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->ScrollTo(aTabId, aX, aY); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvContentScrollBy(
    const uint64_t& aTabId, const int32_t& aX, const int32_t& aY)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->ScrollBy(aTabId, aX, aY); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvContentZoomToRect(
    const uint64_t& aTabId, const float& aX, const float& aY,
    const float& aWidth, const float& aHeight)
{ if (!aTabId || !std::isfinite(aX) || !std::isfinite(aY) || !std::isfinite(aWidth) || aWidth < 0 || !std::isfinite(aHeight) || aHeight < 0) return IPC_FAIL(this, "Invalid content zoom rect"); if (mChromeSession && !mDestroying) Unused << mChromeSession->ZoomToRect(aTabId, aX, aY, aWidth, aHeight); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentDesktopMode(const uint64_t& aTabId, const bool& aValue)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->SetDesktopMode(aTabId, aValue); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentThrottlePainting(const uint64_t& aTabId, const bool& aValue)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->SetThrottlePainting(aTabId, aValue); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSuspendContentTimeouts(const uint64_t& aTabId)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->SuspendTimeouts(aTabId); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvResumeContentTimeouts(const uint64_t& aTabId)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->ResumeTimeouts(aTabId); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentHttpUserAgent(const uint64_t& aTabId, const nsString& aValue)
{ if (aValue.Length() > 1024) return IPC_FAIL(this, "Invalid user agent"); if (mChromeSession && !mDestroying) Unused << mChromeSession->SetHttpUserAgent(aTabId, aValue); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentMargins(const uint64_t& aTabId, const int32_t& aTop, const int32_t& aRight, const int32_t& aBottom, const int32_t& aLeft)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->SetMargins(aTabId, aTop, aRight, aBottom, aLeft); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentSafeAreaInsets(const uint64_t& aTabId, const int32_t& aTop, const int32_t& aRight, const int32_t& aBottom, const int32_t& aLeft)
{ if (mChromeSession && !mDestroying) Unused << mChromeSession->SetSafeAreaInsets(aTabId, aTop, aRight, aBottom, aLeft); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentDynamicToolbarHeight(const uint64_t& aTabId, const int32_t& aHeight)
{ if (aHeight < 0) return IPC_FAIL(this, "Invalid toolbar height"); if (mChromeSession && !mDestroying) Unused << mChromeSession->SetDynamicToolbarHeight(aTabId, aHeight); return IPC_OK(); }
mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentScreenProperties(const int32_t& aDepth, const float& aDensity, const float& aDpi)
{ if (aDepth <= 0 || !std::isfinite(aDensity) || aDensity <= 0 || !std::isfinite(aDpi) || aDpi <= 0) return IPC_FAIL(this, "Invalid screen properties"); SetScreenProperties(aDepth, aDensity, aDpi); return IPC_OK(); }

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvSetActive(const bool& aActive)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SetActive(aActive);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvSetFocused(const bool& aFocused)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SetFocused(aFocused);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvHandleTextEvent(
    const nsCString& aCommit, const nsCString& aPreEdit,
    const int32_t& aReplacementStart,
    const int32_t& aReplacementLength)
{
  if (aReplacementLength < 0) {
    return IPC_FAIL(this, "Invalid chrome text replacement length");
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SendTextEvent(
      NS_ConvertUTF8toUTF16(aCommit), NS_ConvertUTF8toUTF16(aPreEdit),
      aReplacementStart, aReplacementLength);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvHandleKeyPressEvent(
    const int32_t& aDomKeyCode, const int32_t& aModifiers,
    const int32_t& aCharCode)
{
  if (aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return IPC_FAIL(this, "Invalid chrome key press");
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SendKeyPress(
      aDomKeyCode, aModifiers, aCharCode);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvHandleKeyReleaseEvent(
    const int32_t& aDomKeyCode, const int32_t& aModifiers,
    const int32_t& aCharCode)
{
  if (aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return IPC_FAIL(this, "Invalid chrome key release");
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->SendKeyRelease(
      aDomKeyCode, aModifiers, aCharCode);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteWindowChild::RecvReceiveInputEvent(const MultiTouchInput& aEvent)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    Unused << mChromeSession->ReceiveInputEvent(aEvent);
  }
  return IPC_OK();
}

void EmbedLiteWindowChild::ChromeInputContextChanged(
    const widget::InputContext& aContext,
    const widget::InputContextAction& aAction)
{
  if (!mChromeHosted || mDestroying || !CanSend()) {
    return;
  }

  Unused << SendOnInputContextChanged(
    static_cast<int32_t>(aContext.mIMEState.mEnabled),
    static_cast<int32_t>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType, aContext.mHTMLInputMode, aContext.mActionHint,
    static_cast<int32_t>(aAction.mCause),
    static_cast<int32_t>(aAction.mFocusChange));
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetSize(const gfxSize &aSize)
{
  mBounds = LayoutDeviceIntRect(0, 0, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  LOGT("this:%p width: %f, height: %f as int w: %d h: %d", this, aSize.width, aSize.height, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetSize(aSize.width, aSize.height);
    widget->UpdateBounds(true);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentOrientation(const uint32_t &aRotation)
{
  LOGT("this:%p", this);
  mRotation = static_cast<mozilla::ScreenRotation>(aRotation);
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetRotation(mRotation);
    widget->UpdateBounds(true);
  }

  int32_t colorDepth, pixelDepth;

  RefPtr<widget::Screen> screen =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();

  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);

  hal::ScreenOrientation orientation = hal::ScreenOrientation::Default;
  uint16_t angle = 0;
  switch (mRotation) {
    case mozilla::ROTATION_0:
      angle = 0;
      orientation = hal::ScreenOrientation::PortraitPrimary;
      break;
    case mozilla::ROTATION_90:
      angle = 90;
      orientation = hal::ScreenOrientation::LandscapePrimary;
      break;
    case mozilla::ROTATION_180:
      angle = 180;
      orientation = hal::ScreenOrientation::PortraitSecondary;
      break;
    case mozilla::ROTATION_270:
      angle = 270;
      orientation = hal::ScreenOrientation::LandscapeSecondary;
      break;
    default:
      break;
  }

  nsIntRect rect(mBounds.X(), mBounds.Y(), mBounds.Width(), mBounds.Height());

  RefreshScreen();
  SendContentOrientationChanged(mId, orientation);

  return IPC_OK();
}

void EmbedLiteWindowChild::CreateWidget()
{
  LOGT("this:%p", this);
  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }

  if (mDestroyAfterInit && !mWidget) {
    mInitialized = true;
    RecvDestroy();
    return;
  }

  mWidget = new nsWindow(this);
  GetWidget()->SetRotation(mRotation);

  widget::InitData widgetInit;
  widgetInit.mClipChildren = true;
  widgetInit.mWindowType = widget::WindowType::TopLevel;

  // nsWindow::CreateCompositor() reads back Size
  // when it creates the compositor.
  const nsresult createResult =
    mWidget->Create(nullptr, mBounds, &widgetInit);
  if (NS_FAILED(createResult)) {
    mInitialized = true;
    Unused << SendInitialized(false);
    RecvDestroy();
    return;
  }
  if (mChromeHosted) {
    GetWidget()->SetActive(true);
  }
  GetWidget()->UpdateBounds(true);

  // Initialize ScreenManager
  RefreshScreen();

  if (!mChromeHosted) {
    ChromeSessionInitializationFinished(true);
  } else if (!CreateChromeAppWindow()) {
    ChromeSessionInitializationFinished(false);
  }
}

void
EmbedLiteWindowChild::ChromeSessionInitializationFinished(bool aSuccess)
{
  if (mInitialized || mDestroying) {
    return;
  }

  mInitialized = true;
  if (mDestroyAfterInit) {
    RecvDestroy();
    return;
  }

  Unused << SendInitialized(aSuccess);
  if (!aSuccess) {
    RecvDestroy();
  }
}

bool EmbedLiteWindowChild::CreateChromeAppWindow()
{
  MOZ_ASSERT(mChromeHosted);
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(!mChromeWindow);

  nsCOMPtr<nsIURI> chromeURI;
  nsresult rv = NS_NewURI(
    getter_AddRefs(chromeURI),
    "chrome://embedlitechrome/content/browser.xhtml");
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIAppShellService> appShell =
    do_GetService(NS_APPSHELLSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv) || !appShell) {
    return false;
  }

  AutoEmbedLiteChromeWindowHost hostReservation(GetWidget());
  if (!hostReservation.IsValid()) {
    return false;
  }

  const uint32_t chromeFlags =
    nsIWebBrowserChrome::CHROME_OPENAS_CHROME |
    nsIWebBrowserChrome::CHROME_WINDOW_RESIZE |
    nsIWebBrowserChrome::CHROME_SCROLLBARS |
    nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
  rv = appShell->CreateTopLevelWindow(
    nullptr, chromeURI, chromeFlags, mBounds.Width(), mBounds.Height(),
    getter_AddRefs(mChromeWindow));
  if (NS_FAILED(rv) || !mChromeWindow || !hostReservation.WasConsumed()) {
    DestroyChromeAppWindow();
    return false;
  }

  mChromeSession = new EmbedLiteChromeSessionChild(this);
  rv = mChromeSession->Start(mChromeWindow, mInitialContentURI);
  if (NS_FAILED(rv)) {
    DestroyChromeAppWindow();
    return false;
  }
  if (mRestoreTabsReceived &&
      !mChromeSession->RestoreTabs(mPendingRestoreTabs,
                                   mPendingSelectedTabIndex)) {
    DestroyChromeAppWindow();
    return false;
  }
  mPendingRestoreTabs.Clear();
  mPendingSelectedTabIndex = -1;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mChromeWindow, &rv);
  if (NS_FAILED(rv) || !baseWindow ||
      NS_FAILED(baseWindow->SetVisibility(true))) {
    DestroyChromeAppWindow();
    return false;
  }

  return true;
}

void EmbedLiteWindowChild::DestroyChromeAppWindow()
{
  if (mChromeSession) {
    mChromeSession->Shutdown();
    mChromeSession = nullptr;
  }

  if (!mChromeWindow) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mChromeWindow);
  if (baseWindow) {
    Unused << baseWindow->Destroy();
  }
  mChromeWindow = nullptr;
}

void EmbedLiteWindowChild::RefreshScreen()
{
  LayoutDeviceIntRect rect;
  if (mRotation == mozilla::ROTATION_0 ||
      mRotation == mozilla::ROTATION_180)
    rect = mBounds;
  else
    rect = LayoutDeviceIntRect(0, 0, mBounds.Height(), mBounds.Width());

  const float density = GetDensity();
  AutoTArray<RefPtr<widget::Screen>, 1> screenList;
  auto screen = MakeRefPtr<widget::Screen>(
      rect, rect, mDepth, mDepth, 0, DesktopToLayoutDeviceScale(density),
      CSSToLayoutDeviceScale(density), mDpi,
      widget::Screen::IsPseudoDisplay::No, widget::Screen::IsHDR::No);
  screenList.AppendElement(screen.forget());
  widget::ScreenManager::Refresh(std::move(screenList));
}

void EmbedLiteWindowChild::SetScreenProperties(const int &depth, const float &density, const float &dpi)
{
  bool refresh = false;
  float normalizedDensity = density > 0.0f ? density : 1.0f;

  if (depth != mDepth) {
    mDepth = depth;
    refresh = true;
  }

  if (normalizedDensity != mDensity) {
    mDensity = normalizedDensity;
    refresh = true;
  }

  if (dpi != mDpi) {
    mDpi = dpi;
    refresh = true;
  }

  if (refresh) {
    RefreshScreen();
    if (mWidget) {
      GetWidget()->BackingScaleFactorChanged();
    }
  }
}

} // namespace embedlite
} // namespace mozilla
