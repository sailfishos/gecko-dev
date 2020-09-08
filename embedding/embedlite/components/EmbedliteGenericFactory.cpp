/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedliteGenericFactory.h"

namespace mozilla {
namespace embedlite {

NS_IMPL_ISUPPORTS(EmbedliteGenericFactory, nsIFactory)

NS_IMETHODIMP
EmbedliteGenericFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult)
{
  return mCtor(aOuter, aIID, aResult);
}

NS_IMETHODIMP
EmbedliteGenericFactory::LockFactory(bool aLock)
{
  NS_ERROR("Vestigial method, never called!");
  return NS_ERROR_FAILURE;
}

} // namespace embedlite
} // namespace mozilla
