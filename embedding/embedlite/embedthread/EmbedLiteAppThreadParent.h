/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_THREAD_PARENT_H
#define MOZ_APP_EMBED_THREAD_PARENT_H

#include "mozilla/embedlite/EmbedLiteAppParent.h"

namespace mozilla {

namespace layers {
class PCompositorBridgeParent;
} // namespace layers

namespace embedlite {

class EmbedLiteApp;
class EmbedLiteAppThreadParent : public EmbedLiteAppParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppThreadParent)

protected:
  // IPDL implementation
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual PEmbedLiteViewParent* AllocPEmbedLiteViewParent(const uint32_t &windowId,
                                                          const uint32_t &id,
                                                          const uint32_t &parentId,
                                                          const uintptr_t &parentBrowsingContext,
                                                          const bool &isPrivateWindow,
                                                          const bool &isDesktopMode,
                                                          const bool &isHidden) override;
  virtual bool DeallocPEmbedLiteViewParent(PEmbedLiteViewParent*) override;
  virtual PEmbedLiteWindowParent* AllocPEmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener) override;
  virtual bool DeallocPEmbedLiteWindowParent(PEmbedLiteWindowParent*) override;

  // IPDL interface
  virtual mozilla::ipc::IPCResult RecvInitialized() override;
  virtual mozilla::ipc::IPCResult RecvReadyToShutdown() override;
  virtual mozilla::ipc::IPCResult RecvObserve(const nsCString &topic,
                                              const nsString &data) override;
  virtual mozilla::ipc::IPCResult RecvCreateWindow(const uint32_t &parentId,
                                                   const uintptr_t &parentBrowsingContext,
                                                   const uint32_t &chromeFlags,
                                                   const bool &hidden,
                                                   uint32_t *createdID,
                                                   bool *cancel) override;
  virtual mozilla::ipc::IPCResult RecvPrefsArrayInitialized(nsTArray<mozilla::dom::Pref> &&prefs) override;

private:
  virtual ~EmbedLiteAppThreadParent();

  EmbedLiteAppThreadParent();

  EmbedLiteApp* mApp;

private:
  friend class EmbedLiteApp;
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppThreadParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_THREAD_PARENT_H
