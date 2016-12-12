/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewBaseChild.h"
#include "EmbedLiteAppThreadChild.h"

#include "mozilla/unused.h"

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
#include "nsRefreshDriver.h"
#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsISelectionController.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
#include "gfxPrefs.h"
#include "mozilla/layers/APZEventState.h"
#include "APZCCallbackHelper.h"
#include "mozilla/dom/Element.h"

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

EmbedLiteViewBaseChild::EmbedLiteViewBaseChild(const uint32_t& aWindowId, const uint32_t& aId,
                                               const uint32_t& aParentId, const bool& isPrivateWindow)
  : mId(aId)
  , mOuterId(0)
  , mWindow(nullptr)
  , mWebBrowser(nullptr)
  , mChrome(nullptr)
  , mDOMWindow(nullptr)
  , mWebNavigation(nullptr)
  , mViewResized(false)
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

  mWindow = EmbedLiteAppBaseChild::GetInstance()->GetWindowByID(aWindowId);
  MOZ_ASSERT(mWindow != nullptr);

  mInitWindowTask = NewRunnableMethod(this, &EmbedLiteViewBaseChild::InitGeckoWindow,
                                      aParentId, isPrivateWindow);
  MessageLoop::current()->PostTask(FROM_HERE, mInitWindowTask);
}

EmbedLiteViewBaseChild::~EmbedLiteViewBaseChild()
{
  LOGT();
  NS_ASSERTION(mControllerListeners.IsEmpty(), "Controller listeners list is not empty...");
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
    mInitWindowTask = nullptr;
  }
  if (mWindowObserverRegistered) {
    mWindow->GetWidget()->RemoveObserver(this);
  }
  mWindow = nullptr;
}

void
EmbedLiteViewBaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
  mControllerListeners.Clear();
}

bool EmbedLiteViewBaseChild::RecvDestroy()
{
  LOGT("destroy");
  mControllerListeners.Clear();
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
  return true;
}

void
EmbedLiteViewBaseChild::InitGeckoWindow(const uint32_t& parentId, const bool& isPrivateWindow)
{
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;
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

  LayoutDeviceIntRect bounds;
  mWindow->GetWidget()->GetBounds(bounds);
  rv = baseWindow->InitWindow(0, mWidget, 0, 0, bounds.width, bounds.height);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDOMWindow> domWindow;

  mChrome = new WebBrowserChrome(this);
  uint32_t aChromeFlags = 0; // View()->GetWindowFlags();

  if (isPrivateWindow || Preferences::GetBool("browser.privatebrowsing.autostart")) {
    aChromeFlags = nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW|nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  mWebBrowser->SetContainerWindow(mChrome);

  mChrome->SetChromeFlags(aChromeFlags);
  if (aChromeFlags & (nsIWebBrowserChrome::CHROME_OPENAS_CHROME |
                      nsIWebBrowserChrome::CHROME_OPENAS_DIALOG)) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
    docShellItem->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);
    LOGT("Chrome window created.");
  }

  if (NS_FAILED(baseWindow->Create())) {
    NS_ERROR("Creation of basewindow failed!");
  }

  if (NS_FAILED(mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow)))) {
    NS_ERROR("Failed to get the content DOM window!");
  }

  nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(domWindow);
  if(!pWindow) {
    NS_ERROR("Got stuck with DOMWindow!");
  }

  mDOMWindow = do_QueryInterface(pWindow->GetPrivateRoot());
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

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME) {
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(docShell);

    docShell->SetAffectPrivateSessionLifetime(true);
  }

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(loadContext);

    loadContext->SetPrivateBrowsing(true);
  }

  mChrome->SetWebBrowser(mWebBrowser);

  rv = baseWindow->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed!");
  }

  mHelper = new TabChildHelper(this);
  mChrome->SetTabChildHelper(mHelper.get());
  gfxSize size(bounds.width, bounds.height);
  mHelper->ReportSizeUpdate(size);

  MOZ_ASSERT(mWindow->GetWidget());
  mWindow->GetWidget()->AddObserver(this);
  mWindowObserverRegistered = true;

  if (mMargins != nsIntMargin()) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetMargins(mMargins);
    widget->UpdateSize();
  }

  static bool firstViewCreated = false;
  EmbedLiteWindowBaseChild *windowBase = static_cast<EmbedLiteWindowBaseChild*>(mWindow);
  if (!firstViewCreated && windowBase && windowBase->GetWidget()) {
    windowBase->GetWidget()->SetActive(true);
    firstViewCreated = true;
  }

  OnGeckoWindowInitialized();

  Unused << SendInitialized();
}

nsresult
EmbedLiteViewBaseChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outBrowser = mWebBrowser.get());
  return NS_OK;
}

nsresult
EmbedLiteViewBaseChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  if (!mChrome)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outChrome = mChrome.get());
  return NS_OK;
}


/*----------------------------WidgetIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::SetInputContext(const int32_t& IMEEnabled,
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
EmbedLiteViewBaseChild::GetInputContext(int32_t* IMEEnabled,
                                          int32_t* IMEOpen,
                                          intptr_t* NativeIMEContext)
{
  return SendGetInputContext(IMEEnabled, IMEOpen, NativeIMEContext);
}

void EmbedLiteViewBaseChild::ResetInputState()
{
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

/*----------------------------WidgetIface-----------------------------------------------------*/

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::ZoomToRect(const uint32_t& aPresShellId,
                                   const ViewID& aViewId,
                                   const CSSRect& aRect)
{
  return SendZoomToRect(aPresShellId, aViewId, aRect);
}

bool
EmbedLiteViewBaseChild::GetDPI(float* aDPI)
{
  return SendGetDPI(aDPI);
}

bool
EmbedLiteViewBaseChild::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                              const ViewID& aViewId,
                                              const Maybe<ZoomConstraints> &aConstraints)
{
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aConstraints);
}

void
EmbedLiteViewBaseChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->RequestContentRepaint(aFrameMetrics);
  }
}

bool
EmbedLiteViewBaseChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }
  return false;
}

bool
EmbedLiteViewBaseChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewBaseChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::DoCallRpcMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    SendRpcMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

nsIWebNavigation*
EmbedLiteViewBaseChild::WebNavigation()
{
  return mWebNavigation;
}

nsIWidget*
EmbedLiteViewBaseChild::WebWidget()
{
  return mWidget;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::RecvLoadURL(const nsString& url)
{
  LOGT("url:%s", NS_ConvertUTF16toUTF8(url).get());
  NS_ENSURE_TRUE(mWebNavigation, false);

  nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioService) {
    return true;
  }

  ioService->SetOffline(false);
  uint32_t flags = 0;
  if (sAllowKeyWordURL) {
    flags |= nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }
  mWebNavigation->LoadURI(url.get(),
                          flags,
                          0, 0, 0);

  return true;
}

bool
EmbedLiteViewBaseChild::RecvGoBack()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoBack();
  return true;
}

bool
EmbedLiteViewBaseChild::RecvGoForward()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoForward();
  return true;
}

bool
EmbedLiteViewBaseChild::RecvStopLoad()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->Stop(nsIWebNavigation::STOP_NETWORK);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvReload(const bool& aHardReload)
{
  NS_ENSURE_TRUE(mWebNavigation, false);
  uint32_t reloadFlags = aHardReload ?
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE :
                         nsIWebNavigation::LOAD_FLAGS_NONE;

  mWebNavigation->Reload(reloadFlags);
  return true;
}

bool EmbedLiteViewBaseChild::RecvScrollTo(const int &x, const int &y)
{
  if (!mDOMWindow) {
    return false;
  }

  nsGlobalWindow* window = nsGlobalWindow::Cast(mDOMWindow);
  window->ScrollTo(x, y);
  return true;
}

bool EmbedLiteViewBaseChild::RecvScrollBy(const int &x, const int &y)
{
  if (!mDOMWindow) {
    return false;
  }

  nsGlobalWindow* window = nsGlobalWindow::Cast(mDOMWindow);
  window->ScrollBy(x, y);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetIsActive(const bool& aIsActive)
{
  if (!mWebBrowser || !mDOMWindow) {
    return false;
  }

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, false);
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


  mWebBrowser->SetIsActive(aIsActive);
  // Do same stuff that there is in nsPresShell.cpp:10670

  mWidget->Show(aIsActive);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetVisibility(aIsActive);

  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetIsFocused(const bool& aIsFocused)
{
  if (!mWebBrowser || !mDOMWindow) {
    return false;
  }

  if (mIsFocused == aIsFocused) {
    return true;
  }

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, false);
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

  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetThrottlePainting(const bool& aThrottle)
{
  LOGT("aThrottle:%d", aThrottle);
  mHelper->GetPresContext()->RefreshDriver()->SetThrottled(aThrottle);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetMargins(const int& aTop, const int& aRight,
                                       const int& aBottom, const int& aLeft)
{
  mMargins = nsIntMargin(aTop, aRight, aBottom, aLeft);
  if (mWidget) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetMargins(mMargins);
    widget->UpdateSize();

    // Report update for the tab child helper. This triggers update for the viewport.
    LayoutDeviceIntRect bounds;
    mWindow->GetWidget()->GetBounds(bounds);
    nsIntRect b = bounds.ToUnknownRect();
    b.Deflate(mMargins);
    gfxSize size(b.width, b.height);
    mHelper->ReportSizeUpdate(size);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvSuspendTimeouts()
{
  if (!mDOMWindow) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsPIDOMWindow> pwindow(do_QueryInterface(mDOMWindow, &rv));
  NS_ENSURE_SUCCESS(rv, false);
  if (!pwindow->TimeoutSuspendCount()) {
    pwindow->SuspendTimeouts();
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvResumeTimeouts()
{
  if (!mDOMWindow) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsPIDOMWindow> pwindow(do_QueryInterface(mDOMWindow, &rv));
  NS_ENSURE_SUCCESS(rv, false);

  if (pwindow->TimeoutSuspendCount()) {
    rv = pwindow->ResumeTimeouts();
    NS_ENSURE_SUCCESS(rv, false);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvLoadFrameScript(const nsString& uri)
{
  if (mHelper) {
    return mHelper->DoLoadMessageManagerScript(uri, true);
  }
  return false;
}

bool
EmbedLiteViewBaseChild::RecvAsyncMessage(const nsString& aMessage,
                                           const nsString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  EmbedLiteAppService::AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return true;
}


void
EmbedLiteViewBaseChild::RecvAsyncMessage(const nsAString& aMessage,
                                           const nsAString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
}

bool
EmbedLiteViewBaseChild::RecvAddMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvRemoveMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return true;
}

bool
EmbedLiteViewBaseChild::RecvAddMessageListeners(InfallibleTArray<nsString>&& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvRemoveMessageListeners(InfallibleTArray<nsString>&& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return true;
}

void
EmbedLiteViewBaseChild::AddGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.AppendElement(listener);
}

void
EmbedLiteViewBaseChild::RemoveGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.RemoveElement(listener);
}

bool
EmbedLiteViewBaseChild::RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                                const gfxSize& scrollSize)
{
  mozilla::CSSRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  mozilla::CSSSize size(scrollSize.width, scrollSize.height);
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->SendAsyncScrollDOMEvent(0, rect, size);
  }

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
  return true;
}

bool
EmbedLiteViewBaseChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  if (!mWebBrowser) {
    return true;
  }

//  if (mViewResized &&
//      aFrameMetrics.GetIsRoot() &&
//      mHelper->mLastRootMetrics.GetPresShellId() == aFrameMetrics.GetPresShellId() &&
//      mHelper->HandlePossibleViewportChange(mHelper->mInnerSize)) {
//    mViewResized = false;
//  }

  RelayFrameMetrics(aFrameMetrics);

  if (sHandleDefaultAZPC.viewport) {
    return mHelper->RecvUpdateFrame(aFrameMetrics);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvAcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                                    const uint32_t& aScrollGeneration)
{
  APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  return true;
}

void
EmbedLiteViewBaseChild::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (aPoint) {
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  } else {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }

  event.time = PR_Now() / 1000;
}

bool
EmbedLiteViewBaseChild::RecvHandleDoubleTap(const CSSPoint& aPoint,
                                            const Modifiers &aModifiers,
                                            const ScrollableLayerGuid& aGuid)
{
  CSSPoint cssPoint = APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid);

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleDoubleTap(cssPoint, aModifiers, aGuid);
  }

  if (sPostAZPCAsJson.doubleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleSingleTap(const CSSPoint& aPoint,
                                            const Modifiers &aModifiers,
                                            const ScrollableLayerGuid& aGuid)
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

  CSSPoint cssPoint = APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid);

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleSingleTap(cssPoint, aModifiers, aGuid);
  }

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %f, \"y\" : %f }", cssPoint.x, cssPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:SingleTap"), data);
  }


  if (sHandleDefaultAZPC.singleTap) {
    LayoutDevicePoint pt = cssPoint * mWidget->GetDefaultScale();
    Modifiers m;
    APZCCallbackHelper::FireSingleTapEvent(pt, m, mHelper->WebWidget());
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleLongTap(const CSSPoint& aPoint,
                                          const ScrollableLayerGuid& aGuid,
                                          const uint64_t& aInputBlockId)
{
  CSSPoint cssPoint = APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid);

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleLongTap(cssPoint, 0, aGuid, aInputBlockId);
  }

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

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleTextEvent(const nsString& commit, const nsString& preEdit)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  const InputContext& ctx = mWidget->GetInputContext();

  LOGF("ctx.mIMEState.mEnabled:%i, com:%s, pre:%s\n", ctx.mIMEState.mEnabled, NS_ConvertUTF16toUTF8(commit).get(), NS_ConvertUTF16toUTF8(preEdit).get());
  if (!widget || !ctx.mIMEState.mEnabled) {
    return false;
  }

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
        range.mRangeType = NS_TEXTRANGE_RAWINPUT;
        event.mRanges = new TextRangeArray();
        event.mRanges->AppendElement(range);
      }
      APZCCallbackHelper::DispatchWidgetEvent(event);
    }

    nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
    if (!ps) {
      return false;
    }
    nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
    nsIContent* mTarget = DOMFocusManager->GetFocusedContent();

    InternalEditorInputEvent inputEvent(true, eEditorInput, widget);
    inputEvent.time = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIMEComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, eCompositionEnd, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  // If the key isn't autorepeat, we need to send the initial down event
  utils->SendKeyEvent(NS_LITERAL_STRING("keydown"), domKeyCode, 0, gmodifiers, 0, &handled);
  // Don't pass modifiers as NS_KEY_PRESS events.
  // Instead of selectively excluding some keys from NS_KEY_PRESS events,
  // we instead selectively include (as per MSDN spec
  // ( http://msdn.microsoft.com/en-us/library/system.windows.forms.control.keypress%28VS.71%29.aspx );
  // no official spec covers KeyPress events).
  if (domKeyCode != nsIDOMKeyEvent::DOM_VK_SHIFT &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_META &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_CONTROL &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_ALT)
  {
    utils->SendKeyEvent(NS_LITERAL_STRING("keypress"), domKeyCode, charCode, gmodifiers, 0, &handled);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  utils->SendKeyEvent(NS_LITERAL_STRING("keyup"), domKeyCode, 0, gmodifiers, 0, &handled);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvMouseEvent(const nsString& aType,
                                       const float&    aX,
                                       const float&    aY,
                                       const int32_t&  aButton,
                                       const int32_t&  aClickCount,
                                       const int32_t&  aModifiers,
                                       const bool&     aIgnoreRootScrollFrame)
{
  if (!mWebBrowser) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);

  NS_ENSURE_TRUE(utils, true);
  bool ignored = false;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0, false, 4, &ignored);

  return !ignored;
}

bool EmbedLiteViewBaseChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault)
{
  return SendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewBaseChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  LOGT();
  WidgetTouchEvent localEvent;

  if (!mHelper->ConvertMutiTouchInputToEvent(aData, localEvent)) {
    return true;
  }

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid, mWidget->GetDefaultScale());
  nsEventStatus status =
      APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNavigation);
  nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();
  if (innerWindow /*&& innerWindow->HasTouchEventListeners()*/ ) {
    SendContentReceivedInputBlock(aGuid, mPendingTouchPreventedBlockId,
                                  false /*nsIPresShell::gPreventMouseEvents*/);
  }
  mPendingTouchPreventedBlockId = aInputBlockId;

  static bool sDispatchMouseEvents = false;
  static bool sDispatchMouseEventsCached = false;
  if (!sDispatchMouseEventsCached) {
    sDispatchMouseEventsCached = true;
    Preferences::AddBoolVarCache(&sDispatchMouseEvents,
                                 "embedlite.dispatch_mouse_events", false);
  }
  if (status != nsEventStatus_eConsumeNoDefault && sDispatchMouseEvents) {
    DispatchSynthesizedMouseEvent(localEvent);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  return RecvInputDataTouchEvent(aGuid, aData, aInputBlockId);
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward, bool aIsSameDocument)
{
  Unused << aIsSameDocument;
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnWindowCloseRequested()
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
EmbedLiteViewBaseChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnFirstPaint(int32_t aX, int32_t aY)
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
EmbedLiteViewBaseChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

bool
EmbedLiteViewBaseChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::FrameMetrics::ViewID *aViewIdOut)
{
  nsCOMPtr<nsIDocument> doc(mHelper->GetDocument());
  return APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

float
EmbedLiteViewBaseChild::GetPresShellResolution() const
{
  nsCOMPtr<nsIDocument> document(mHelper->GetDocument());
  nsIPresShell* shell = document->GetShell();
  if (!shell) {
    return 1.0f;
  }
  return shell->GetResolution();
}

void
EmbedLiteViewBaseChild::DispatchSynthesizedMouseEvent(const WidgetTouchEvent& aEvent)
{
  // TODO : how should we handle now global mouse event prevent.
//  if (nsIPresShell::gPreventMouseEvents) {
//    return;
//  }

  if (aEvent.mMessage == eTouchEnd) {
    int64_t time = aEvent.time;
    MOZ_ASSERT(aEvent.touches.Length() == 1);
    LayoutDevicePoint pt = aEvent.touches[0]->mRefPoint;
    Modifiers m;
    APZCCallbackHelper::DispatchSynthesizedMouseEvent(eMouseMove, time, pt, m, aEvent.widget);
    APZCCallbackHelper::DispatchSynthesizedMouseEvent(eMouseDown, time, pt, m, aEvent.widget);
    APZCCallbackHelper::DispatchSynthesizedMouseEvent(eMouseUp, time, pt, m, aEvent.widget);
  }
}

void EmbedLiteViewBaseChild::WidgetBoundsChanged(const nsIntRect& aSize)
{
  LOGT("sz[%d,%d]", aSize.width, aSize.height);
  mViewResized = true;

  MOZ_ASSERT(mHelper && mWebBrowser);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetPositionAndSize(0, 0, aSize.width, aSize.height, true);

  gfxSize size(aSize.width, aSize.height);
  mHelper->ReportSizeUpdate(size);
}

} // namespace embedlite
} // namespace mozilla

