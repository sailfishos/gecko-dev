/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewChild.h"
#include "EmbedLiteAppThreadChild.h"
#include "nsWindow.h"

#include "mozilla/Unused.h"

#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsGlobalWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserSetup.h"
#include "nsRefreshDriver.h"
#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsILoadContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsISelectionController.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
#include "gfxPrefs.h"
#include "mozilla/layers/APZEventState.h"
#include "APZCCallbackHelper.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Document.h"

#include "mozilla/layers/DoubleTapToZoom.h" // for CalculateRectToZoomTo
#include "nsIFrame.h"                       // for nsIFrame
#include "FrameLayerBuilder.h"              // for FrameLayerbuilder

#include <sys/syscall.h>

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool doubleTap;
    bool longTap;
} sHandleDefaultAZPC;
static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool doubleTap;
    bool longTap;
} sPostAZPCAsJson;

static bool sAllowKeyWordURL = false;

static void ReadAZPCPrefs()
{
  // Init default azpc notifications behavior
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.viewport, "embedlite.azpc.handle.viewport", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.singleTap, "embedlite.azpc.handle.singletap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.doubleTap, "embedlite.azpc.handle.doubletap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.longTap, "embedlite.azpc.handle.longtap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.scroll, "embedlite.azpc.handle.scroll", true);

  Preferences::AddBoolVarCache(&sPostAZPCAsJson.viewport, "embedlite.azpc.json.viewport", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.singleTap, "embedlite.azpc.json.singletap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.doubleTap, "embedlite.azpc.json.doubletap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.longTap, "embedlite.azpc.json.longtap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.scroll, "embedlite.azpc.json.scroll", false);

  Preferences::AddBoolVarCache(&sAllowKeyWordURL, "keyword.enabled", sAllowKeyWordURL);
}

EmbedLiteViewChild::EmbedLiteViewChild(const uint32_t& aWindowId, const uint32_t& aId,
                                       const uint32_t& aParentId, const bool& isPrivateWindow)
  : mId(aId)
  , mOuterId(0)
  , mWindow(nullptr)
  , mWebBrowser(nullptr)
  , mChrome(nullptr)
  , mDOMWindow(nullptr)
  , mWebNavigation(nullptr)
  , mWindowObserverRegistered(false)
  , mIsFocused(false)
  , mMargins(0, 0, 0, 0)
  , mIMEComposing(false)
  , mPendingTouchPreventedBlockId(0)
{
  LOGT("id:%u, parentID:%u", aId, aParentId);
  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    ReadAZPCPrefs();
  }

  mWindow = EmbedLiteAppChild::GetInstance()->GetWindowByID(aWindowId);
  MOZ_ASSERT(mWindow != nullptr);

  MessageLoop::current()->PostTask(NewRunnableMethod<const uint32_t, const bool>
                                   ("mozilla::embedlite::EmbedLiteViewChild::InitGeckoWindow",
                                    this,
                                    &EmbedLiteViewChild::InitGeckoWindow,
                                    aParentId,
                                    isPrivateWindow));
}

NS_IMETHODIMP EmbedLiteViewChild::QueryInterface(REFNSIID aIID, void **aInstancePtr)
{
  LOGT("Implement me");
  return NS_OK;
}

EmbedLiteViewChild::~EmbedLiteViewChild()
{
  LOGT();
  if (mWindowObserverRegistered) {
    mWindow->GetWidget()->RemoveObserver(this);
  }
  mWindow = nullptr;
}

void
EmbedLiteViewChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvDestroy()
{
  LOGT("destroy");
  EmbedLiteAppService::AppService()->UnregisterView(mId);
  if (mHelper)
    mHelper->Unload();
  if (mChrome)
    mChrome->RemoveEventHandler();
  if (mWidget) {
    mWidget->Destroy();
  }
  mWebBrowser = nullptr;
  mChrome = nullptr;
  mDOMWindow = nullptr;
  mWebNavigation = nullptr;
  Unused << SendDestroyed();
  PEmbedLiteViewChild::Send__delete__(this);
  return IPC_OK();
}

void
EmbedLiteViewChild::InitGeckoWindow(const uint32_t parentId, const bool isPrivateWindow)
{
  if (!mWindow) {
    LOGT("Init called for already destroyed object");
    return;
  }

  LOGT("parentID: %u", parentId);
  nsresult rv;

  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  gfxPrefs::GetSingleton();
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  mWidget = new EmbedLitePuppetWidget(this);
  LOGT("puppet widget: %p", static_cast<EmbedLitePuppetWidget*>(mWidget.get()));
  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.clipSiblings = true;
  widgetInit.mWindowType = eWindowType_child;
  LayoutDeviceIntRect naturalBounds = mWindow->GetWidget()->GetNaturalBounds();
  rv = mWidget->Create(mWindow->GetWidget(), 0, naturalBounds, &widgetInit);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create widget for EmbedLiteView");
    mWidget = nullptr;
    return;
  }


  nsCOMPtr<nsIWebBrowserSetup> webBrowserSetup = do_QueryInterface(baseWindow);
  if (webBrowserSetup) {
    webBrowserSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH, true);
  }

  LayoutDeviceIntRect bounds = mWindow->GetWidget()->GetBounds();
  rv = baseWindow->InitWindow(0, mWidget, 0, 0, bounds.width, bounds.height);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<mozIDOMWindowProxy> domWindow;

  mChrome = new WebBrowserChrome(this);
  uint32_t chromeFlags = 0; // View()->GetWindowFlags();

  if (isPrivateWindow || Preferences::GetBool("browser.privatebrowsing.autostart")) {
    chromeFlags = nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW|nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  mWebBrowser->SetContainerWindow(mChrome);
  mChrome->SetChromeFlags(chromeFlags);
  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  if (NS_FAILED(baseWindow->Create())) {
    NS_ERROR("Creation of basewindow failed!");
  }

  if (NS_FAILED(mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow)))) {
    NS_ERROR("Failed to get the content DOM window!");
  }

  mDOMWindow = do_QueryInterface(domWindow);
  if (!mDOMWindow) {
    NS_ERROR("Got stuck with root DOMWindow!");
  }

  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(mDOMWindow);
  utils->GetOuterWindowID(&mOuterId);

  EmbedLiteAppService::AppService()->RegisterView(mId);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(mDOMWindow, "embedliteviewcreated", nullptr);
  }

  mWebNavigation = do_QueryInterface(baseWindow);
  if (!mWebNavigation) {
    NS_ERROR("Failed to get the web navigation interface.");
  }

  if (chromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME) {
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(docShell);

    docShell->SetAffectPrivateSessionLifetime(true);
  }

  if (chromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(loadContext);

    loadContext->SetPrivateBrowsing(true);
  }

  mChrome->SetWebBrowser(mWebBrowser);

  rv = baseWindow->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed!");
  }

  mHelper = new BrowserChildHelper(this);
  mChrome->SetBrowserChildHelper(mHelper.get());
  mHelper->ReportSizeUpdate(bounds);

  MOZ_ASSERT(mWindow->GetWidget());
  mWindow->GetWidget()->AddObserver(this);
  mWindowObserverRegistered = true;

  if (mMargins.LeftRight() > 0 || mMargins.TopBottom() > 0) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetMargins(mMargins);
    widget->UpdateSize();
  }

  static bool firstViewCreated = false;
  EmbedLiteWindowChild *windowBase = mWindow;
  if (!firstViewCreated && windowBase && windowBase->GetWidget()) {
    windowBase->GetWidget()->SetActive(true);
    firstViewCreated = true;
  }

  nsWeakPtr weakPtrThis = do_GetWeakReference(mWidget);  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](const ScrollableLayerGuid& aGuid,
                    uint64_t aInputBlockId,
                    bool aPreventDefault)
      {
        if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(weakPtrThis)) {
          EmbedLitePuppetWidget *puppetWidget = static_cast<EmbedLitePuppetWidget*>(widget.get());
          puppetWidget->DoSendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mWidget, Move(callback));
  mSetAllowedTouchBehaviorCallback = [weakPtrThis](uint64_t aInputBlockId,
                                                   const nsTArray<mozilla::layers::TouchBehaviorFlags>& aFlags)
  {
    if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(weakPtrThis)) {
      EmbedLitePuppetWidget *puppetWidget = static_cast<EmbedLitePuppetWidget*>(widget.get());
      puppetWidget->DoSendSetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  };

  OnGeckoWindowInitialized();
  mHelper->OpenIPC();

  Unused << SendInitialized();
}

nsresult
EmbedLiteViewChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outBrowser = mWebBrowser.get());
  return NS_OK;
}

nsresult
EmbedLiteViewChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  if (!mChrome)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outChrome = mChrome.get());
  return NS_OK;
}


/*----------------------------WidgetIface-----------------------------------------------------*/

bool
EmbedLiteViewChild::SetInputContext(const int32_t& IMEEnabled,
                                    const int32_t& IMEOpen,
                                    const nsString& type,
                                    const nsString& inputmode,
                                    const nsString& actionHint,
                                    const int32_t& cause,
                                    const int32_t& focusChange)
{
  return SendSetInputContext(IMEEnabled,
                             IMEOpen,
                             type,
                             inputmode,
                             actionHint,
                             cause,
                             focusChange);
}

bool
EmbedLiteViewChild::GetInputContext(int32_t* IMEEnabled,
                                    int32_t* IMEOpen)
{
  LOGT();
  return SendGetInputContext(IMEEnabled, IMEOpen);
}

void EmbedLiteViewChild::ResetInputState()
{
  LOGT();
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

/*----------------------------WidgetIface-----------------------------------------------------*/

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewChild::ZoomToRect(const uint32_t& aPresShellId,
                               const ViewID& aViewId,
                               const CSSRect& aRect)
{
  return SendZoomToRect(aPresShellId, aViewId, aRect);
}

bool
EmbedLiteViewChild::SetTargetAPZC(uint64_t aInputBlockId, const nsTArray<ScrollableLayerGuid> &aTargets)
{
  LOGT();
  return SendSetTargetAPZC(aInputBlockId, aTargets);
}

bool
EmbedLiteViewChild::GetDPI(float* aDPI)
{
  return SendGetDPI(aDPI);
}

bool
EmbedLiteViewChild::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                          const ViewID& aViewId,
                                          const Maybe<ZoomConstraints> &aConstraints)
{
  LOGT();
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aConstraints);
}

void
EmbedLiteViewChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  LOGT();
}

bool
EmbedLiteViewChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }
  return false;
}

bool
EmbedLiteViewChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
#endif
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, nsTArray<nsString>* aJSONRetVal)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
#endif
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

bool
EmbedLiteViewChild::DoCallRpcMessage(const char16_t* aMessageName, const char16_t* aMessage, nsTArray<nsString>* aJSONRetVal)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
#endif
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    SendRpcMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

nsIWebNavigation*
EmbedLiteViewChild::WebNavigation()
{
  return mWebNavigation;
}

nsIWidget*
EmbedLiteViewChild::WebWidget()
{
  return mWidget;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvLoadURL(const nsString &url)
{
  LOGT("url:%s", NS_ConvertUTF16toUTF8(url).get());
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  uint32_t flags = 0;
  if (sAllowKeyWordURL) {
    flags |= nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    flags |= nsIWebNavigation::LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
  }
  flags |= nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
  mWebNavigation->LoadURI(url.get(),
                          flags,
                          nullptr, nullptr,
                          nullptr, nsContentUtils::GetSystemPrincipal());

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvGoBack()
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->GoBack();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvGoForward()
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->GoForward();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvStopLoad()
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  mWebNavigation->Stop(nsIWebNavigation::STOP_NETWORK);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvReload(const bool& aHardReload)
{
  NS_ENSURE_TRUE(mWebNavigation, IPC_OK());

  uint32_t reloadFlags = aHardReload ?
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE :
                         nsIWebNavigation::LOAD_FLAGS_NONE;

  mWebNavigation->Reload(reloadFlags);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScrollTo(const int &x, const int &y)
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *window = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());
  window->ScrollTo(x, y);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScrollBy(const int &x, const int &y)
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsGlobalWindowInner *window = nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());
  window->ScrollBy(x, y);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetIsActive(const bool &aIsActive)
{
  NS_ENSURE_TRUE(mWebBrowser && mDOMWindow, IPC_OK());

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, IPC_OK());

  if (aIsActive) {
    fm->WindowRaised(mDOMWindow);
    LOGT("Activate browser");
  } else {
    fm->WindowLowered(mDOMWindow);
    LOGT("Deactivate browser");
  }

  EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
  if (widget) {
    widget->SetActive(aIsActive);
  }

  // Update state via WebBrowser -> DocShell -> PresShell
  mWebBrowser->SetIsActive(aIsActive);

  mWidget->Show(aIsActive);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetVisibility(aIsActive);

  if (aIsActive) {
    RecvScheduleUpdate();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetIsFocused(const bool &aIsFocused)
{
  LOGT("child focus: %d thread %ld", aIsFocused, syscall(SYS_gettid));
  NS_ENSURE_TRUE(mWebBrowser && mDOMWindow && (mIsFocused != aIsFocused), IPC_OK());

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, IPC_OK());

  nsIWidgetListener* listener = mWidget->GetWidgetListener();
  if (listener) {
    if (aIsFocused) {
      LOGT("Activate browser focus");
      listener->WindowActivated();
    }
    else {
      listener->WindowDeactivated();
    }
  }
  if (!aIsFocused) {
    fm->ClearFocus(mDOMWindow);
    LOGT("Clear browser focus");
  }

  mIsFocused = aIsFocused;

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetThrottlePainting(const bool &aThrottle)
{
  LOGT("aThrottle:%d", aThrottle);
  mHelper->GetPresContext()->RefreshDriver()->SetThrottled(aThrottle);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSetMargins(const int& aTop, const int& aRight,
                                                           const int& aBottom, const int& aLeft)
{
  mMargins = LayoutDeviceIntMargin(aTop, aRight, aBottom, aLeft);
  if (mWidget) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetMargins(mMargins);
    widget->UpdateSize();

    // Report update for the tab child helper. This triggers update for the viewport.
    LayoutDeviceIntRect bounds = mWindow->GetWidget()->GetBounds();
    bounds.Deflate(mMargins);
    mHelper->ReportSizeUpdate(bounds);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvScheduleUpdate()
{
  // Same that there is in nsPresShell.cpp:10670
  nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
  if (ps && mWidget->IsVisible()) {
    if (nsIFrame* root = ps->GetRootFrame()) {
      FrameLayerBuilder::InvalidateAllLayersForFrame(nsLayoutUtils::GetDisplayRootFrame(root));
      root->SchedulePaint();
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvSuspendTimeouts()
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsCOMPtr<nsPIDOMWindowInner> pwindow(mDOMWindow->GetCurrentInnerWindow());
  if (pwindow && !pwindow->IsFrozen()) {
    pwindow->Freeze();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvResumeTimeouts()
{
  NS_ENSURE_TRUE(mDOMWindow, IPC_OK());

  nsCOMPtr<nsPIDOMWindowInner> pwindow(mDOMWindow->GetCurrentInnerWindow());

  if (pwindow && pwindow->IsFrozen()) {
    pwindow->Thaw();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvLoadFrameScript(const nsString &uri)
{
  if (mHelper) {
    mHelper->DoLoadMessageManagerScript(uri, true);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAsyncMessage(const nsString &aMessage,
                                                             const nsString &aData)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
#endif
  EmbedLiteAppService::AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return IPC_OK();
}


mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAsyncMessage(const nsAString &aMessage,
                                                             const nsAString &aData)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
#endif
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAddMessageListener(const nsCString &name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvRemoveMessageListener(const nsCString &name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvAddMessageListeners(nsTArray<nsString> &&messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvRemoveMessageListeners(nsTArray<nsString> &&messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleScrollEvent(const bool &isRootScrollFrame,
                                                                  const gfxRect &contentRect,
                                                                  const gfxSize &scrollSize)
{
  mozilla::CSSRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  mozilla::CSSSize size(scrollSize.width, scrollSize.height);

  if (sPostAZPCAsJson.scroll) {
    nsString data;
    data.AppendPrintf("{ \"contentRect\" : { \"x\" : ");
    data.AppendFloat(contentRect.x);
    data.AppendPrintf(", \"y\" : ");
    data.AppendFloat(contentRect.y);
    data.AppendPrintf(", \"width\" : ");
    data.AppendFloat(contentRect.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(contentRect.height);
    data.AppendPrintf("}, \"scrollSize\" : { \"width\" : ");
    data.AppendFloat(scrollSize.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(scrollSize.height);
    data.AppendPrintf(" }}");
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("AZPC:ScrollDOMEvent"), data);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvUpdateFrame(const FrameMetrics &aFrameMetrics)
{
  LOGT();
  NS_ENSURE_TRUE(mWebBrowser, IPC_OK());

  RelayFrameMetrics(aFrameMetrics);

  if (sHandleDefaultAZPC.viewport) {
    mHelper->UpdateFrame(aFrameMetrics);
  }

  return IPC_OK();
}

void
EmbedLiteViewChild::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (aPoint) {
    event.mRefPoint.x = aPoint->x;
    event.mRefPoint.y = aPoint->y;
  } else {
    event.mRefPoint.x = 0;
    event.mRefPoint.y = 0;
  }

  event.mTime = PR_Now() / 1000;
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleDoubleTap(const LayoutDevicePoint &aPoint,
                                                                const Modifiers &aModifiers,
                                                                const ScrollableLayerGuid &aGuid)
{
  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  nsIContent* content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
  NS_ENSURE_TRUE(content, IPC_OK());

  nsIPresShell* presShell = APZCCallbackHelper::GetRootContentDocumentPresShellForContent(content);
  NS_ENSURE_TRUE(presShell, IPC_OK());

  RefPtr<Document> document = presShell->GetDocument();
  NS_ENSURE_TRUE(document && !document->Fullscreen(), IPC_OK());

  CSSRect zoomToRect = CalculateRectToZoomTo(document, cssPoint);

  uint32_t presShellId;
  ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      document->GetDocumentElement(), &presShellId, &viewId)) {
    ZoomToRect(presShellId, viewId, zoomToRect);
  }

  if (sPostAZPCAsJson.doubleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleSingleTap(const LayoutDevicePoint &aPoint,
                                                                const Modifiers &aModifiers,
                                                                const ScrollableLayerGuid &aGuid)
{
  if (mIMEComposing) {
    // If we are in the middle of compositing we must finish it, before it is too late.
    // this way we can get focus and actual compositing node working properly in future composition
    nsPoint offset;
    nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
    WidgetCompositionEvent event(true, eCompositionEnd, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
    mIMEComposing = false;
  }

  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:SingleTap"), data);
  }


  if (sHandleDefaultAZPC.singleTap) {
    LayoutDevicePoint pt = cssPoint * mWidget->GetDefaultScale();
    Modifiers m;
    APZCCallbackHelper::FireSingleTapEvent(pt, m, 1, mHelper->WebWidget());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleLongTap(const LayoutDevicePoint &aPoint,
                                                              const ScrollableLayerGuid &aGuid,
                                                              const uint64_t &aInputBlockId)
{
  bool ok = false;
  CSSPoint cssPoint = mHelper->ApplyPointTransform(aPoint, aGuid, &ok);
  NS_ENSURE_TRUE(ok, IPC_OK());

  if (sPostAZPCAsJson.longTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:LongTap"), data);
  }

  bool eventHandled = false;
  if (sHandleDefaultAZPC.longTap) {
    eventHandled = RecvMouseEvent(NS_LITERAL_STRING("contextmenu"), cssPoint.x, cssPoint.y,
                   2 /* Right button */,
                   1 /* Click count */,
                   0 /* Modifiers */,
                   false /* Ignore root scroll frame */);
  }

  SendContentReceivedInputBlock(aGuid, aInputBlockId, eventHandled);

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleTextEvent(const nsString& commit, const nsString& preEdit)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  const InputContext& ctx = mWidget->GetInputContext();

#if EMBEDLITE_LOG_SENSITIVE
  LOGF("ctx.mIMEState.mEnabled:%i, com:%s, pre:%s\n", ctx.mIMEState.mEnabled, NS_ConvertUTF16toUTF8(commit).get(), NS_ConvertUTF16toUTF8(preEdit).get());
#endif
  NS_ENSURE_TRUE(widget && ctx.mIMEState.mEnabled, IPC_OK());

  // probably logic here is over engineered, but clean enough
  bool prevIsComposition = mIMEComposing;
  bool StartComposite = !prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool ChangeComposite = prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool EndComposite = prevIsComposition && preEdit.IsEmpty();
  mIMEComposing = ChangeComposite || StartComposite;
  nsString pushStr = preEdit.IsEmpty() ? commit : preEdit;
  if (!commit.IsEmpty() && !EndComposite) {
    StartComposite = ChangeComposite = EndComposite = true;
  }

  if (StartComposite) {
    WidgetCompositionEvent event(true, eCompositionStart, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  if (StartComposite || ChangeComposite || EndComposite) {

    {
      WidgetCompositionEvent event(true, eCompositionChange, widget);
      InitEvent(event, nullptr);
      event.mData = pushStr;

      if (!EndComposite) {
        // Because we're leaving the composition open, we need to
        // include proper text ranges to make the editor happy.
        TextRange range;
        range.mStartOffset = 0;
        range.mEndOffset = event.mData.Length();
        range.mRangeType = TextRangeType::eRawClause;
        event.mRanges = new TextRangeArray();
        event.mRanges->AppendElement(range);
      }
      APZCCallbackHelper::DispatchWidgetEvent(event);
    }

    nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
    NS_ENSURE_TRUE(ps, IPC_OK());

    nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
    nsIContent* mTarget = DOMFocusManager->GetFocusedContent();

    InternalEditorInputEvent inputEvent(true, eEditorInput, widget);
    inputEvent.mTime = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIMEComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, eCompositionEnd, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  return IPC_OK();
}

nsresult EmbedLiteViewChild::DispatchKeyPressEvent(nsIWidget *widget, const EventMessage &message, const int &domKeyCode, const int &gmodifiers, const int &charCode)
{
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  EventMessage msg = message;
  WidgetKeyboardEvent event(true, msg, widget);
  event.mModifiers = Modifiers(gmodifiers);
  event.mKeyCode = charCode ? 0 : domKeyCode;
  event.mCharCode = charCode;
  event.mLocation = eKeyLocationStandard;
  event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  event.mTime = PR_IntervalNow();
  if (domKeyCode == dom::KeyboardEventBinding::DOM_VK_RETURN) {
    // Needed for multiline editing
    event.mKeyNameIndex = KEY_NAME_INDEX_Enter;
  }
  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleKeyPressEvent(const int &domKeyCode,
                                                                    const int &gmodifiers,
                                                                    const int &charCode)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  NS_ENSURE_TRUE(widget, IPC_OK());
  // Initial key down event
  NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyDown, domKeyCode, gmodifiers, 0), IPC_OK());
  if (domKeyCode != dom::KeyboardEventBinding::DOM_VK_SHIFT &&
      domKeyCode != dom::KeyboardEventBinding::DOM_VK_META &&
      domKeyCode != dom::KeyboardEventBinding::DOM_VK_CONTROL &&
      domKeyCode != dom::KeyboardEventBinding::DOM_VK_ALT) {
          // Key press event
          NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyPress, domKeyCode, gmodifiers, charCode), IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvHandleKeyReleaseEvent(const int &domKeyCode,
                                                                      const int &gmodifiers,
                                                                      const int &charCode)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  NS_ENSURE_TRUE(widget, IPC_OK());
  // Key up event
  NS_ENSURE_SUCCESS(DispatchKeyPressEvent(widget, eKeyUp, domKeyCode, gmodifiers, 0), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvMouseEvent(const nsString &aType,
                                                           const float &aX,
                                                           const float &aY,
                                                           const int32_t &aButton,
                                                           const int32_t &aClickCount,
                                                           const int32_t &aModifiers,
                                                           const bool &aIgnoreRootScrollFrame)
{
  NS_ENSURE_TRUE(mWebBrowser, IPC_OK());

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, IPC_OK());

  bool ignored = false;
  uint8_t argc = 6;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame,
                        0.0, nsIDOMMouseEvent::MOZ_SOURCE_TOUCH,
                        false, false, 0, 0, argc, &ignored);

  return IPC_OK();
}

bool
EmbedLiteViewChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return DoSendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewChild::DoSendContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid &aGuid, uint64_t aInputBlockId, bool aPreventDefault)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return SendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewChild::DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  return SendSetAllowedTouchBehavior(aInputBlockId, aFlags);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid,
                                                                    const mozilla::MultiTouchInput& aInput,
                                                                    const uint64_t& aInputBlockId,
                                                                    const nsEventStatus& aApzResponse)
{
  LOGT("thread: %ld", syscall(SYS_gettid));
  MOZ_ASSERT(NS_IsMainThread());

  WidgetTouchEvent localEvent;

  if (!mHelper->ConvertMutiTouchInputToEvent(aInput, localEvent)) {
    return IPC_OK();
  }

  UserActivity();
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid, mWidget->GetDefaultScale());

  if (localEvent.mMessage == eTouchStart) {
    // CSS touch actions do not work yet. Thus, explicitly disabling in the code.
#if 0
    if (gfxPrefs::TouchActionEnabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(mWidget,
          localEvent, aInputBlockId, mSetAllowedTouchBehaviorCallback);
    }
#endif
    nsCOMPtr<Document> document = mHelper->GetTopLevelDocument();
    APZCCallbackHelper::SendSetTargetAPZCNotification(mWidget, document,
        localEvent, aGuid, aInputBlockId);
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  // We shouldn't have any e10s platforms that have touch events enabled
  // without APZ.
  MOZ_ASSERT(mWidget->AsyncPanZoomEnabled());

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
      aApzResponse, status);
  return IPC_OK();
}



mozilla::ipc::IPCResult EmbedLiteViewChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid,
                                                                        const mozilla::MultiTouchInput& aData,
                                                                        const uint64_t& aInputBlockId,
                                                                        const nsEventStatus& aApzResponse)
{
  LOGT();
  return RecvInputDataTouchEvent(aGuid, aData, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvNotifyAPZStateChange(const ViewID &aViewId, const APZStateChange &aChange, const int &aArg)
{
  LOGT("thread: %ld", syscall(SYS_gettid));
  mAPZEventState->ProcessAPZStateChange(aViewId, aChange, aArg);
  if (aChange == APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewChild::RecvNotifyFlushComplete()
{
  nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
  APZCCallbackHelper::NotifyFlushComplete(ps);
  return IPC_OK();
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward, bool aIsSameDocument)
{
  Unused << aIsSameDocument;
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnWindowCloseRequested()
{
  if (SendOnWindowCloseRequested()) {
    // The event listeners can indirectly hold a reference to nsWebBrowser.
    // Since the listener removal process is not synchronous we have to make
    // sure that we start the process before ::RecvDestroy is called. Otherwise
    // NULLing mWebBrowser in the function won't actually remove it and the current
    // implementation relies on that. The nsWebBrowser object needs to be removed in
    // order for all EmbedLitePuppetWidgets we created to also be released. The puppet
    // widget implementation holds a pointer to EmbedLiteViewThreadParent in mEmbed
    // variable and can use it during shutdown process. This means puppet widgets
    // have to be removed before the view.
    if (mChrome)
      mChrome->RemoveEventHandler();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnFirstPaint(int32_t aX, int32_t aY)
{
  if (mDOMWindow) {
    nsCOMPtr<nsIDocShell> docShell = mDOMWindow->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        nscolor bgcolor = presShell->GetCanvasBackground();
        Unused << SendSetBackgroundColor(bgcolor);
      }
    }
  }

  return SendOnFirstPaint(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

bool
EmbedLiteViewChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::FrameMetrics::ViewID *aViewIdOut)
{
  nsCOMPtr<Document> doc(mHelper->GetTopLevelDocument());
  return APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

float
EmbedLiteViewChild::GetPresShellResolution() const
{
  nsCOMPtr<Document> document(mHelper->GetTopLevelDocument());
  nsIPresShell* shell = document->GetShell();
  if (!shell) {
    return 1.0f;
  }
  return shell->GetResolution();
}

void
EmbedLiteViewChild::WidgetBoundsChanged(const LayoutDeviceIntRect &aSize)
{
  LOGT("sz[%d,%d]", aSize.width, aSize.height);
  MOZ_ASSERT(mHelper && mWebBrowser);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetPositionAndSize(0, 0, aSize.width, aSize.height, true);

  mHelper->ReportSizeUpdate(aSize);
}

void
EmbedLiteViewChild::UserActivity()
{
  LOGT();
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

} // namespace embedlite
} // namespace mozilla

