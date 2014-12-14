/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_PROCESS_CHILD_H
#define MOZ_APP_EMBED_PROCESS_CHILD_H

#include "mozilla/embedlite/PEmbedLiteAppChild.h"  // for PEmbedLiteAppChild
#include "mozilla/embedlite/EmbedLiteAppChildIface.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteViewChildIface;
class EmbedLiteAppProcessChild : public PEmbedLiteAppChild,
                                 public EmbedLiteAppChildIface
{
public:
  EmbedLiteAppProcessChild();
  virtual ~EmbedLiteAppProcessChild();
  nsrefcnt AddRef() { return 1; }
  nsrefcnt Release() { return 1; }

  bool Init(MessageLoop* aIOLoop,
            base::ProcessHandle aParentHandle,
            IPC::Channel* aChannel);
  void InitXPCOM();

  struct AppInfo
  {
    nsCString version;
    nsCString buildID;
    nsCString name;
    nsCString UAName;
    nsCString ID;
    nsCString vendor;
  };

  static EmbedLiteAppProcessChild* GetSingleton() {
    return sSingleton;
  }

  const AppInfo& GetAppInfo() {
    return mAppInfo;
  }

/*--------------------------------*/
  virtual EmbedLiteViewChildIface* GetViewByID(uint32_t aId);
  virtual EmbedLiteViewChildIface* GetViewByChromeParent(nsIWebBrowserChrome* aParent);
  virtual bool CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel);


protected:
  // Embed API ipdl interface
  virtual bool RecvSetBoolPref(const nsCString&, const bool&) MOZ_OVERRIDE;
  virtual bool RecvSetCharPref(const nsCString&, const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvSetIntPref(const nsCString&, const int&) MOZ_OVERRIDE;
  virtual bool RecvLoadGlobalStyleSheet(const nsCString&, const bool&) MOZ_OVERRIDE;
  virtual bool RecvLoadComponentManifest(const nsCString&) MOZ_OVERRIDE;

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool RecvPreDestroy() MOZ_OVERRIDE;
  virtual bool RecvObserve(const nsCString& topic,
                           const nsString& data) MOZ_OVERRIDE;
  virtual bool RecvAddObserver(const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvRemoveObserver(const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvAddObservers(const InfallibleTArray<nsCString>& observers) MOZ_OVERRIDE;
  virtual bool RecvRemoveObservers(const InfallibleTArray<nsCString>& observers) MOZ_OVERRIDE;

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t&, const uint32_t& parentId, const bool& isPrivateWindow) MOZ_OVERRIDE;
  virtual bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*) MOZ_OVERRIDE;

private:
  void QuickExit();
  void InitWindowWatcher();
  nsresult InitAppService();

  AppInfo mAppInfo;
  static EmbedLiteAppProcessChild* sSingleton;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppProcessChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_PROCESS_CHILD_H
