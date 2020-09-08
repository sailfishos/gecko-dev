/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedWidgetFactoryRegister.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "EmbedliteGenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsComponentManagerUtils.h"

#include "nsILoginManager.h"
#include "nsWidgetsCID.h"
#include "nsClipboard.h"

using namespace mozilla::embedlite;

const char* clipBoardCONTRACTID = "@mozilla.org/widget/clipboard;1";

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEmbedClipboard)

EmbedWidgetFactoryRegister::EmbedWidgetFactoryRegister()
{
}

EmbedWidgetFactoryRegister::~EmbedWidgetFactoryRegister()
{
}

NS_IMPL_ISUPPORTS(EmbedWidgetFactoryRegister, nsISupportsWeakReference)

nsresult
EmbedWidgetFactoryRegister::Init()
{
    nsCOMPtr<nsIComponentRegistrar> cr;
    nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCOMPtr<nsIComponentManager> cm;
    rv = NS_GetComponentManager (getter_AddRefs (cm));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCOMPtr<nsIFactory> fp = new mozilla::embedlite::EmbedliteGenericFactory(nsEmbedClipboardConstructor);
    if (!fp) {
        NS_WARNING("Unable to create factory for component");
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIFactory> oldFactory = do_GetClassObject(clipBoardCONTRACTID);
    if (oldFactory) {
        nsCID* cid = nullptr;
        rv = cr->ContractIDToCID(clipBoardCONTRACTID, &cid);
        if (!NS_FAILED(rv)) {
            rv = cr->UnregisterFactory(*cid, oldFactory.get());
            free(cid);
            if (NS_FAILED(rv)) {
                return NS_ERROR_FAILURE;
            }
        }
    }

    nsCID clipboardCID = NS_EMBED_CLIPBOARD_SERVICE_CID;
    rv = cr->RegisterFactory(clipboardCID, "EmbedLite ClipBoard",
                             clipBoardCONTRACTID, fp);

    return NS_OK;
}
