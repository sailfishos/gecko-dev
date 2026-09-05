/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include <cmath>
#include <utility>

#include "nsWindow.h"
#include "EmbedLiteChromeSessionChild.h"
#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteHostedWindow.h"
#include "EmbedLiteWindowParent.h"
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
static std::map<uint32_t, EmbedLiteHostedWindow*> sHostedWindows;
} // namespace

EmbedLiteHostedWindow::EmbedLiteHostedWindow(
    uint16_t aWidth, uint16_t aHeight, uint32_t aId,
    EmbedLiteWindowParent* aOwner, const nsACString& aInitialContentURI,
    bool aPrivateBrowsing)
  : mId(aId)
  , mOwner(aOwner)
  , mWidget(nullptr)
  , mBounds(0, 0, aWidth, aHeight)
  , mRotation(mozilla::ROTATION_0)
  , mInitialContentURI(aInitialContentURI)
  , mPendingSelectedTabIndex(-1)
  , mPrivateBrowsing(aPrivateBrowsing)
  , mRestoreTabsReceived(false)
  , mInitialized(false)
  , mDestroyAfterInit(false)
  , mDestroying(false)
  , mDepth(32)
  , mDensity(1.0)
  , mDpi(96)
{
  MOZ_ASSERT(sHostedWindows.find(aId) == sHostedWindows.end());
  MOZ_ASSERT(mOwner);
  sHostedWindows[aId] = this;

  MOZ_COUNT_CTOR(EmbedLiteHostedWindow);

  mCreateWidgetTask = NewCancelableRunnableMethod("EmbedLiteHostedWindow::CreateWidget",
                                                  this,
                                                  &EmbedLiteHostedWindow::CreateWidget);
  MessageLoop::current()->PostTask(mCreateWidgetTask.forget());

  // Make sure gfx platform is initialized and ready to go.
  gfxPlatform::GetPlatform();
}

EmbedLiteHostedWindow* EmbedLiteHostedWindow::From(uint32_t aId)
{
  const auto it = sHostedWindows.find(aId);
  if (it != sHostedWindows.end()) {
    return it->second;
  }
  return nullptr;
}

bool EmbedLiteHostedWindow::RequestChromeTabBeforeUnloadPrompt(
    BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aBrowsingContext || !aPromise) {
    return false;
  }

  for (const auto& window : sHostedWindows) {
    EmbedLiteHostedWindow* hosted = window.second;
    if (hosted->mChromeSession &&
        hosted->mChromeSession->RequestBeforeUnloadPrompt(
          aBrowsingContext, aTitle, aText, aLeaveLabel, aStayLabel,
          aPromise)) {
      return true;
    }
  }
  return false;
}

EmbedLiteHostedWindow::~EmbedLiteHostedWindow()
{
  DestroyChromeAppWindow();

  const auto window = sHostedWindows.find(mId);
  MOZ_ASSERT(window != sHostedWindows.end());
  if (window != sHostedWindows.end()) {
    sHostedWindows.erase(window);
  }

  MOZ_COUNT_DTOR(EmbedLiteHostedWindow);

  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }
}

nsWindow *EmbedLiteHostedWindow::GetWidget() const
{
  return static_cast<nsWindow*>(mWidget.get());
}

EmbedLiteWindowListener* EmbedLiteHostedWindow::GetListener() const
{
  return mOwner ? mOwner->GetListener() : nullptr;
}

bool EmbedLiteHostedWindow::OnTabSnapshot(
    const EmbedLiteChromeSessionData& aSnapshot)
{
  return mOwner && mOwner->OnTabSnapshot(aSnapshot);
}

bool EmbedLiteHostedWindow::OnBeforeUnloadPrompt(
    const EmbedLiteChromeBeforeUnloadData& aPrompt)
{
  return mOwner && mOwner->OnBeforeUnloadPrompt(aPrompt);
}

bool EmbedLiteHostedWindow::OnTabCloseResult(uint64_t aTabId, bool aClosed)
{
  return mOwner && mOwner->OnTabCloseResult(aTabId, aClosed);
}

bool EmbedLiteHostedWindow::OnContentStateChanged(
    const EmbedLiteChromeContentStateData& aState)
{
  return mOwner && mOwner->OnContentStateChanged(aState);
}

bool EmbedLiteHostedWindow::OnContentAsyncMessage(
    uint64_t aTabId, uint64_t aPersistentId,
    uint64_t aLocationRevision, const nsAString& aName,
    const nsAString& aJSON)
{
  return mOwner && mOwner->OnContentAsyncMessage(
    aTabId, aPersistentId, aLocationRevision, aName, aJSON);
}

bool EmbedLiteHostedWindow::OnContentWindowCloseRequested(
    uint64_t aTabId, uint64_t aPersistentId)
{
  return mOwner &&
    mOwner->OnContentWindowCloseRequested(aTabId, aPersistentId);
}

bool EmbedLiteHostedWindow::Destroy()
{
  if (!mInitialized) {
    if (!mWidget) {
      mDestroyAfterInit = true;
      return true;
    }
    // Chrome AppWindow initialization is asynchronous. Once the root widget
    // exists it is safe to cancel that initialization and tear it down.
    mInitialized = true;
  }

  if (mDestroying) {
    return true;
  }
  mDestroying = true;

  LOGT("destroy");
  DestroyChromeAppWindow();
  if (mWidget) {
    mWidget->Destroy();
    mWidget = nullptr;
  }
  RefPtr<EmbedLiteHostedWindow> deathGrip(this);
  EmbedLiteWindowParent* owner = mOwner;
  mOwner = nullptr;
  if (owner) {
    owner->OnDestroyed();
  }
  return true;
}

bool EmbedLiteHostedWindow::LoadURL(const nsACString& aURL,
                                  bool aFromExternal)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->LoadURL(aURL, aFromExternal);
  }
  return true;
}

bool EmbedLiteHostedWindow::GoBack(bool aRequireUserInteraction,
                                 bool aUserActivation)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->GoBack(aRequireUserInteraction,
                                     aUserActivation);
  }
  return true;
}

bool EmbedLiteHostedWindow::GoForward(bool aRequireUserInteraction,
                                    bool aUserActivation)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->GoForward(aRequireUserInteraction,
                                        aUserActivation);
  }
  return true;
}

bool EmbedLiteHostedWindow::StopLoad()
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->StopLoad();
  }
  return true;
}

bool EmbedLiteHostedWindow::Reload(bool aHardReload)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->Reload(aHardReload);
  }
  return true;
}

bool EmbedLiteHostedWindow::RestoreTabs(
    const nsTArray<EmbedLiteChromeTabRestoreData>& aTabs,
    int32_t aSelectedTabIndex)
{
  if (mDestroying) {
    return true;
  }
  if (mRestoreTabsReceived) {
    return false;
  }

  mRestoreTabsReceived = true;
  if (mChromeSession) {
    if (!mChromeSession->RestoreTabs(aTabs, aSelectedTabIndex)) {
      return false;
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
  return true;
}

bool EmbedLiteHostedWindow::NewTab(const nsACString& aURL,
                                 uint64_t aPersistentId,
                                 bool aFromExternal,
                                 bool aInBackground)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->NewTab(
      aURL, aPersistentId, aFromExternal, aInBackground);
  }
  return true;
}

bool EmbedLiteHostedWindow::AssociateTab(uint64_t aTabId,
                                       uint64_t aPersistentId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->AssociateTab(aTabId, aPersistentId);
  }
  return true;
}

bool EmbedLiteHostedWindow::SelectTab(uint64_t aTabId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SelectTab(aTabId);
  }
  return true;
}

bool EmbedLiteHostedWindow::CloseTab(uint64_t aTabId)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->CloseTab(aTabId);
  }
  return true;
}

bool EmbedLiteHostedWindow::ResolveBeforeUnloadPrompt(
    uint64_t aRequestId, uint64_t aTabId,
    bool aPermit)
{
  if (mChromeSession && !mDestroying) {
    mChromeSession->ResolveBeforeUnloadPrompt(
      aRequestId, aTabId, aPermit);
  }
  return true;
}

bool EmbedLiteHostedWindow::LoadContentFrameScript(
    const nsACString& aURI)
{ if (aURI.IsEmpty() || aURI.Length() > 1024 * 1024) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->LoadFrameScript(aURI); return true; }
bool EmbedLiteHostedWindow::AddContentMessageListener(
    const nsACString& aName)
{ if (aName.IsEmpty() || aName.Length() > 1024) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->AddMessageListener(aName); return true; }
bool EmbedLiteHostedWindow::RemoveContentMessageListener(
    const nsACString& aName)
{ if (aName.IsEmpty() || aName.Length() > 1024) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->RemoveMessageListener(aName); return true; }
bool EmbedLiteHostedWindow::SendContentAsyncMessage(
    uint64_t aTabId, const nsAString& aName, const nsAString& aJSON)
{ if (!aTabId || aName.IsEmpty() || aName.Length() > 1024 || aJSON.Length() > 1024 * 1024) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->SendAsyncMessage(aTabId, aName, aJSON); return true; }

bool EmbedLiteHostedWindow::SendContentMouseEvent(
    uint64_t aTabId, uint8_t aType, int32_t aX,
    int32_t aY, uint64_t aTime, uint32_t aButton,
    uint32_t aButtons, uint32_t aModifiers,
    uint32_t aClickCount)
{ if (!aTabId || aType > 2 || aButton > 4 || aButtons > 0x1f || aClickCount > 3) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->SendMouseEvent(aTabId, aType, aX, aY, aTime, aButton, aButtons, aModifiers, aClickCount); return true; }
bool EmbedLiteHostedWindow::SendContentWheelEvent(
    uint64_t aTabId, int32_t aX, int32_t aY,
    uint64_t aTime, double aDeltaX, double aDeltaY,
    uint32_t aDeltaMode, uint32_t aModifiers)
{ if (!aTabId || !std::isfinite(aDeltaX) || !std::isfinite(aDeltaY) || aDeltaMode > 2) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->SendWheelEvent(aTabId, aX, aY, aTime, aDeltaX, aDeltaY, aDeltaMode, aModifiers); return true; }
bool EmbedLiteHostedWindow::ContentScrollTo(
    uint64_t aTabId, int32_t aX, int32_t aY)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->ScrollTo(aTabId, aX, aY); return true; }
bool EmbedLiteHostedWindow::ContentScrollBy(
    uint64_t aTabId, int32_t aX, int32_t aY)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->ScrollBy(aTabId, aX, aY); return true; }
bool EmbedLiteHostedWindow::ContentZoomToRect(
    uint64_t aTabId, float aX, float aY,
    float aWidth, float aHeight)
{ if (!aTabId || !std::isfinite(aX) || !std::isfinite(aY) || !std::isfinite(aWidth) || aWidth < 0 || !std::isfinite(aHeight) || aHeight < 0) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->ZoomToRect(aTabId, aX, aY, aWidth, aHeight); return true; }
bool EmbedLiteHostedWindow::SetContentDesktopMode(uint64_t aTabId, bool aValue)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->SetDesktopMode(aTabId, aValue); return true; }
bool EmbedLiteHostedWindow::SetContentJavascriptEnabled(bool aEnabled)
{ return mChromeSession && !mDestroying &&
    mChromeSession->SetJavascriptEnabled(aEnabled); }
bool EmbedLiteHostedWindow::SetContentThrottlePainting(uint64_t aTabId, bool aValue)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->SetThrottlePainting(aTabId, aValue); return true; }
bool EmbedLiteHostedWindow::SuspendContentTimeouts(uint64_t aTabId)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->SuspendTimeouts(aTabId); return true; }
bool EmbedLiteHostedWindow::ResumeContentTimeouts(uint64_t aTabId)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->ResumeTimeouts(aTabId); return true; }
bool EmbedLiteHostedWindow::SetContentHttpUserAgent(uint64_t aTabId, const nsAString& aValue)
{ if (aValue.Length() > 1024) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->SetHttpUserAgent(aTabId, aValue); return true; }
bool EmbedLiteHostedWindow::SetContentMargins(uint64_t aTabId, int32_t aTop, int32_t aRight, int32_t aBottom, int32_t aLeft)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->SetMargins(aTabId, aTop, aRight, aBottom, aLeft); return true; }
bool EmbedLiteHostedWindow::SetContentSafeAreaInsets(uint64_t aTabId, int32_t aTop, int32_t aRight, int32_t aBottom, int32_t aLeft)
{ if (mChromeSession && !mDestroying) (void) mChromeSession->SetSafeAreaInsets(aTabId, aTop, aRight, aBottom, aLeft); return true; }
bool EmbedLiteHostedWindow::SetContentDynamicToolbarHeight(uint64_t aTabId, int32_t aHeight)
{ if (aHeight < 0) return false; if (mChromeSession && !mDestroying) (void) mChromeSession->SetDynamicToolbarHeight(aTabId, aHeight); return true; }
bool EmbedLiteHostedWindow::SetContentScreenProperties(int32_t aDepth, float aDensity, float aDpi)
{ if (aDepth <= 0 || !std::isfinite(aDensity) || aDensity <= 0 || !std::isfinite(aDpi) || aDpi <= 0) return false; SetScreenProperties(aDepth, aDensity, aDpi); return true; }

bool EmbedLiteHostedWindow::SetActive(bool aActive)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SetActive(aActive);
  }
  return true;
}

bool EmbedLiteHostedWindow::SetFocused(bool aFocused)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SetFocused(aFocused);
  }
  return true;
}

bool EmbedLiteHostedWindow::HandleTextEvent(
    const nsACString& aCommit, const nsACString& aPreEdit,
    int32_t aReplacementStart,
    int32_t aReplacementLength)
{
  if (aReplacementLength < 0) {
    return false;
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SendTextEvent(
      NS_ConvertUTF8toUTF16(aCommit), NS_ConvertUTF8toUTF16(aPreEdit),
      aReplacementStart, aReplacementLength);
  }
  return true;
}

bool EmbedLiteHostedWindow::HandleTextEventAtOffset(
    const nsACString& aCommit, const nsACString& aPreEdit,
    uint32_t aReplacementOffset, int32_t aReplacementLength)
{
  if (aReplacementLength < 0) {
    return false;
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SendTextEventAtOffset(
      NS_ConvertUTF8toUTF16(aCommit), NS_ConvertUTF8toUTF16(aPreEdit),
      aReplacementOffset, aReplacementLength);
  }
  return true;
}

bool EmbedLiteHostedWindow::HandleKeyPressEvent(
    int32_t aDomKeyCode, int32_t aModifiers,
    int32_t aCharCode)
{
  if (aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return false;
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SendKeyPress(
      aDomKeyCode, aModifiers, aCharCode);
  }
  return true;
}

bool EmbedLiteHostedWindow::HandleKeyReleaseEvent(
    int32_t aDomKeyCode, int32_t aModifiers,
    int32_t aCharCode)
{
  if (aDomKeyCode < 0 || aCharCode < 0 || aCharCode > UINT16_MAX) {
    return false;
  }
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->SendKeyRelease(
      aDomKeyCode, aModifiers, aCharCode);
  }
  return true;
}

bool EmbedLiteHostedWindow::ReceiveInputEvent(const MultiTouchInput& aEvent)
{
  if (mChromeSession && mInitialized && !mDestroying) {
    (void) mChromeSession->ReceiveInputEvent(aEvent);
  }
  return true;
}

void EmbedLiteHostedWindow::ChromeInputContextChanged(
    const widget::InputContext& aContext,
    const widget::InputContextAction& aAction)
{
  if (mDestroying || !mOwner) {
    return;
  }

  mOwner->OnInputContextChanged(
    static_cast<int32_t>(aContext.mIMEState.mEnabled),
    static_cast<int32_t>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType, aContext.mHTMLInputMode, aContext.mActionHint,
    static_cast<int32_t>(aAction.mCause),
    static_cast<int32_t>(aAction.mFocusChange));
}

bool EmbedLiteHostedWindow::SetSize(const gfxSize &aSize)
{
  const LayoutDeviceIntRect bounds(
    0, 0, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  const bool sizeChanged = bounds.Width() != mBounds.Width() ||
                           bounds.Height() != mBounds.Height();
  mBounds = bounds;
  LOGT("this:%p width: %f, height: %f as int w: %d h: %d", this, aSize.width, aSize.height, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetSize(aSize.width, aSize.height);
    widget->UpdateBounds(true);
  }
  if (sizeChanged) {
    RefreshScreen();
  }
  return true;
}

bool EmbedLiteHostedWindow::SetContentOrientation(uint32_t aRotation)
{
  LOGT("this:%p", this);
  mRotation = static_cast<mozilla::ScreenRotation>(aRotation);
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetRotation(mRotation);
    widget->UpdateBounds(true);
  }

  RefreshScreen();
  return true;
}

void EmbedLiteHostedWindow::CreateWidget()
{
  LOGT("this:%p", this);
  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }

  if (mDestroyAfterInit && !mWidget) {
    mInitialized = true;
    Destroy();
    return;
  }

  RefreshScreen();

  mWidget = new nsWindow(this);
  GetWidget()->SetRotation(mRotation);

  widget::InitData widgetInit;
  widgetInit.mClipChildren = true;
  widgetInit.mWindowType = widget::WindowType::TopLevel;

  // nsWindow::CreateCompositor() reads back Size
  // when it creates the compositor.
  const nsresult createResult =
    mWidget->Create(nullptr, mBounds, widgetInit);
  if (NS_FAILED(createResult)) {
    mInitialized = true;
    if (mOwner) {
      mOwner->OnInitialized(false);
    }
    Destroy();
    return;
  }
  GetWidget()->SetActive(true);
  GetWidget()->UpdateBounds(true);

  if (!CreateChromeAppWindow()) {
    ChromeSessionInitializationFinished(false);
  }
}

void
EmbedLiteHostedWindow::ChromeSessionInitializationFinished(bool aSuccess)
{
  if (mInitialized || mDestroying) {
    return;
  }

  mInitialized = true;
  if (mDestroyAfterInit) {
    Destroy();
    return;
  }

  if (mOwner) {
    mOwner->OnInitialized(aSuccess);
  }
  if (!aSuccess) {
    Destroy();
  }
}

bool EmbedLiteHostedWindow::CreateChromeAppWindow()
{
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

  uint32_t chromeFlags =
    nsIWebBrowserChrome::CHROME_OPENAS_CHROME |
    nsIWebBrowserChrome::CHROME_WINDOW_RESIZE |
    nsIWebBrowserChrome::CHROME_SCROLLBARS |
    nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
  if (mPrivateBrowsing) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW |
                   nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }
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

void EmbedLiteHostedWindow::DestroyChromeAppWindow()
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
    (void) baseWindow->Destroy();
  }
  mChromeWindow = nullptr;
}

void EmbedLiteHostedWindow::RefreshScreen()
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
      widget::Screen::IsPseudoDisplay::No, widget::Screen::IsHDR::No,
      80.0f, 80.0f);
  screenList.AppendElement(screen.forget());
  widget::ScreenManager::Refresh(std::move(screenList));
}

void EmbedLiteHostedWindow::SetScreenProperties(int aDepth, float aDensity,
                                                float aDpi)
{
  bool screenChanged = false;
  bool resolutionChanged = false;
  float normalizedDensity = aDensity > 0.0f ? aDensity : 1.0f;

  if (aDepth != mDepth) {
    mDepth = aDepth;
    screenChanged = true;
  }

  if (normalizedDensity != mDensity) {
    mDensity = normalizedDensity;
    screenChanged = true;
    resolutionChanged = true;
  }

  if (aDpi != mDpi) {
    mDpi = aDpi;
    screenChanged = true;
    resolutionChanged = true;
  }

  if (!screenChanged || !mWidget) {
    return;
  }

  RefreshScreen();
  if (resolutionChanged) {
    GetWidget()->BackingScaleFactorChanged();
  }
}

} // namespace embedlite
} // namespace mozilla
