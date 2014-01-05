/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteModulesService_H_
#define EmbedLiteModulesService_H_

#include "nsWeakReference.h"
#include "nsIObserver.h"
#include <map>

class nsIDOMWindow;

namespace mozilla {
namespace embedlite {

class EmbedLiteViewThreadChild;
class EmbedLiteModulesService : public nsIObserver,
                                public nsSupportsWeakReference
{
public:
  EmbedLiteModulesService();
  virtual ~EmbedLiteModulesService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult Init();
};

}
}

#endif /*EmbedLiteModulesService_H_*/
