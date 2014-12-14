/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK2
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_QT
#include "nsQAppInstance.h"
#endif

#include "EmbedLog.h"

#include "EmbedLiteAppProcessChild.h"
#include "mozilla/unused.h"
#include "nsThreadManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIConsoleService.h"
#include "nsDebugImpl.h"
#include "EmbedLiteViewProcessChild.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "WindowCreator.h"
#include "nsIEmbedAppService.h"
#include "EmbedLiteAppService.h"
#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteJSON.h"
#include "nsIComponentRegistrar.h"             // for nsIComponentRegistrar
#include "nsIComponentManager.h"               // for nsIComponentManager
#include "nsIFactory.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"               // for NS_GENERIC_FACTORY_CONSTRUCTOR

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessChild* EmbedLiteAppProcessChild::sSingleton;

EmbedLiteAppProcessChild::EmbedLiteAppProcessChild()
{
  LOGT();
  nsDebugImpl::SetMultiprocessMode("Child");
}

EmbedLiteAppProcessChild::~EmbedLiteAppProcessChild()
{
  LOGT();
}

bool
EmbedLiteAppProcessChild::Init(MessageLoop* aIOLoop,
                               base::ProcessHandle aParentHandle,
                               IPC::Channel* aChannel)
{
  LOGT();
#ifdef MOZ_WIDGET_GTK
  // sigh
  gtk_init(nullptr, nullptr);
#endif

#ifdef MOZ_WIDGET_QT
  // sigh, seriously
  nsQAppInstance::AddRef();
#endif

#ifdef MOZ_X11
  // Do this after initializing GDK, or GDK will install its own handler.
  XRE_InstallX11ErrorHandler();
#endif

#ifdef MOZ_NUWA_PROCESS
  SetTransport(aChannel);
#endif

  NS_ASSERTION(!sSingleton, "only one ContentChild per child");

  // Once we start sending IPC messages, we need the thread manager to be
  // initialized so we can deal with the responses. Do that here before we
  // try to construct the crash reporter.
  nsresult rv = nsThreadManager::get()->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!Open(aChannel, aParentHandle, aIOLoop)) {
    return false;
  }
  sSingleton = this;

  return true;
}

void
EmbedLiteAppProcessChild::InitXPCOM()
{
  LOGT("Initialize some global XPCOM stuff here");

  InitWindowWatcher();

  RecvSetBoolPref(nsDependentCString("layers.offmainthreadcomposition.enabled"), true);

  mozilla::DebugOnly<nsresult> rv = InitAppService();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  SendInitialized();

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->NotifyObservers(nullptr, "embedliteInitialized", nullptr);
  }

  unused << SendInitialized();
}

nsresult
EmbedLiteAppProcessChild::InitAppService()
{
  LOGT();

  nsCOMPtr<nsIComponentRegistrar> cr;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIComponentManager> cm;
  rv = NS_GetComponentManager (getter_AddRefs (cm));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  {
    nsCOMPtr<nsIFactory> f = new mozilla::GenericFactory(EmbedLiteAppServiceConstructor);
    if (!f) {
      NS_WARNING("Unable to create factory for component");
      return NS_ERROR_FAILURE;
    }

    nsCID appCID = NS_EMBED_LITE_APP_SERVICE_CID;
    rv = cr->RegisterFactory(appCID, NS_EMBED_LITE_APP_SERVICE_CLASSNAME,
                             NS_EMBED_LITE_APP_CONTRACTID, f);
  }

  {
    nsCOMPtr<nsIFactory> f = new mozilla::GenericFactory(EmbedLiteJSONConstructor);
    if (!f) {
      NS_WARNING("Unable to create factory for component");
      return NS_ERROR_FAILURE;
    }

    nsCID appCID = NS_IEMBEDLITEJSON_IID;
    rv = cr->RegisterFactory(appCID, NS_EMBED_LITE_JSON_SERVICE_CLASSNAME,
                             NS_EMBED_LITE_JSON_CONTRACTID, f);
  }

  return NS_OK;
}

void
EmbedLiteAppProcessChild::InitWindowWatcher()
{
  // create an nsWindowCreator and give it to the WindowWatcher service
  nsCOMPtr<nsIWindowCreator> creator(new WindowCreator(this));
  if (!creator) {
    LOGE("Out of memory");
    return;
  }
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (!wwatch) {
    LOGE("Fail to get watcher service");
    return;
  }
  LOGT("Created window watcher!");
  wwatch->SetWindowCreator(creator);
}

void
EmbedLiteAppProcessChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("shutting down early because of crash!");
    QuickExit();
  }

  XRE_ShutdownChildProcess();
}

void
EmbedLiteAppProcessChild::QuickExit()
{
    NS_WARNING("content process _exit()ing");
    _exit(0);
}

PEmbedLiteViewChild*
EmbedLiteAppProcessChild::AllocPEmbedLiteViewChild(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
{
  LOGT("id:%u, parentId:%u", id, parentId);
  EmbedLiteViewProcessChild* view = new EmbedLiteViewProcessChild(id, parentId, isPrivateWindow);
  view->AddRef();
  return view;
}

bool
EmbedLiteAppProcessChild::DeallocPEmbedLiteViewChild(PEmbedLiteViewChild* actor)
{
  LOGT();
  EmbedLiteViewProcessChild* p = static_cast<EmbedLiteViewProcessChild*>(actor);
  p->Release();
  return true;
}

/*---------------------------------*/

EmbedLiteViewChildIface*
EmbedLiteAppProcessChild::GetViewByID(uint32_t aId)
{
  LOGNI();
  return nullptr;
}

EmbedLiteViewChildIface*
EmbedLiteAppProcessChild::GetViewByChromeParent(nsIWebBrowserChrome* aParent)
{
  LOGNI();
  return nullptr;
}

bool EmbedLiteAppProcessChild::CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel)
{
  LOGNI();
  return false;
}

/*---------------------------------*/

bool
EmbedLiteAppProcessChild::RecvPreDestroy()
{
  LOGT();
  SendReadyToShutdown();
  return true;
}

bool
EmbedLiteAppProcessChild::RecvSetBoolPref(const nsCString& aName, const bool& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  return true;
}

bool EmbedLiteAppProcessChild::RecvSetCharPref(const nsCString& aName, const nsCString& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%s", aName.get(), aValue.get());
  return true;
}

bool EmbedLiteAppProcessChild::RecvSetIntPref(const nsCString& aName, const int& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  return true;
}

bool
EmbedLiteAppProcessChild::RecvLoadGlobalStyleSheet(const nsCString& uri, const bool& aEnable)
{
  return true;
}

bool EmbedLiteAppProcessChild::RecvLoadComponentManifest(const nsCString& manifest)
{
  return true;
}

bool
EmbedLiteAppProcessChild::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvAddObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvRemoveObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvAddObservers(const InfallibleTArray<nsCString>& observers)
{
  LOGT();
  return true;
}

bool
EmbedLiteAppProcessChild::RecvRemoveObservers(const InfallibleTArray<nsCString>& observers)
{
  LOGT();
  return true;
}

} // namespace embedlite
} // namespace mozilla

