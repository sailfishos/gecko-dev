/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"

namespace mozilla {
namespace embedlite {

EmbedLiteViewThreadParent::EmbedLiteViewThreadParent(const uint32_t &windowId,
                                                     const uint32_t &id,
                                                     const uint32_t &parentId,
                                                     const uintptr_t &parentBrowsingContext,
                                                     const bool &isPrivateWindow,
                                                     const bool &isDesktopMode,
                                                     const bool &isHidden)
  : EmbedLiteViewParent(windowId, id, parentId, parentBrowsingContext, isPrivateWindow, isDesktopMode, isHidden)
{
}

EmbedLiteViewThreadParent::~EmbedLiteViewThreadParent()
{
}

} // namespace embedlite
} // namespace mozilla
