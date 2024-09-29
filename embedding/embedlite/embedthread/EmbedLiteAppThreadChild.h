/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_THREAD_CHILD_H
#define MOZ_APP_EMBED_THREAD_CHILD_H

#include "EmbedLiteAppChild.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteAppThreadChild : public EmbedLiteAppChild
{
public:
  EmbedLiteAppThreadChild(MessageLoop* aParentLoop);
  static EmbedLiteAppThreadChild* GetInstance();

protected:
  virtual ~EmbedLiteAppThreadChild();

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t &windowId,
                                                        const uint32_t &id,
                                                        const uint32_t &parentId,
                                                        const uintptr_t &parentBrowsingContext,
                                                        const bool &isPrivateWindow,
                                                        const bool &isDesktopMode,
                                                        const bool &isHidden) override;
  virtual PEmbedLiteWindowChild* AllocPEmbedLiteWindowChild(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener) override;
  virtual mozilla::layers::PCompositorBridgeChild* AllocPCompositorBridgeChild(Transport* aTransport, ProcessId aOtherProcess);

private:
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_THREAD_CHILD_H
