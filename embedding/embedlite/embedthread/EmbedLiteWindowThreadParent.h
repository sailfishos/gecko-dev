/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_THREAD_PARENT_H
#define MOZ_WINDOW_EMBED_THREAD_PARENT_H

#include "EmbedLiteWindowParent.h"
#include "mozilla/embedlite/EmbedLiteWindowParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteWindowThreadParent : public EmbedLiteWindowParent
{
public:
  EmbedLiteWindowThreadParent(const uint16_t& width, const uint16_t& height, const uint32_t& id);

protected:
  virtual ~EmbedLiteWindowThreadParent() override;

private:
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowThreadParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_THREAD_PARENT_H

