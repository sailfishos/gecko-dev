/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindowThreadParent.h"

namespace mozilla {
namespace embedlite {

EmbedLiteWindowThreadParent::EmbedLiteWindowThreadParent(const uint16_t& width, const uint16_t& height, const uint32_t& id)
  : EmbedLiteWindowParent(width, height, id)
{
}

EmbedLiteWindowThreadParent::~EmbedLiteWindowThreadParent()
{
}

} // namespace embedlite
} // namespace mozilla

