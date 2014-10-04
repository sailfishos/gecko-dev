/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteAppThreadParent.h"
#include "EmbedLiteApp.h"

#include "mozilla/unused.h"

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
namespace embedlite {

static EmbedLiteAppThreadParent* sAppThreadParent = nullptr;

/* Implementation file */
NS_IMPL_ISUPPORTS(EmbedLiteAppThreadParent, EmbedLiteAppIface)

EmbedLiteAppThreadParent::EmbedLiteAppThreadParent()
  : mApp(EmbedLiteApp::GetInstance())
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppThreadParent);
  NS_ASSERTION(!sAppThreadParent, "Only one instance of EmbedLiteAppThreadParent is allowed");
  sAppThreadParent = this;
}

EmbedLiteAppThreadParent::~EmbedLiteAppThreadParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppThreadParent);
  sAppThreadParent = nullptr;
}

bool
EmbedLiteAppThreadParent::RecvInitialized()
{
  LOGT();
  PR_SetEnv("MOZ_USE_OMTC=1");
  PR_SetEnv("MOZ_LAYERS_PREFER_OFFSCREEN=1");
  mApp->Initialized();
  bool accel = mApp->IsAccelerated();
  mApp->SetBoolPref("dom.netinfo.enabled", false);
  mApp->SetBoolPref("layers.acceleration.disabled", !accel);
  mApp->SetBoolPref("layers.acceleration.force-enabled", accel);
  mApp->SetBoolPref("layers.async-video.enabled", accel);
  mApp->SetBoolPref("layers.offmainthreadcomposition.force-basic", !accel);
  return true;
}

bool
EmbedLiteAppThreadParent::RecvReadyToShutdown()
{
  LOGT();
  mApp->ChildReadyToDestroy();
  return true;
}

bool
EmbedLiteAppThreadParent::RecvCreateWindow(const uint32_t& parentId,
                                           const nsCString& uri,
                                           const uint32_t& chromeFlags,
                                           const uint32_t& contextFlags,
                                           uint32_t* createdID,
                                           bool* cancel)
{
  *createdID = mApp->CreateWindowRequested(chromeFlags, uri.get(), contextFlags, parentId);
  *cancel = !*createdID;
  return true;
}

void
EmbedLiteAppThreadParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

PEmbedLiteViewParent*
EmbedLiteAppThreadParent::AllocPEmbedLiteViewParent(const uint32_t& id, const uint32_t& parentId)
{
  LOGT("id:%u, parent:%u", id, parentId);
  // Return iv view has been destroyed during creation
  if (!EmbedLiteApp::GetInstance()->GetViewByID(id)) {
    NS_ERROR("View was unexpectedly destroyed during startup let's Shutdown");
    EmbedLiteApp::GetInstance()->ViewDestroyed(id);
    return nullptr;
  }
  return new EmbedLiteViewThreadParent(id, parentId);
}

bool
EmbedLiteAppThreadParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* actor)
{
  LOGT();
  delete actor;
  return true;
}

bool
EmbedLiteAppThreadParent::RecvObserve(const nsCString& topic,
                                      const nsString& data)
{
  LOGT("topic:%s", topic.get());
  mApp->GetListener()->OnObserve(topic.get(), data.get());
  return true;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::LoadComponentManifest(const char* aPath)
{
  unused << SendLoadComponentManifest(nsDependentCString(aPath));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::SetBoolPref(const char* aName, bool aValue)
{
  unused << SendSetBoolPref(nsDependentCString(aName), aValue);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::SetCharPref(const char* aName, const char * aValue)
{
  unused << SendSetCharPref(nsDependentCString(aName), nsDependentCString(aValue));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::SetIntPref(const char* aName, int32_t aValue)
{
  unused << SendSetIntPref(nsDependentCString(aName), aValue);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::LoadGlobalStyleSheet(const char* aName, bool aEnable)
{
  unused << SendLoadGlobalStyleSheet(nsDependentCString(aName), aEnable);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::Observe(const char* aName, const char* aMessage)
{
  unused << SendObserve(nsDependentCString(aName), aMessage ? nsDependentString((const char16_t*)aMessage) : nsString());
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::AddObserver(const char* aMessageName)
{
  unused << SendAddObserver(nsDependentCString(aMessageName));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::RemoveObserver(const char* aMessageName)
{
  unused << SendRemoveObserver(nsDependentCString(aMessageName));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::AddObservers(const nsTArray<nsCString>& observersList)
{
  unused << SendAddObservers(observersList);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::RemoveObservers(const nsTArray<nsCString>& observersList)
{
  unused << SendRemoveObservers(observersList);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::CreateView(uint32_t aViewId, uint32_t aParentId)
{
  unused << SendCreateView(aViewId, aParentId);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::PreDestroy()
{
  mApp->GetUILoop()->PostTask(FROM_HERE,
    NewRunnableMethod(this, &EmbedLiteAppThreadParent::SendPreDestroy));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppThreadParent::GetEmbedIPCChannel(mozilla::ipc::MessageChannel* *aChannel)
{
  *aChannel = GetIPCChannel();
  return NS_OK;
}

} // namespace embedlite
} // namespace mozilla

