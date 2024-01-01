/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_CHILD_H
#define MOZ_APP_EMBED_CHILD_H

#include "mozilla/embedlite/PEmbedLiteAppChild.h"  // for PEmbedLiteAppChild
#include "nsIObserver.h"                           // for nsIObserver
#include "EmbedLiteAppChildIface.h"

class EmbedLiteAppService;
class nsIWebBrowserChrome;

namespace mozilla {
namespace embedlite {

class EmbedLiteViewChild;
class EmbedLiteWindowChild;

class EmbedLiteAppChild : public PEmbedLiteAppChild,
                          public nsIObserver,
                          public EmbedLiteAppChildIface
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  EmbedLiteAppChild(MessageLoop* aParentLoop);
  void Init(MessageChannel* aParentChannel);
  EmbedLiteViewChildIface* GetViewByID(uint32_t aId) const override;
  EmbedLiteViewChildIface* GetViewByChromeParent(nsIWebBrowserChrome* aParent) const override;
  EmbedLiteWindowChild* GetWindowByID(uint32_t aWindowID);
  bool CreateWindow(const uint32_t &parentId,
                    const uintptr_t &parentBrowsingContext,
                    const uint32_t &chromeFlags,
                    const bool &hidden,
                    uint32_t *createdID,
                    bool *cancel) override;
  static EmbedLiteAppChild* GetInstance();

protected:
  virtual ~EmbedLiteAppChild();

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t &windowId,
                                                        const uint32_t &id,
                                                        const uint32_t &parentId,
                                                        const uintptr_t &parentBrowsingContext,
                                                        const bool &isPrivateWindow,
                                                        const bool &isDesktopMode,
                                                        const bool &isHidden) = 0;
  virtual PEmbedLiteWindowChild* AllocPEmbedLiteWindowChild(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener) = 0;

protected:
  MessageLoop* mParentLoop;
  std::map<uint32_t, EmbedLiteViewChild*> mWeakViewMap;
  std::map<uint32_t, EmbedLiteWindowChild*> mWeakWindowMap;
  void InitWindowWatcher();
  nsresult InitAppService();

private:
  friend class EmbedLiteViewChild;
  friend class EmbedLiteViewThreadChild;
  friend class EmbedLiteViewProcessChild;
  friend class EmbedLiteAppProcessChild;
  friend class PEmbedLiteAppChild;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppChild);

  // Embed API ipdl interface
  mozilla::ipc::IPCResult RecvSetBoolPref(const nsCString &, const bool &);
  mozilla::ipc::IPCResult RecvSetCharPref(const nsCString &, const nsCString &);
  mozilla::ipc::IPCResult RecvSetIntPref(const nsCString &, const int &);
  mozilla::ipc::IPCResult RecvLoadGlobalStyleSheet(const nsCString &, const bool &);
  mozilla::ipc::IPCResult RecvLoadComponentManifest(const nsCString &);

  mozilla::ipc::IPCResult RecvPreDestroy();
  mozilla::ipc::IPCResult RecvObserve(const nsCString &topic,
                                              const nsString &data);
  mozilla::ipc::IPCResult RecvAddObserver(const nsCString &);
  mozilla::ipc::IPCResult RecvRemoveObserver(const nsCString &);
  mozilla::ipc::IPCResult RecvAddObservers(nsTArray<nsCString> &&observers);
  mozilla::ipc::IPCResult RecvRemoveObservers(nsTArray<nsCString> &&observers);

  bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*);
  bool DeallocPEmbedLiteWindowChild(PEmbedLiteWindowChild*);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_CHILD_H
