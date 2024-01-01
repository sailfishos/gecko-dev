/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_PARENT_H
#define MOZ_APP_EMBED_PARENT_H

#include "mozilla/embedlite/PEmbedLiteAppParent.h"

namespace mozilla {

namespace layers {
class PCompositorBridgeParent;
} // namespace layers

namespace embedlite {

class EmbedLiteApp;
class EmbedLiteAppParent : public PEmbedLiteAppParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppParent)

public:
  EmbedLiteAppParent();

protected:
  virtual ~EmbedLiteAppParent();

  // IPDL implementation
  virtual void ActorDestroy(ActorDestroyReason aWhy)  = 0;
  virtual PEmbedLiteViewParent* AllocPEmbedLiteViewParent(const uint32_t &windowId,
                                                          const uint32_t &id,
                                                          const uint32_t &parentId,
                                                          const uintptr_t &parentBrowsingContext,
                                                          const bool &isPrivateWindow,
                                                          const bool &isDesktopMode,
                                                          const bool &isHidden)  = 0;
  virtual bool DeallocPEmbedLiteViewParent(PEmbedLiteViewParent*)  = 0;
  virtual PEmbedLiteWindowParent* AllocPEmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener)  = 0;
  virtual bool DeallocPEmbedLiteWindowParent(PEmbedLiteWindowParent*)  = 0;

  // IPDL interface
  virtual mozilla::ipc::IPCResult RecvInitialized()  = 0;
  virtual mozilla::ipc::IPCResult RecvReadyToShutdown()  = 0;
  virtual mozilla::ipc::IPCResult RecvObserve(const nsCString &topic,
                                              const nsString &data)  = 0;
  virtual mozilla::ipc::IPCResult RecvCreateWindow(const uint32_t &parentId,
                                                   const uintptr_t &parentBrowsingContext,
                                                   const uint32_t &chromeFlags,
                                                   const bool &hidden,
                                                   uint32_t *createdID,
                                                   bool *cancel)  = 0;
  virtual mozilla::ipc::IPCResult RecvPrefsArrayInitialized(nsTArray<mozilla::dom::Pref> &&prefs)  = 0;

private:
  friend class EmbedLiteApp;
  friend class PEmbedLiteAppParent;
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_PARENT_H
