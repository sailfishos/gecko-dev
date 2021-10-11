/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteXulAppInfo_H_
#define EmbedLiteXulAppInfo_H_

#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteXulAppInfo : public nsIXULAppInfo
                          , public nsIXULRuntime
{
public:
  EmbedLiteXulAppInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIXULRUNTIME
  NS_DECL_NSIXULAPPINFO
  NS_DECL_NSIPLATFORMINFO

  /**
   * Obtains a pointer that has had AddRef called on it.
   * See embedlite/components/components.conf
   */
  static already_AddRefed<EmbedLiteXulAppInfo> GetSingleton();

protected:
  virtual ~EmbedLiteXulAppInfo();
  static EmbedLiteXulAppInfo* sXulAppInfo;

};


}}

#endif /* EmbedLiteXulAppInfo_H_ */

