/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteWindowThreadChild.h"

namespace mozilla {
namespace embedlite {

EmbedLiteWindowThreadChild::EmbedLiteWindowThreadChild(const uint16_t &width, const uint16_t &height, const uint32_t &id, EmbedLiteWindowListener *aListener)
  : EmbedLiteWindowBaseChild(width, height, id, aListener)
{
  LOGT();
}

EmbedLiteWindowThreadChild::~EmbedLiteWindowThreadChild()
{
  LOGT();
}

} // namespace embedlite
} // namespace mozilla

