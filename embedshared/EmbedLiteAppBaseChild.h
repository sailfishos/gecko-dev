/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_BASE_CHILD_H
#define MOZ_APP_EMBED_BASE_CHILD_H

#include "mozilla/embedlite/PEmbedLiteAppChild.h"  // for PEmbedLiteAppChild
#include "nsIObserver.h"                           // for nsIObserver
#include "EmbedLiteAppChildIface.h"
#include "EmbedLiteViewBaseChild.h"

class EmbedLiteAppService;
class nsIWebBrowserChrome;

namespace mozilla {
namespace embedlite {

class EmbedLiteAppBaseChild : public PEmbedLiteAppChild,
                              public nsIObserver,
                              public EmbedLiteAppChildIface
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  EmbedLiteAppBaseChild(MessageLoop* aParentLoop);
  void Init(MessageChannel* aParentChannel);
  EmbedLiteViewChildIface* GetViewByID(uint32_t aId);
  EmbedLiteViewChildIface* GetViewByChromeParent(nsIWebBrowserChrome* aParent);
  bool CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel);
  static EmbedLiteAppBaseChild* GetInstance();

protected:
  virtual ~EmbedLiteAppBaseChild();

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
  virtual bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*) MOZ_OVERRIDE;

protected:
  MessageLoop* mParentLoop;
  std::map<uint32_t, EmbedLiteViewBaseChild*> mWeakViewMap;

private:
  void InitWindowWatcher();
  nsresult InitAppService();
  friend class EmbedLiteViewBaseChild;
  friend class EmbedLiteViewThreadChild;
  friend class EmbedLiteViewProcessChild;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppBaseChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_BASE_CHILD_H
