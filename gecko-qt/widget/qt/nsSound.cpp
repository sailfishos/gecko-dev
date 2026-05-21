/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSound.h"

NS_IMPL_ISUPPORTS(nsSound, nsISound)

NS_IMETHODIMP
nsSound::Play(nsIURL* aURL) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsSound::Beep() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsSound::Init() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsSound::PlayEventSound(uint32_t aEventId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
