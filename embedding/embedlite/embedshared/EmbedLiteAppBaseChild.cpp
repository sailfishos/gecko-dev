/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteAppBaseChild.h"

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
#include "mozilla/unused.h"
#include "mozilla/layers/ImageBridgeChild.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteJSON)

static EmbedLiteAppBaseChild* sAppBaseChild = nullptr;

EmbedLiteAppBaseChild*
EmbedLiteAppBaseChild::GetInstance()
{
  return sAppBaseChild;
}

EmbedLiteAppBaseChild::EmbedLiteAppBaseChild(MessageLoop* aParentLoop)
  : mParentLoop(aParentLoop)
{
  LOGT();
  sAppBaseChild = this;
}

NS_IMPL_ISUPPORTS(EmbedLiteAppBaseChild, nsIObserver)

EmbedLiteAppBaseChild::~EmbedLiteAppBaseChild()
{
  LOGT();
  sAppBaseChild = nullptr;
}

NS_IMETHODIMP
EmbedLiteAppBaseChild::Observe(nsISupports* aSubject,
                                 const char* aTopic,
                                 const char16_t* aData)
{
  LOGF("topic:%s", aTopic);
  unused << SendObserve(nsDependentCString(aTopic), aData ? nsDependentString(aData) : nsString());
  return NS_OK;
}

void
EmbedLiteAppBaseChild::Init(MessageChannel* aParentChannel)
{
  LOGT();
  InitWindowWatcher();
  Open(aParentChannel, mParentLoop, ipc::ChildSide);
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
EmbedLiteAppBaseChild::InitAppService()
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
EmbedLiteAppBaseChild::InitWindowWatcher()
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
EmbedLiteAppBaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

bool
EmbedLiteAppBaseChild::DeallocPEmbedLiteViewChild(PEmbedLiteViewChild* actor)
{
  LOGT();
  std::map<uint32_t, EmbedLiteViewBaseChild*>::iterator it;
  for (it = mWeakViewMap.begin(); it != mWeakViewMap.end(); ++it) {
    if (actor == it->second) {
      mWeakViewMap.erase(it);
      break;
    }
  }
  EmbedLiteViewBaseChild* p = static_cast<EmbedLiteViewBaseChild*>(actor);
  p->Release();
  return true;
}

bool EmbedLiteAppBaseChild::CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel)
{
  return SendCreateWindow(parentId, uri, chromeFlags, contextFlags, createdID, cancel);
}

EmbedLiteViewChildIface*
EmbedLiteAppBaseChild::GetViewByID(uint32_t aId)
{
  return aId ? mWeakViewMap[aId] : nullptr;
}

EmbedLiteViewChildIface*
EmbedLiteAppBaseChild::GetViewByChromeParent(nsIWebBrowserChrome* aParent)
{
  LOGT("mWeakViewMap:%i", mWeakViewMap.size());
  std::map<uint32_t, EmbedLiteViewBaseChild*>::iterator it;
  for (it = mWeakViewMap.begin(); it != mWeakViewMap.end(); ++it) {
    if (aParent == it->second->mChrome.get()) {
      return it->second;
    }
  }
  return nullptr;
}

bool
EmbedLiteAppBaseChild::RecvPreDestroy()
{
  LOGT();
  ImageBridgeChild::ShutDown();
  SendReadyToShutdown();
  return true;
}

bool
EmbedLiteAppBaseChild::RecvSetBoolPref(const nsCString& aName, const bool& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    LOGE("Cannot get prefService");
    return false;
  }

  pref->SetBoolPref(aName.get(), aValue);
  return true;
}

bool EmbedLiteAppBaseChild::RecvSetCharPref(const nsCString& aName, const nsCString& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%s", aName.get(), aValue.get());
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    LOGE("Cannot get prefService");
    return false;
  }

  pref->SetCharPref(aName.get(), aValue.get());
  return true;
}

bool EmbedLiteAppBaseChild::RecvSetIntPref(const nsCString& aName, const int& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    LOGE("Cannot get prefService");
    return false;
  }

  pref->SetIntPref(aName.get(), aValue);
  return true;
}

bool
EmbedLiteAppBaseChild::RecvLoadGlobalStyleSheet(const nsCString& uri, const bool& aEnable)
{
  LOGT("uri:%s, enable:%i", uri.get(), aEnable);
  nsCOMPtr<nsIStyleSheetService> styleSheetService =
    do_GetService("@mozilla.org/content/style-sheet-service;1");
  NS_ENSURE_TRUE(styleSheetService, false);
  nsCOMPtr<nsIURI> nsuri;
  NS_NewURI(getter_AddRefs(nsuri), uri);
  NS_ENSURE_TRUE(nsuri, false);
  if (aEnable) {
    styleSheetService->LoadAndRegisterSheet(nsuri, nsIStyleSheetService::AGENT_SHEET);
  } else {
    styleSheetService->UnregisterSheet(nsuri, nsIStyleSheetService::AGENT_SHEET);
  }
  return true;
}

bool EmbedLiteAppBaseChild::RecvLoadComponentManifest(const nsCString& manifest)
{
  nsCOMPtr<nsIFile> f;
  NS_NewNativeLocalFile(manifest, true,
                        getter_AddRefs(f));
  if (f) {
    LOGT("Loading manifest: %s", manifest.get());
    XRE_AddManifestLocation(NS_COMPONENT_LOCATION, f);
  } else {
    NS_ERROR("Failed to create nsIFile for manifest location");
  }
  return true;
}

bool
EmbedLiteAppBaseChild::RecvObserve(const nsCString& topic,
                                     const nsString& data)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(nullptr, topic.get(), data.get());
  }
  return true;
}

bool
EmbedLiteAppBaseChild::RecvAddObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->AddObserver(this,
                                 topic.get(),
                                 false);
  }

  return true;
}

bool
EmbedLiteAppBaseChild::RecvRemoveObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->RemoveObserver(this,
                                    topic.get());
  }

  return true;
}

bool
EmbedLiteAppBaseChild::RecvAddObservers(const InfallibleTArray<nsCString>& observers)
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

  return true;
}

bool
EmbedLiteAppBaseChild::RecvRemoveObservers(const InfallibleTArray<nsCString>& observers)
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    for (unsigned int i = 0; i < observers.Length(); i++) {
      observerService->RemoveObserver(this,
                                      observers[i].get());
    }
  }

  return true;
}

} // namespace embedlite
} // namespace mozilla

