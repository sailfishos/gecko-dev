/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadChild.h"

namespace mozilla {
namespace embedlite {

EmbedLiteViewThreadChild::EmbedLiteViewThreadChild(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
  : EmbedLiteViewBaseChild(id, parentId, isPrivateWindow)
{
  LOGT();
}

EmbedLiteViewThreadChild::~EmbedLiteViewThreadChild()
{
  LOGT();
}

} // namespace embedlite
} // namespace mozilla

