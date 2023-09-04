/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_PROCESS_PARENT_H
#define MOZ_APP_EMBED_PROCESS_PARENT_H

#include "mozilla/embedlite/EmbedLiteAppParent.h"

namespace mozilla {
namespace ipc {
class GeckoChildProcessHost;
}
namespace dom {
class Pref;
}
namespace embedlite {

class EmbedLiteApp;
class EmbedLiteAppProcessParent : public EmbedLiteAppParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppProcessParent)
  EmbedLiteAppProcessParent();

public:
  static EmbedLiteAppProcessParent* CreateEmbedLiteAppProcessParent();

  static EmbedLiteAppProcessParent* GetInstance();

  void GetPrefs(nsTArray<mozilla::dom::Pref>* prefs);

protected:
  virtual mozilla::ipc::IPCResult RecvInitialized() override;
  virtual mozilla::ipc::IPCResult RecvReadyToShutdown() override;
  virtual mozilla::ipc::IPCResult RecvCreateWindow(const uint32_t &parentId,
                                                   const uintptr_t &parentBrowsingContext,
                                                   const uint32_t &chromeFlags,
                                                   uint32_t *createdID,
                                                   bool *cancel) override;
  virtual mozilla::ipc::IPCResult RecvObserve(const nsCString &topic,
                                              const nsString &data) override;

  virtual PEmbedLiteViewParent *AllocPEmbedLiteViewParent(const uint32_t &windowId,
                                                          const uint32_t &id,
                                                          const uint32_t &parentId,
                                                          const uintptr_t &parentBrowsingContext,
                                                          const bool &isPrivateWindow,
                                                          const bool &isDesktopMode) override;

  virtual bool DeallocPEmbedLiteViewParent(PEmbedLiteViewParent *aActor) override;
  virtual PEmbedLiteWindowParent *AllocPEmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener) override;
  virtual bool DeallocPEmbedLiteWindowParent(PEmbedLiteWindowParent *aActor) override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult RecvPrefsArrayInitialized(nsTArray<mozilla::dom::Pref> &&prefs) override;

private:
  virtual ~EmbedLiteAppProcessParent();
  void ShutDownProcess(bool aCloseWithError);

  EmbedLiteApp* mApp;
  mozilla::ipc::GeckoChildProcessHost* mSubprocess;
  nsTArray<mozilla::dom::Pref> mPrefs;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppProcessParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_PROCESS_PARENT_H
