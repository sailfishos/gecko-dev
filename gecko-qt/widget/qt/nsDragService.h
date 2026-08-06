/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"

class nsDragSession final : public nsBaseDragSession {
 public:
  nsDragSession() = default;

  MOZ_CAN_RUN_SCRIPT nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* aTransferableArray,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

 protected:
  ~nsDragSession() override = default;
};

class nsDragService final : public nsBaseDragService {
 public:
  nsDragService();

 protected:
  ~nsDragService() override;

  already_AddRefed<nsIDragSession> CreateDragSession() override;
};

#endif  // nsDragService_h__
