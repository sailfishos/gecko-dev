/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_PROCESS_CHILD_H
#define MOZ_VIEW_EMBED_PROCESS_CHILD_H

#include "EmbedLiteViewChild.h"

namespace mozilla {
namespace dom {
class BrowsingContext;
}

namespace embedlite {

class EmbedLiteViewProcessChild : public EmbedLiteViewChild
{
public:
  MOZ_IMPLICIT EmbedLiteViewProcessChild(const uint32_t &windowId,
                                         const uint32_t &id,
                                         const uint32_t &parentId,
                                         mozilla::dom::BrowsingContext *parentBrowsingContext,
                                         const bool &isPrivateWindow,
                                         const bool &isDesktopMode,
                                         const bool &isHidden);

protected:
  virtual void OnGeckoWindowInitialized();

private:
  virtual ~EmbedLiteViewProcessChild();

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewProcessChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PROCESS_CHILD_H
