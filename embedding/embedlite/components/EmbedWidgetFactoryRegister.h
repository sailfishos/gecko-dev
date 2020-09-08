/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedWidgetFactoryRegister_H_
#define EmbedWidgetFactoryRegister_H_

#include "nsWeakReference.h"

class EmbedWidgetFactoryRegister : public nsSupportsWeakReference
{
public:
    EmbedWidgetFactoryRegister();
    NS_DECL_ISUPPORTS

    nsresult Init();
private:
    virtual ~EmbedWidgetFactoryRegister();
};

#define NS_EMBED_WIDGETFACTORY_CONTRACTID "@mozilla.org/embed-widget-factory-component;1"
#define NS_EMBED_WIDGETFACTORY_SERVICE_CLASSNAME "Embed Widget Factory Component"
#define NS_EMBED_WIDGETFACTORY_SERVICE_CID \
{ 0xa0ee14a6, \
  0x815a, \
  0x11e2, \
  { 0xb0, 0x73, 0x9b, 0xe0, 0x48, 0x40, 0x2e }}

#endif /*EmbedWidgetFactoryRegister_H_*/
