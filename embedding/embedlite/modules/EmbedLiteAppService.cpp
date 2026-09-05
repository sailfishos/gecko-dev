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
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "mozilla/ErrorResult.h"
#include "EmbedLiteHostedWindow.h"
#include "EmbedLiteChromeSessionChild.h"
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
    for (uint32_t index = 0; index < aText.Length(); ++index) {
      const char16_t character = aText[index];
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
  if (!browserId) {
    return 0;
  }
  const uint32_t endpoint = mChromeEndpointIds.Allocate(
    browserId, [](uint32_t) { return false; });
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
  return mChromeEndpointIds.Rebind(aEndpointId, browserId);
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

NS_IMETHODIMP
EmbedLiteAppService::GetIDByWindow(mozIDOMWindowProxy* aWindow, uint32_t* aId)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_ENSURE_ARG_POINTER(aId);
  *aId = 0;

  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(outer, NS_ERROR_NOT_AVAILABLE);
  if (dom::BrowsingContext* browsingContext = outer->GetBrowsingContext()) {
    const uint32_t endpoint = mChromeEndpointIds.FindByBrowserId(
      browsingContext->Top()->BrowserId());
    if (endpoint) {
      *aId = endpoint;
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

  if (!EmbedLiteHostedWindow::RequestChromeTabBeforeUnloadPrompt(
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
  return mChromeEndpoints.count(aId) ? NS_ERROR_NOT_IMPLEMENTED
                                     : NS_ERROR_NOT_AVAILABLE;
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

NS_IMETHODIMP
EmbedLiteAppService::ContentReceivedInputBlock(uint32_t aWinId, bool)
{
  return mChromeEndpoints.count(aWinId) ? NS_ERROR_NOT_IMPLEMENTED
                                        : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteAppService::GetBrowserByID(uint32_t, nsIWebBrowser** outWindow)
{
  NS_ENSURE_ARG_POINTER(outWindow);
  *outWindow = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}


NS_IMETHODIMP
EmbedLiteAppService::GetContentWindowByID(
    uint32_t, mozIDOMWindowProxy** contentWindow)
{
  NS_ENSURE_ARG_POINTER(contentWindow);
  *contentWindow = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessageLocal(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  NS_ENSURE_ARG_POINTER(messageName);
  NS_ENSURE_ARG_POINTER(message);
  NS_ENSURE_TRUE(*messageName && NS_strlen(messageName) <= 1024 &&
                 NS_strlen(message) <= 1024 * 1024,
                 NS_ERROR_INVALID_ARG);
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
EmbedLiteAppService::GetAnyEmbedWindow(bool, mozIDOMWindowProxy** embedWindow)
{
  NS_ENSURE_ARG_POINTER(embedWindow);
  *embedWindow = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}
