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
#include "EmbedLiteViewProcessChild.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "WindowCreator.h"
#include "nsIEmbedAppService.h"
#include "EmbedLiteAppService.h"
#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteJSON.h"
#include "nsIComponentRegistrar.h"             // for nsIComponentRegistrar
#include "nsIComponentManager.h"               // for nsIComponentManager
#include "nsIFactory.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"               // for NS_GENERIC_FACTORY_CONSTRUCTOR
#include "mozilla/layers/PCompositorChild.h"


using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessChild*
EmbedLiteAppProcessChild::GetSingleton()
{
  return static_cast<EmbedLiteAppProcessChild*>(EmbedLiteAppBaseChild::GetInstance());
}

EmbedLiteAppProcessChild::EmbedLiteAppProcessChild()
  : EmbedLiteAppBaseChild(nullptr)
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

  return true;
}

void
EmbedLiteAppProcessChild::InitXPCOM()
{
  LOGT("Initialize some global XPCOM stuff here");

  InitWindowWatcher();

  RecvSetBoolPref(nsDependentCString("layers.offmainthreadcomposition.enabled"), true);

  mozilla::DebugOnly<nsresult> rv = InitAppService();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  SendInitialized();

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->NotifyObservers(nullptr, "embedliteInitialized", nullptr);
  }

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
EmbedLiteAppProcessChild::AllocPEmbedLiteViewChild(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
{
  LOGT("id:%u, parentId:%u", id, parentId);
  static bool sViewInitializeOnce = false;
  if (!sViewInitializeOnce) {
    gfxPlatform::GetPlatform()->ComputeTileSize();
    sViewInitializeOnce = true;
  }
  EmbedLiteViewProcessChild* view = new EmbedLiteViewProcessChild(id, parentId, isPrivateWindow);
  view->AddRef();
  return view;
}

PCompositorChild*
EmbedLiteAppProcessChild::AllocPCompositorChild(Transport* aTransport, ProcessId aOtherProcess)
{
  LOGT("!!!!!!!!!!!!!!!!!!!!!!!!!Need to CompositorChild::Create(aTransport, aOtherProcess)");
  return 0; //CompositorChild::Create(aTransport, aOtherProcess);
}

} // namespace embedlite
} // namespace mozilla

