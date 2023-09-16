/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteAppService.h"

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
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "mozilla/embedlite/EmbedLog.h"
// #include "xpcprivate.h"
#include "nsPIDOMWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/EventTarget.h"

using namespace mozilla;
using namespace mozilla::embedlite;

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

namespace
{
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

NS_IMPL_ISUPPORTS(EmbedLiteAppService, nsIObserver, nsIEmbedAppService)

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
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, );
  mIDMap[view->GetOuterID()] = aId;
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
}

NS_IMETHODIMP
EmbedLiteAppService::GetIDByWindow(mozIDOMWindowProxy* aWindow, uint32_t* aId)
{
  dom::AutoJSAPI jsapiChromeGuard;
  nsCOMPtr<nsIWebNavigation> navNav(do_GetInterface(aWindow));
  nsCOMPtr<nsIDocShellTreeItem> navItem(do_QueryInterface(navNav));
  NS_ENSURE_TRUE(navItem, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  navItem->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<mozIDOMWindowProxy> rootWin(do_GetInterface(rootItem));
  NS_ENSURE_TRUE(rootWin, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowOuter> pwindow(do_QueryInterface(rootWin));
  nsCOMPtr<nsPIDOMWindowOuter> outerWindow = pwindow->GetInProcessTop();
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(navNav);
  uint64_t OuterWindowID = 0;
  docShell->GetOuterWindowID(&OuterWindowID);
  *aId = mIDMap[OuterWindowID];
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->DoSendAsyncMessage(messageName, message);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendSyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message, nsAString& retval)
{
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
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
  nsDependentCString cstrname(name);
  mMessageListeners.GetOrInsertNew(cstrname)->AppendElement(listener);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::RemoveMessageListener(const char* name, nsIEmbedMessageListener* aListener)
{
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
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

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
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->ContentReceivedInputBlock(aPreventDefault, 0);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetBrowserByID(uint32_t aId, nsIWebBrowser * *outWindow)
{
  EmbedLiteViewChildIface* view = sGetViewById(aId);
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
  EmbedLiteViewChildIface* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->RecvAsyncMessage(nsDependentString(messageName), nsDependentString(message));
  return NS_OK;
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
        bool isActive;
        docShell->GetIsActive(&isActive);
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
