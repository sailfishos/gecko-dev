/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_PROCESS_PARENT_H
#define MOZ_VIEW_EMBED_PROCESS_PARENT_H


#include "EmbedLiteViewBaseParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteViewProcessParent : public EmbedLiteViewBaseParent
{
public:
    MOZ_IMPLICIT EmbedLiteViewProcessParent(const uint32_t& id, const uint32_t& parentId, const bool&);
    virtual ~EmbedLiteViewProcessParent();

private:
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewProcessParent);
};
} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PROCESS_PARENT_H

