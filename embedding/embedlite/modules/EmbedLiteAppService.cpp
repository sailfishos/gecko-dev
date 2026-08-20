/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteAppService.h"

#include <cmath>
#include <cstring>

#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsContentUtils.h"

#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "mozilla/ErrorResult.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteWindowChild.h"
#include "EmbedLiteChromeSessionChild.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "mozilla/embedlite/EmbedLog.h"
// #include "xpcprivate.h"
#include "nsPIDOMWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Promise.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::embedlite;

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

namespace
{
  bool ParseCanonicalTabId(const nsAString& aText, uint64_t* aResult)
  {
    MOZ_ASSERT(aResult);
    if (aText.IsEmpty() || aText[0] == '0') {
      return false;
    }
    uint64_t value = 0;
    for (char16_t character : aText) {
      if (character < '0' || character > '9') {
        return false;
      }
      const uint64_t digit = character - '0';
      if (value > (UINT64_MAX - digit) / 10) {
        return false;
      }
      value = value * 10 + digit;
    }
    *aResult = value;
    return true;
  }

  class AsyncArrayRemove : public mozilla::Runnable
  {
  protected:
    nsCString mName;
    RefPtr<EmbedLiteAppService> mService;
    nsCOMPtr<nsIEmbedMessageListener> mListener;
  public:
    explicit AsyncArrayRemove(EmbedLiteAppService* service, const char* name, nsIEmbedMessageListener* aListener)
      : mozilla::Runnable("EmbedLiteAppService::AsyncArrayRemove")
      , mName(name)
      , mService(service)
      , mListener(aListener)
    {
    }

    NS_IMETHOD Run()
    {
        return mService->RemoveMessageListener(mName.get(), mListener);
    }
  };
}

static EmbedLiteViewChildIface* sGetViewById(uint32_t aId);

EmbedLiteAppService::EmbedLiteAppService()
  : mHandlingMessages(false)
{
}

uint32_t EmbedLiteAppService::RegisterChromeTab(
    dom::BrowsingContext* aBrowsingContext,
    EmbedLiteChromeSessionChild* aSession, uint64_t aTabId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aBrowsingContext || !aSession || !aTabId) {
    return 0;
  }
  const uint64_t browserId = aBrowsingContext->Top()->BrowserId();
  if (!browserId || mBrowserIDMap.count(browserId)) {
    return 0;
  }
  const uint32_t endpoint = mChromeEndpointIds.Allocate(
    browserId, [this](uint32_t aCandidate) {
      bool mappedLegacyId = false;
      for (const auto& entry : mIDMap) {
        if (entry.second == aCandidate) {
          mappedLegacyId = true;
          break;
        }
      }
      if (!mappedLegacyId) {
        for (const auto& entry : mBrowserIDMap) {
          if (entry.second == aCandidate) {
            mappedLegacyId = true;
            break;
          }
        }
      }
      return mappedLegacyId || sGetViewById(aCandidate);
    });
  if (!endpoint) {
    return 0;
  }
  const auto inserted = mChromeEndpoints.emplace(
    endpoint, ChromeEndpoint{aSession, aTabId});
  if (!inserted.second) {
    MOZ_ALWAYS_TRUE(mChromeEndpointIds.Remove(endpoint));
    return 0;
  }
  return endpoint;
}

bool EmbedLiteAppService::UpdateChromeTabBrowsingContext(
    uint32_t aEndpointId, dom::BrowsingContext* aBrowsingContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  const auto endpoint = mChromeEndpoints.find(aEndpointId);
  if (endpoint == mChromeEndpoints.end() || !aBrowsingContext) {
    return false;
  }
  const uint64_t browserId = aBrowsingContext->Top()->BrowserId();
  return !mBrowserIDMap.count(browserId) &&
    mChromeEndpointIds.Rebind(aEndpointId, browserId);
}

void EmbedLiteAppService::UnregisterChromeTab(uint32_t aEndpointId)
{
  MOZ_ASSERT(NS_IsMainThread());
  const auto endpoint = mChromeEndpoints.find(aEndpointId);
  if (endpoint == mChromeEndpoints.end()) {
    return;
  }
  MOZ_ALWAYS_TRUE(mChromeEndpointIds.Remove(aEndpointId));
  mChromeEndpoints.erase(endpoint);
}

void EmbedLiteAppService::RegisterChromeWindow(
    uint32_t aWindowId, mozIDOMWindowProxy* aWindow,
    EmbedLiteChromeSessionChild* aSession)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsPIDOMWindowOuter* window = nsPIDOMWindowOuter::From(aWindow);
  if (!aWindowId || !aSession) {
    return;
  }
  if (window) {
    const auto chrome = mChromeWindows.emplace(window->WindowID(), aSession);
    MOZ_RELEASE_ASSERT(chrome.second || chrome.first->second == aSession);
  }
  const auto hosted = mChromeHostedWindows.emplace(aWindowId, aSession);
  MOZ_RELEASE_ASSERT(hosted.second || hosted.first->second == aSession);
}

void EmbedLiteAppService::UnregisterChromeWindow(
    uint32_t aWindowId, mozIDOMWindowProxy* aWindow,
    EmbedLiteChromeSessionChild* aSession)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsPIDOMWindowOuter* window = nsPIDOMWindowOuter::From(aWindow);
  if (window) {
    const auto chrome = mChromeWindows.find(window->WindowID());
    if (chrome != mChromeWindows.end() && chrome->second == aSession) {
      mChromeWindows.erase(chrome);
    }
  }
  const auto hosted = mChromeHostedWindows.find(aWindowId);
  if (hosted != mChromeHostedWindows.end() && hosted->second == aSession) {
    mChromeHostedWindows.erase(hosted);
  }
}

EmbedLiteAppService::~EmbedLiteAppService()
{
}

EmbedLiteAppService*
EmbedLiteAppService::AppService()
{
  nsCOMPtr<nsIEmbedAppService> service =
    do_GetService("@mozilla.org/embedlite-app-service;1");
  return static_cast<EmbedLiteAppService*>(service.get());
}

NS_IMPL_ISUPPORTS(EmbedLiteAppService, nsIObserver, nsIEmbedAppService,
                  nsIEmbedChromeAppService)

NS_IMETHODIMP
EmbedLiteAppService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  return NS_OK;
}

static EmbedLiteViewChildIface* sGetViewById(uint32_t aId)
{
  EmbedLiteAppChild* app = EmbedLiteAppChild::GetInstance();
  NS_ENSURE_TRUE(app, nullptr);
  EmbedLiteViewChildIface* view = app->GetViewByID(aId);
  return view;
}

void EmbedLiteAppService::RegisterView(uint32_t aId)
{
  MOZ_RELEASE_ASSERT(!mChromeEndpointIds.ContainsEndpoint(aId));
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, );
  mIDMap[view->GetOuterID()] = aId;

  nsCOMPtr<nsIWebBrowser> browser;
  nsresult rv = view->GetBrowser(getter_AddRefs(browser));
  NS_ENSURE_SUCCESS(rv, );
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(browser);
  NS_ENSURE_TRUE(docShell, );
  RefPtr<dom::BrowsingContext> browsingContext =
    docShell->GetBrowsingContext();
  NS_ENSURE_TRUE(browsingContext, );
  mBrowserIDMap[browsingContext->Top()->BrowserId()] = aId;
}

void EmbedLiteAppService::UnregisterView(uint32_t aId)
{
  std::map<uint64_t, uint32_t>::iterator it;
  for (it = mIDMap.begin(); it != mIDMap.end(); ++it) {
    if (aId == it->second) {
      mIDMap.erase(it);
      break;
    }
  }

  for (it = mBrowserIDMap.begin(); it != mBrowserIDMap.end(); ++it) {
    if (aId == it->second) {
      mBrowserIDMap.erase(it);
      break;
    }
  }
}

NS_IMETHODIMP
EmbedLiteAppService::GetIDByWindow(mozIDOMWindowProxy* aWindow, uint32_t* aId)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_ENSURE_ARG_POINTER(aId);
  *aId = 0;

  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(aWindow);
  if (outer) {
    if (dom::BrowsingContext* browsingContext = outer->GetBrowsingContext()) {
      const auto endpoint =
        mBrowserIDMap.find(browsingContext->Top()->BrowserId());
      if (endpoint != mBrowserIDMap.end()) {
        *aId = endpoint->second;
        return NS_OK;
      }
      const uint32_t hostedEndpoint =
        mChromeEndpointIds.FindByBrowserId(
          browsingContext->Top()->BrowserId());
      if (hostedEndpoint) {
        *aId = hostedEndpoint;
        return NS_OK;
      }
    }
    const auto chromeWindow = mChromeWindows.find(outer->WindowID());
    if (chromeWindow != mChromeWindows.end()) {
      const uint32_t endpoint = chromeWindow->second->SelectedEndpointId();
      if (endpoint) {
        *aId = endpoint;
        return NS_OK;
      }
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  nsCOMPtr<nsIWebNavigation> navNav(do_GetInterface(aWindow));
  nsCOMPtr<nsIDocShellTreeItem> navItem(do_QueryInterface(navNav));
  NS_ENSURE_TRUE(navItem, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  navItem->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(rootItem);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  uint64_t OuterWindowID = 0;
  docShell->GetOuterWindowID(&OuterWindowID);
  const auto view = mIDMap.find(OuterWindowID);
  if (view != mIDMap.end()) {
    *aId = view->second;
    return NS_OK;
  }
  if (dom::BrowsingContext* browsingContext = docShell->GetBrowsingContext()) {
    const auto endpoint =
      mBrowserIDMap.find(browsingContext->Top()->BrowserId());
    if (endpoint != mBrowserIDMap.end()) {
      *aId = endpoint->second;
      return NS_OK;
    }
    const uint32_t hostedEndpoint =
      mChromeEndpointIds.FindByBrowserId(
        browsingContext->Top()->BrowserId());
    if (hostedEndpoint) {
      *aId = hostedEndpoint;
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteAppService::GetIDByBrowsingContext(
    dom::BrowsingContext* aBrowsingContext, uint32_t* aId)
{
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  NS_ENSURE_ARG_POINTER(aId);
  *aId = 0;

  const auto view =
    mBrowserIDMap.find(aBrowsingContext->Top()->BrowserId());
  if (view != mBrowserIDMap.end()) {
    *aId = view->second;
    return NS_OK;
  }
  const uint32_t endpoint = mChromeEndpointIds.FindByBrowserId(
    aBrowsingContext->Top()->BrowserId());
  if (!endpoint) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aId = endpoint;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetChromeTabBrowsingContext(
    uint32_t aWindowId, const nsAString& aTabId,
    dom::BrowsingContext** aBrowsingContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  *aBrowsingContext = nullptr;
  uint64_t parsedTabId = 0;
  if (!aWindowId || !ParseCanonicalTabId(aTabId, &parsedTabId)) {
    return NS_ERROR_INVALID_ARG;
  }
  const auto hosted = mChromeHostedWindows.find(aWindowId);
  NS_ENSURE_TRUE(hosted != mChromeHostedWindows.end(),
                 NS_ERROR_NOT_AVAILABLE);
  RefPtr<dom::BrowsingContext> browsingContext =
    hosted->second->BrowsingContextForTab(parsedTabId);
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_NOT_AVAILABLE);
  browsingContext.forget(aBrowsingContext);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::AsyncChromeTabBeforeUnloadCheck(
    dom::BrowsingContext* aBrowsingContext,
    const nsAString& aTitle, const nsAString& aText,
    const nsAString& aLeaveLabel, const nsAString& aStayLabel,
    JSContext* aCx, dom::Promise** aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  NS_ENSURE_ARG_POINTER(aResult);

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  ErrorResult error;
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  if (!EmbedLiteWindowChild::RequestChromeTabBeforeUnloadPrompt(
        aBrowsingContext, aTitle, aText, aLeaveLabel, aStayLabel,
        promise)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  }

  promise.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  NS_ENSURE_ARG_POINTER(messageName);
  NS_ENSURE_ARG_POINTER(message);
  NS_ENSURE_TRUE(*messageName && NS_strlen(messageName) <= 1024 &&
                 NS_strlen(message) <= 1024 * 1024,
                 NS_ERROR_INVALID_ARG);
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  if (view) {
    view->DoSendAsyncMessage(messageName, message);
    return NS_OK;
  }
  const auto endpoint = mChromeEndpoints.find(aId);
  NS_ENSURE_TRUE(endpoint != mChromeEndpoints.end(), NS_ERROR_NOT_AVAILABLE);
  return endpoint->second.session->SendContentMessageToEmbedder(
    endpoint->second.tabId, nsDependentString(messageName),
    nsDependentString(message)) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteAppService::SendSyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message, nsAString& retval)
{
  NS_ENSURE_ARG_POINTER(messageName);
  NS_ENSURE_ARG_POINTER(message);
  retval.Truncate();
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  if (!view && mChromeEndpoints.count(aId)) {
    // Hosted logical tabs never nest a synchronous call into Qt. Legacy
    // selection sync messages are converted by browser.js to async delivery.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_ENSURE_TRUE(view, NS_ERROR_NOT_AVAILABLE);
  nsTArray<nsString> retvalArray;
  view->DoSendSyncMessage(messageName, message, &retvalArray);
  if (!retvalArray.IsEmpty()) {
    retval = retvalArray[0];
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::AddMessageListener(const char* name, nsIEmbedMessageListener* listener)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(*name && std::strlen(name) <= 1024,
                 NS_ERROR_INVALID_ARG);
  nsDependentCString cstrname(name);
  mMessageListeners.GetOrInsertNew(cstrname)->AppendElement(listener);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::RemoveMessageListener(const char* name, nsIEmbedMessageListener* aListener)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_TRUE(*name && std::strlen(name) <= 1024,
                 NS_ERROR_INVALID_ARG);
  if (mHandlingMessages) {
    nsCOMPtr<nsIRunnable> event =
      new AsyncArrayRemove(this, name, aListener);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  nsTArray<nsCOMPtr<nsIEmbedMessageListener> >* array;
  nsDependentCString cstrname(name);
  if (!mMessageListeners.Get(cstrname, &array)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsCOMPtr<nsIEmbedMessageListener> listener = array->ElementAt(i);
    if (listener == aListener) {
      array->RemoveElementAt(i);
      if (array->IsEmpty()) {
        mMessageListeners.Remove(cstrname);
      }
      break;
    }
  }

  return NS_OK;
}

void
EmbedLiteAppService::HandleAsyncMessage(const char* aMessage, const nsString& aData)
{
  AutoRestore<bool> setVisited(mHandlingMessages);
  mHandlingMessages = true;

  nsTArray<nsCOMPtr<nsIEmbedMessageListener> >* array;
  if (!mMessageListeners.Get(nsDependentCString(aMessage), &array)) {
    return;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsCOMPtr<nsIEmbedMessageListener>& listener = array->ElementAt(i);
    listener->OnMessageReceived(aMessage, aData.get());
  }
}

NS_IMETHODIMP
EmbedLiteAppService::ZoomToRect(uint32_t aWinId, float aX, float aY, float aWidth, float aHeight)
{
  EmbedLiteViewChildIface* view = sGetViewById(aWinId);
  if (!view) {
    const auto endpoint = mChromeEndpoints.find(aWinId);
    NS_ENSURE_TRUE(endpoint != mChromeEndpoints.end(), NS_ERROR_NOT_AVAILABLE);
    NS_ENSURE_TRUE(std::isfinite(aX) && std::isfinite(aY) &&
                   std::isfinite(aWidth) && aWidth >= 0 &&
                   std::isfinite(aHeight) && aHeight >= 0,
                   NS_ERROR_INVALID_ARG);
    return endpoint->second.session->ZoomToRect(
      endpoint->second.tabId, aX, aY, aWidth, aHeight)
      ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t presShellId;
  mozilla::layers::ScrollableLayerGuid::ViewID viewId;
  if (view->GetScrollIdentifiers(&presShellId, &viewId)) {
    view->ZoomToRect(presShellId, viewId, ZoomTarget{CSSRect(aX, aY, aWidth, aHeight)});
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::ContentReceivedInputBlock(uint32_t aWinId, bool aPreventDefault)
{
  EmbedLiteViewChildIface* view = sGetViewById(aWinId);
  if (!view && mChromeEndpoints.count(aWinId)) {
    // ChromeProcessController/APZEventState owns this acknowledgement for
    // hosted content. A second acknowledgement would corrupt APZ ordering.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->ContentReceivedInputBlock(aPreventDefault, 0);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetBrowserByID(uint32_t aId, nsIWebBrowser * *outWindow)
{
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  if (!view && mChromeEndpoints.count(aId)) {
    // A remote XUL browser is an Element/BrowserParent, not nsIWebBrowser.
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> br;
  rv = view->GetBrowser(getter_AddRefs(br));
  NS_ENSURE_TRUE(br, rv);
  *outWindow = br.forget().take();
  return rv;
}


NS_IMETHODIMP
EmbedLiteAppService::GetContentWindowByID(uint32_t aId, mozIDOMWindowProxy * *contentWindow)
{
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  if (!view && mChromeEndpoints.count(aId)) {
    // There is no parent-process DOMWindowProxy for remote/Fission content.
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> br;
  rv = view->GetBrowser(getter_AddRefs(br));
  NS_ENSURE_TRUE(br, rv);
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  br->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  domWindow.forget(contentWindow);
  return rv;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessageLocal(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  NS_ENSURE_ARG_POINTER(messageName);
  NS_ENSURE_ARG_POINTER(message);
  NS_ENSURE_TRUE(*messageName && NS_strlen(messageName) <= 1024 &&
                 NS_strlen(message) <= 1024 * 1024,
                 NS_ERROR_INVALID_ARG);
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  if (view) {
    view->RecvAsyncMessage(nsDependentString(messageName),
                           nsDependentString(message));
    return NS_OK;
  }
  const auto endpoint = mChromeEndpoints.find(aId);
  NS_ENSURE_TRUE(endpoint != mChromeEndpoints.end(), NS_ERROR_NOT_AVAILABLE);
  return endpoint->second.session->SendContentMessageFromAppService(
    endpoint->second.tabId, nsDependentString(messageName),
    nsDependentString(message)) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteAppService::ChromeEventHandler(mozIDOMWindowProxy *aWindow, EventTarget * *eventHandler)
{
  nsCOMPtr<nsPIDOMWindowOuter> pidomWindow = do_GetInterface(aWindow);
  NS_ENSURE_TRUE(pidomWindow, NS_ERROR_FAILURE);
  RefPtr<EventTarget> target(pidomWindow->GetChromeEventHandler());
  *eventHandler = target.forget().take();
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetAnyEmbedWindow(bool aActive, mozIDOMWindowProxy * *embedWindow)
{
  std::map<uint64_t, uint32_t>::iterator it;
  for (it = mIDMap.begin(); it != mIDMap.end(); ++it) {
    EmbedLiteViewChildIface* view = sGetViewById(it->second);
    if (view) {
      if (!aActive) {
        nsresult rv;
        nsCOMPtr<nsIWebBrowser> br;
        rv = view->GetBrowser(getter_AddRefs(br));
        NS_ENSURE_TRUE(br, rv);
        nsCOMPtr<mozIDOMWindowProxy> domWindow;
        br->GetContentDOMWindow(getter_AddRefs(domWindow));
        if (!domWindow) {
          return NS_ERROR_NOT_AVAILABLE;
        }

        nsCOMPtr<nsPIDOMWindowOuter> piWindow = nsPIDOMWindowOuter::From(domWindow);
        piWindow.forget(embedWindow);
        return NS_OK;
      } else {
        nsresult rv;
        nsCOMPtr<nsIWebBrowser> br;
        rv = view->GetBrowser(getter_AddRefs(br));
        NS_ENSURE_TRUE(br, rv);
        nsCOMPtr<nsIDocShell> docShell = do_GetInterface(br);
        bool isActive = docShell->GetBrowsingContext()->IsActive();
        if (isActive) {
          nsCOMPtr<mozIDOMWindowProxy> domWindow;
          br->GetContentDOMWindow(getter_AddRefs(domWindow));
          if (!domWindow) {
            return NS_ERROR_NOT_AVAILABLE;
          }
          nsCOMPtr<nsPIDOMWindowOuter> piWindow = nsPIDOMWindowOuter::From(domWindow);
          piWindow.forget(embedWindow);
          return NS_OK;
        }
      }
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}
