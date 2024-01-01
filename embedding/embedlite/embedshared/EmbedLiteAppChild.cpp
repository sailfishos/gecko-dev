/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteAppChild.h"

#include "nsIComponentRegistrar.h"             // for nsIComponentRegistrar
#include "nsIComponentManager.h"               // for nsIComponentManager
#include "mozilla/GenericFactory.h"            // for nsIFactory
#include "EmbedLiteAppService.h"               // for EmbedLiteAppServiceConstructor
#include "EmbedLiteJSON.h"                     // for EmbedLiteJSONConstructor
#include "mozilla/ModuleUtils.h"               // for NS_GENERIC_FACTORY_CONSTRUCTOR
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIWindowWatcher.h"
#include "WindowCreator.h"
#include "nsIURI.h"
#include "nsIStyleSheetService.h"
#include "nsNetUtil.h"
#include "gfxPlatform.h"

#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteWindowThreadChild.h"
#include "mozilla/Unused.h"
#include "mozilla/layers/ImageBridgeChild.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteJSON)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteAppService)

static EmbedLiteAppChild* sAppBaseChild = nullptr;

EmbedLiteAppChild*
EmbedLiteAppChild::GetInstance()
{
  return sAppBaseChild;
}

EmbedLiteAppChild::EmbedLiteAppChild(MessageLoop* aParentLoop)
  : mParentLoop(aParentLoop)
{
  LOGT();
  sAppBaseChild = this;
}

NS_IMPL_ISUPPORTS(EmbedLiteAppChild, nsIObserver)

EmbedLiteAppChild::~EmbedLiteAppChild()
{
  LOGT();
  sAppBaseChild = nullptr;
}

NS_IMETHODIMP
EmbedLiteAppChild::Observe(nsISupports* aSubject,
                           const char* aTopic,
                           const char16_t* aData)
{
  LOGF("topic:%s", aTopic);
  Unused << SendObserve(nsDependentCString(aTopic), aData ? nsDependentString(aData) : nsString());
  return NS_OK;
}

void
EmbedLiteAppChild::Init(MessageChannel* aParentChannel)
{
  LOGT();
  InitWindowWatcher();
  Open(aParentChannel, mParentLoop->SerialEventTarget(), ipc::ChildSide);
  RecvSetBoolPref(nsDependentCString("layers.offmainthreadcomposition.enabled"), true);

  mozilla::DebugOnly<nsresult> rv = InitAppService();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  SendInitialized();

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->NotifyObservers(nullptr, "embedliteInitialized", nullptr);
  }
}

nsresult
EmbedLiteAppChild::InitAppService()
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
EmbedLiteAppChild::InitWindowWatcher()
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
EmbedLiteAppChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

bool
EmbedLiteAppChild::DeallocPEmbedLiteViewChild(PEmbedLiteViewChild* actor)
{
  LOGT();
  std::map<uint32_t, EmbedLiteViewChild*>::iterator it;
  for (it = mWeakViewMap.begin(); it != mWeakViewMap.end(); ++it) {
    if (actor == it->second) {
      mWeakViewMap.erase(it);
      break;
    }
  }
  EmbedLiteViewChild* p = static_cast<EmbedLiteViewChild*>(actor);
  p->Release();
  return true;
}

bool
EmbedLiteAppChild::DeallocPEmbedLiteWindowChild(PEmbedLiteWindowChild* aActor)
{
  LOGT();
  std::map<uint32_t, EmbedLiteWindowChild*>::iterator it;
  for (it = mWeakWindowMap.begin(); it != mWeakWindowMap.end(); ++it) {
    if (aActor == it->second) {
      mWeakWindowMap.erase(it);
      break;
    }
  }
  EmbedLiteWindowChild* w = static_cast<EmbedLiteWindowChild*>(aActor);
  w->Release();
  return true;
}

bool EmbedLiteAppChild::CreateWindow(const uint32_t &parentId,
                                     const uintptr_t &parentBrowsingContext,
                                     const uint32_t &chromeFlags,
                                     const bool &hidden,
                                     uint32_t *createdID,
                                     bool *cancel)
{
  return SendCreateWindow(parentId, parentBrowsingContext, chromeFlags, hidden, createdID, cancel);
}

EmbedLiteViewChildIface*
EmbedLiteAppChild::GetViewByID(uint32_t aId) const
{
  const std::map<uint32_t, EmbedLiteViewChild*>::const_iterator it = mWeakViewMap.find(aId);
  return (it != mWeakViewMap.end()) ? it->second : nullptr;
}

EmbedLiteViewChildIface*
EmbedLiteAppChild::GetViewByChromeParent(nsIWebBrowserChrome* aParent) const
{
  LOGT("mWeakViewMap:%i", mWeakViewMap.size());
  for (const std::pair<uint32_t, EmbedLiteViewChild*> &viewPair : mWeakViewMap) {
    if (aParent == viewPair.second->mChrome.get()) {
      return viewPair.second;
    }
  }
  return nullptr;
}

EmbedLiteWindowChild*
EmbedLiteAppChild::GetWindowByID(uint32_t aWindowID)
{
  LOGT("mWeakWindowMap:%i", mWeakWindowMap.size());
  std::map<uint32_t, EmbedLiteWindowChild*>::const_iterator it;
  it = mWeakWindowMap.find(aWindowID);
  if (it != mWeakWindowMap.end()) {
    return it->second;
  }
  return nullptr;
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvPreDestroy()
{
  LOGT();
  ImageBridgeChild::ShutDown();
  SendReadyToShutdown();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvSetBoolPref(const nsCString &aName, const bool &aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_TRUE(pref, IPC_OK());
  pref->SetBoolPref(aName.get(), aValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvSetCharPref(const nsCString &aName, const nsCString &aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%s", aName.get(), aValue.get());
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_TRUE(pref, IPC_OK());

  pref->SetCharPref(aName.get(), aValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvSetIntPref(const nsCString &aName, const int &aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_TRUE(pref, IPC_OK());
  pref->SetIntPref(aName.get(), aValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvLoadGlobalStyleSheet(const nsCString &uri,
                                                                    const bool &aEnable)
{
  LOGT("uri:%s, enable:%i", uri.get(), aEnable);
  nsCOMPtr<nsIStyleSheetService> styleSheetService =
    do_GetService("@mozilla.org/content/style-sheet-service;1");
  NS_ENSURE_TRUE(styleSheetService, IPC_OK());

  nsCOMPtr<nsIURI> nsuri;
  NS_NewURI(getter_AddRefs(nsuri), uri);
  NS_ENSURE_TRUE(nsuri, IPC_OK());

  if (aEnable) {
    styleSheetService->LoadAndRegisterSheet(nsuri, nsIStyleSheetService::AGENT_SHEET);
  } else {
    styleSheetService->UnregisterSheet(nsuri, nsIStyleSheetService::AGENT_SHEET);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvLoadComponentManifest(const nsCString &manifest)
{
  nsCOMPtr<nsIFile> f;
  NS_NewNativeLocalFile(manifest, true,
                        getter_AddRefs(f));
  if (f) {
    LOGT("Loading manifest: %s", manifest.get());
    XRE_AddManifestLocation(NS_APP_LOCATION, f);
  } else {
    NS_ERROR(nsPrintfCString("Failed to create nsIFile for manifest location: %s", manifest.get()).get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvObserve(const nsCString &topic,
                                                       const nsString &data)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(nullptr, topic.get(), data.get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvAddObserver(const nsCString &topic)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->AddObserver(this,
                                 topic.get(),
                                 false);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvRemoveObserver(const nsCString &topic)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->RemoveObserver(this,
                                    topic.get());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvAddObservers(nsTArray<nsCString> &&observers)
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    for (unsigned int i = 0; i < observers.Length(); i++) {
      observerService->AddObserver(this,
                                   observers[i].get(),
                                   false);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppChild::RecvRemoveObservers(nsTArray<nsCString> &&observers)
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    for (unsigned int i = 0; i < observers.Length(); i++) {
      observerService->RemoveObserver(this,
                                      observers[i].get());
    }
  }

  return IPC_OK();
}

} // namespace embedlite
} // namespace mozilla

