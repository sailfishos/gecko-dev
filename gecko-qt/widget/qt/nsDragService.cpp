/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDragService.h"

nsDragService::nsDragService() = default;

nsDragService::~nsDragService() = default;

nsresult nsDragService::InvokeDragSessionImpl(
    nsIArray* aTransferableArray,
    const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
    uint32_t aActionType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
