/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteWindowThreadParent.h"
#include "EmbedLiteAppThreadParent.h"
#include "EmbedLiteApp.h"
#include "mozilla/layers/PCompositorBridgeParent.h"

#include "mozilla/Unused.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

static EmbedLiteAppThreadParent* sAppThreadParent = nullptr;

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

mozilla::ipc::IPCResult EmbedLiteAppThreadParent::RecvInitialized()
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
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppThreadParent::RecvReadyToShutdown()
{
  LOGT();
  mApp->ChildReadyToDestroy();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppThreadParent::RecvCreateWindow(const uint32_t &parentId,
                                                                   const uintptr_t &parentBrowsingContext,
                                                                   const uint32_t &chromeFlags,
                                                                   const bool &hidden,
                                                                   uint32_t *createdID,
                                                                   bool *cancel)
{
  *createdID = mApp->CreateWindowRequested(chromeFlags, hidden, parentId, parentBrowsingContext);
  *cancel = !*createdID;
  return IPC_OK();
}

void
EmbedLiteAppThreadParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

PEmbedLiteViewParent*
EmbedLiteAppThreadParent::AllocPEmbedLiteViewParent(const uint32_t &windowId,
                                                    const uint32_t &id,
                                                    const uint32_t &parentId,
                                                    const uintptr_t &parentBrowsingContext,
                                                    const bool &isPrivateWindow,
                                                    const bool &isDesktopMode,
                                                    const bool &isHidden)
{
  LOGT("id:%u, parent:%u", id, parentId);
  EmbedLiteViewThreadParent* p = new EmbedLiteViewThreadParent(windowId, id, parentId,
                                                               parentBrowsingContext,
                                                               isPrivateWindow, isDesktopMode,
                                                               isHidden);
  p->AddRef();
  return p;
}

bool
EmbedLiteAppThreadParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* actor)
{
  LOGT();
  EmbedLiteViewThreadParent* p = static_cast<EmbedLiteViewThreadParent *>(actor);
  p->Release();
  return true;
}

PEmbedLiteWindowParent*
EmbedLiteAppThreadParent::AllocPEmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener)
{
  LOGT("id:%u", id);
  EmbedLiteWindowThreadParent *p = new EmbedLiteWindowThreadParent(width, height, id, reinterpret_cast<EmbedLiteWindowListener*>(aListener));
  p->AddRef();
  return p;
}

bool
EmbedLiteAppThreadParent::DeallocPEmbedLiteWindowParent(PEmbedLiteWindowParent* aActor)
{
  LOGT();
  EmbedLiteWindowThreadParent* p = static_cast<EmbedLiteWindowThreadParent *>(aActor);
  p->Release();
  return true;
}

mozilla::ipc::IPCResult EmbedLiteAppThreadParent::RecvObserve(const nsCString &topic,
                                                              const nsString &data)
{
  LOGT("topic:%s", topic.get());
  mApp->GetListener()->OnObserve(topic.get(), data.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteAppThreadParent::RecvPrefsArrayInitialized(
        nsTArray<mozilla::dom::Pref> &&prefs)
{
  LOGT();
  return IPC_OK();
}

} // namespace embedlite
} // namespace mozilla

