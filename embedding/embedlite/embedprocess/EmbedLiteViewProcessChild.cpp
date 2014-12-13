
#include "FrameMetrics.h"
#include "EmbedLiteViewProcessChild.h"
#include "EmbedLog.h"
#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsIDOMWindowUtils.h"
#include "nsEmbedCID.h"
#include "nsIIOService.h"
#include "nsNetCID.h"

using namespace mozilla::layers;
using namespace mozilla::widget;

static bool sAllowKeyWordURL = false;

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT
EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId)
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteViewProcessChild);
  mInitWindowTask = NewRunnableMethod(this,
                                      &EmbedLiteViewProcessChild::InitGeckoWindow, parentId);
  MessageLoop::current()->PostTask(FROM_HERE, mInitWindowTask);
}

MOZ_IMPLICIT EmbedLiteViewProcessChild::~EmbedLiteViewProcessChild()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteViewProcessChild);
}

void
EmbedLiteViewProcessChild::InitGeckoWindow(const uint32_t& parentId)
{
  LOGT("parentID: %u", parentId);
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;

  // TODO initialize Gecko browser
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

  mWidget = new EmbedLitePuppetWidget(this, mId);

  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.mWindowType = eWindowType_toplevel;
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(mGLViewSize.width, mGLViewSize.height)),
    nullptr,                 // HandleWidgetEvent
    &widgetInit              // nsDeviceContext
  );

  if (!mWidget) {
    NS_ERROR("couldn't create fake widget");
    return;
  }

  rv = baseWindow->InitWindow(0, mWidget, 0, 0, mViewSize.width, mViewSize.height);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDOMWindow> domWindow;

  mChrome = new WebBrowserChrome(this);
  uint32_t aChromeFlags = 0; // View()->GetWindowFlags();

  mWebBrowser->SetContainerWindow(mChrome);

  mChrome->SetChromeFlags(aChromeFlags);
  if (aChromeFlags & (nsIWebBrowserChrome::CHROME_OPENAS_CHROME |
                      nsIWebBrowserChrome::CHROME_OPENAS_DIALOG)) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
    docShellItem->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);
    LOGT("Chrome window created\n");
  }

  if (NS_FAILED(baseWindow->Create())) {
    NS_ERROR("Creation of basewindow failed.\n");
  }

  if (NS_FAILED(mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow)))) {
    NS_ERROR("Failed to get the content DOM window.\n");
  }

 mDOMWindow = do_QueryInterface(domWindow);
  if (!mDOMWindow) {
    NS_ERROR("Got stuck with DOMWindow1!");
  }

  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(mDOMWindow);
  utils->GetOuterWindowID(&mOuterId);

#warning "Return me back"
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
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebBrowser);

  mChrome->SetWebBrowser(mWebBrowser);

  rv = baseWindow->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed.\n");
  }

  mHelper = new TabChildHelper(this);

  unused << SendInitialized();
}

/*-------------------------------Widget-------------------------------------*/

bool
EmbedLiteViewProcessChild::SetInputContext(const int32_t& IMEEnabled,
                                           const int32_t& IMEOpen,
                                           const nsString& type,
                                           const nsString& inputmode,
                                           const nsString& actionHint,
                                           const int32_t& cause,
                                           const int32_t& focusChange)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::GetInputContext(int32_t* IMEEnabled,
                                           int32_t* IMEOpen,
                                           intptr_t* NativeIMEContext)
{
  LOGNI();
  return false;
}

void
EmbedLiteViewProcessChild::ResetInputState()
{
  LOGNI();
  return;
}

gfxSize
EmbedLiteViewProcessChild::GetGLViewSize()
{
  LOGNI();
  return gfxSize(0, 0);
}

/*---------------------------TabChild-----------------------------------------*/

bool
EmbedLiteViewProcessChild::ZoomToRect(const uint32_t& aPresShellId,
                                     const ViewID& aViewId,
                                     const CSSRect& aRect)
{
  LOGNI();
  return SendZoomToRect(aPresShellId, aViewId, aRect);
}

bool
EmbedLiteViewProcessChild::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                                 const ViewID& aViewId,
                                                 const bool& aIsRoot,
                                                 const ZoomConstraints& aConstraints)
{
  LOGNI();
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aIsRoot,
                                   aConstraints);
}

void
EmbedLiteViewProcessChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  LOGNI();
}

bool
EmbedLiteViewProcessChild::HasMessageListener(const nsAString& aMessageName)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGNI("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  return true;
}

bool
EmbedLiteViewProcessChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGNI("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  return true;
}

bool
EmbedLiteViewProcessChild::DoCallRpcMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGNI("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  return true;
}

nsIWebNavigation*
EmbedLiteViewProcessChild::WebNavigation()
{
  return mWebNavigation;
}

nsIWidget*
EmbedLiteViewProcessChild::WebWidget()
{
  return mWidget;
}

/*----------------------------nsIEmbedBrowserChromeListener----------------------------------------*/

/* void onLocationChanged (in string aLocation, in boolean aCanGoBack, in boolean aCanGoForward); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnLocationChanged(const char * aLocation, bool aCanGoBack, bool aCanGoForward)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLoadStarted (in string aLocation); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnLoadStarted(const char * aLocation)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLoadFinished (); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnLoadFinished()
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLoadRedirect (); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnLoadRedirect()
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onWindowCloseRequested (); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnWindowCloseRequested()
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLoadProgress (in int32_t aProgress, in int32_t aCurTotal, in int32_t aMaxTotal); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onSecurityChanged (in string aStatus, in uint32_t aState); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnSecurityChanged(const char * aStatus, uint32_t aState)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onFirstPaint (in int32_t aX, in int32_t aY); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnFirstPaint(int32_t aX, int32_t aY)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onScrolledAreaChanged (in uint32_t aWidth, in uint32_t aHeight); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onScrollChanged (in int32_t offSetX, in int32_t offSetY); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onTitleChanged (in wstring aTitle); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnTitleChanged(const char16_t * aTitle)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onUpdateDisplayPort (); */
NS_IMETHODIMP EmbedLiteViewProcessChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*-----------------------WindowCreator---------------------------------------------*/

NS_IMETHODIMP EmbedLiteViewProcessChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  LOGNI();
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*-------------------------------------- */

uint32_t EmbedLiteViewProcessChild::GetID()
{
  LOGNI();
  return 0;
}

nsresult EmbedLiteViewProcessChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  LOGNI();
  return NS_ERROR_FAILURE;
}

uint64_t EmbedLiteViewProcessChild::GetOuterID()
{
  LOGNI();
  return 0;
}

void EmbedLiteViewProcessChild::AddGeckoContentListener(EmbedLiteContentController* listener)
{
}

void EmbedLiteViewProcessChild::RemoveGeckoContentListener(EmbedLiteContentController* listener)
{
}

bool EmbedLiteViewProcessChild::GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId)
{
  LOGNI();
  return false;
}

bool EmbedLiteViewProcessChild::ContentReceivedTouch(const mozilla::layers::ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault)
{
  LOGNI();
  return false;
}

/*-----------------------PEmbedLiteViewChild---------------------------------------------*/

bool
EmbedLiteViewProcessChild::RecvLoadURL(const nsString& url)
{
  LOGNI("url:%s", NS_ConvertUTF16toUTF8(url).get());
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
EmbedLiteViewProcessChild::RecvGoBack()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvGoForward()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvStopLoad()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvReload(const bool& hardReload)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvLoadFrameScript(const nsString& uri)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvSetViewSize(const gfxSize& aSize)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvSetGLViewSize(const gfxSize& aSize)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvSetIsActive(const bool& aIsActive)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvSetIsFocused(const bool& aIsFocused)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvSuspendTimeouts()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvResumeTimeouts()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvAsyncScrollDOMEvent(
        const gfxRect& contentRect,
        const gfxSize& scrollSize)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvUpdateFrame(const FrameMetrics& frame)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleDoubleTap(const nsIntPoint& point)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleSingleTap(const nsIntPoint& point)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleLongTap(
        const nsIntPoint& point,
        const ScrollableLayerGuid& aGuid,
        const uint64_t& aInputBlockId)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvAcknowledgeScrollUpdate(
        const ViewID& aScrollId,
        const uint32_t& aScrollGeneration)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleTextEvent(
        const nsString& commit,
        const nsString& preEdit)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleKeyPressEvent(
        const int& domKeyCode,
        const int& gmodifiers,
        const int& charCode)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleKeyReleaseEvent(
                                                     const int& domKeyCode,
                                                     const int& gmodifiers,
                                                     const int& charCode)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvMouseEvent(
                                          const nsString& aType,
                                          const float& aX,
                                          const float& aY,
                                          const int32_t& aButton,
                                          const int32_t& aClickCount,
                                          const int32_t& aModifiers,
                                          const bool& aIgnoreRootScrollFrame)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvInputDataTouchEvent(
                                                   const ScrollableLayerGuid& aGuid,
                                                   const MultiTouchInput& event,
                                                   const uint64_t& aInputBlockId)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvInputDataTouchMoveEvent(
                                                       const ScrollableLayerGuid& aGuid,
                                                       const MultiTouchInput& event,
                                                       const uint64_t& aInputBlockId)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvAddMessageListener(const nsCString& name)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvRemoveMessageListener(const nsCString& name)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvAddMessageListeners(const nsTArray<nsString>& messageNames)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvRemoveMessageListeners(const nsTArray<nsString>& messageNames)
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvDestroy()
{
  LOGNI();
  return false;
}

bool
EmbedLiteViewProcessChild::RecvAsyncMessage(const nsString& aMessage,
                                            const nsString& aData)
{
  LOGNI();
  return false;
}

}  // namespace embedlite
}  // namespace mozilla
