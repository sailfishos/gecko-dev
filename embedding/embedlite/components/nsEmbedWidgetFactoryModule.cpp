/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsServiceManagerUtils.h"
#include "nsICategoryManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsIAppStartupNotifier.h"
#include "EmbedWidgetFactoryRegister.h"
#include "nsNetCID.h"
#include <iostream>

/* ===== XPCOM registration stuff ======== */

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(EmbedWidgetFactoryRegister, Init)

NS_DEFINE_NAMED_CID(NS_EMBED_WIDGETFACTORY_SERVICE_CID);

static const mozilla::Module::CIDEntry kEMBEDWIDGETFACTORYCIDs[] = {
    { &kNS_EMBED_WIDGETFACTORY_SERVICE_CID, false, NULL, EmbedWidgetFactoryRegisterConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kEMBEDWIDGETFACTORYContracts[] = {
    { NS_EMBED_WIDGETFACTORY_CONTRACTID, &kNS_EMBED_WIDGETFACTORY_SERVICE_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kEMBEDWIDGETFACTORYCategories[] = {
    { APPSTARTUP_CATEGORY, NS_EMBED_WIDGETFACTORY_SERVICE_CLASSNAME, NS_EMBED_WIDGETFACTORY_CONTRACTID },
    { NULL }
};

static nsresult
EmbedWidgetFactory_Initialize()
{
    return NS_OK;
}

static void
EmbedWidgetFactory_Shutdown()
{
}

static const mozilla::Module kEMBEDWIDGETFACTORYModule = {
    mozilla::Module::kVersion,
    kEMBEDWIDGETFACTORYCIDs,
    kEMBEDWIDGETFACTORYContracts,
    kEMBEDWIDGETFACTORYCategories,
    NULL,
    EmbedWidgetFactory_Initialize,
    EmbedWidgetFactory_Shutdown
};

NSMODULE_DEFN(nsEmbedWidgetFactoryModule) = &kEMBEDWIDGETFACTORYModule;
