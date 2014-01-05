/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteModulesService"
#include "EmbedLog.h"

#include "EmbedLiteModulesService.h"

#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsStringGlue.h"
#include "nsIChannel.h"
#include "EmbedLiteAppService.h"
#include "EmbedLiteJSON.h"

#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Preferences.h"

#include "nsIDOMWindowUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestorUtils.h"

// nsCxPusher
#include "nsContentUtils.h"
#include "nsISupportsPrimitives.h"

#include "EmbedLiteViewThreadChild.h"

using namespace mozilla::embedlite;

NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteAppService)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteJSON)

EmbedLiteModulesService::EmbedLiteModulesService()
{
}

EmbedLiteModulesService::~EmbedLiteModulesService()
{
}

NS_IMPL_ISUPPORTS2(EmbedLiteModulesService, nsIObserver, nsSupportsWeakReference)

nsresult
EmbedLiteModulesService::Init()
{
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

NS_IMETHODIMP
EmbedLiteModulesService::Observe(nsISupports* aSubject,
                                 const char* aTopic,
                                 const PRUnichar* aData)
{
  return NS_OK;
}
