/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK2
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_QT
#include "nsQAppInstance.h"
#endif

#include "EmbedLog.h"

#include "EmbedLiteAppProcessChild.h"
#include "mozilla/unused.h"
#include "nsThreadManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIConsoleService.h"
#include "nsDebugImpl.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessChild* EmbedLiteAppProcessChild::sSingleton;

EmbedLiteAppProcessChild::EmbedLiteAppProcessChild()
{
  LOGT();
  nsDebugImpl::SetMultiprocessMode("Child");
}

EmbedLiteAppProcessChild::~EmbedLiteAppProcessChild()
{
  LOGT();
}

bool
EmbedLiteAppProcessChild::Init(MessageLoop* aIOLoop,
                               base::ProcessHandle aParentHandle,
                               IPC::Channel* aChannel)
{
  LOGT();
#ifdef MOZ_WIDGET_GTK
  // sigh
  gtk_init(nullptr, nullptr);
#endif

#ifdef MOZ_WIDGET_QT
  // sigh, seriously
  nsQAppInstance::AddRef();
#endif

#ifdef MOZ_X11
  // Do this after initializing GDK, or GDK will install its own handler.
  XRE_InstallX11ErrorHandler();
#endif

#ifdef MOZ_NUWA_PROCESS
  SetTransport(aChannel);
#endif

  NS_ASSERTION(!sSingleton, "only one ContentChild per child");

  // Once we start sending IPC messages, we need the thread manager to be
  // initialized so we can deal with the responses. Do that here before we
  // try to construct the crash reporter.
  nsresult rv = nsThreadManager::get()->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!Open(aChannel, aParentHandle, aIOLoop)) {
    return false;
  }
  sSingleton = this;

  return true;
}

void
EmbedLiteAppProcessChild::InitXPCOM()
{
  LOGT();
  unused << SendInitialized();
}

void
EmbedLiteAppProcessChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("shutting down early because of crash!");
    QuickExit();
  }

  XRE_ShutdownChildProcess();
}

void
EmbedLiteAppProcessChild::QuickExit()
{
    NS_WARNING("content process _exit()ing");
    _exit(0);
}

PEmbedLiteViewChild*
EmbedLiteAppProcessChild::AllocPEmbedLiteViewChild(const uint32_t& id, const uint32_t& parentId)
{
  LOGT("id:%u, parentId:%u", id, parentId);
  return nullptr;
}

bool
EmbedLiteAppProcessChild::DeallocPEmbedLiteViewChild(PEmbedLiteViewChild* actor)
{
  LOGT();
  return true;
}

bool
EmbedLiteAppProcessChild::RecvPreDestroy()
{
  LOGT();
  SendReadyToShutdown();
  return true;
}

bool
EmbedLiteAppProcessChild::RecvSetBoolPref(const nsCString& aName, const bool& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  return true;
}

bool EmbedLiteAppProcessChild::RecvSetCharPref(const nsCString& aName, const nsCString& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%s", aName.get(), aValue.get());
  return true;
}

bool EmbedLiteAppProcessChild::RecvSetIntPref(const nsCString& aName, const int& aValue)
{
  LOGC("EmbedPrefs", "n:%s, v:%i", aName.get(), aValue);
  return true;
}

bool
EmbedLiteAppProcessChild::RecvLoadGlobalStyleSheet(const nsCString& uri, const bool& aEnable)
{
  return true;
}

bool EmbedLiteAppProcessChild::RecvLoadComponentManifest(const nsCString& manifest)
{
  return true;
}

bool
EmbedLiteAppProcessChild::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvAddObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvRemoveObserver(const nsCString& topic)
{
  LOGT("topic:%s", topic.get());
  return true;
}

bool
EmbedLiteAppProcessChild::RecvAddObservers(const InfallibleTArray<nsCString>& observers)
{
  LOGT();
  return true;
}

bool
EmbedLiteAppProcessChild::RecvRemoveObservers(const InfallibleTArray<nsCString>& observers)
{
  LOGT();
  return true;
}

} // namespace embedlite
} // namespace mozilla

