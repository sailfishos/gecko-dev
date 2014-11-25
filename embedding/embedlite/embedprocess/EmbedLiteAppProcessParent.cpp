/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "EmbedLiteAppProcessParent.h"
#include "EmbedLiteApp.h"
#include "mozilla/Preferences.h"
#if defined(ANDROID) || defined(LINUX)
#include "nsSystemInfo.h"
#endif
#include "mozilla/unused.h"
#if defined(ANDROID) || defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

using namespace base;
using base::ChildPrivileges;
using base::KillProcess;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessParent::EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppProcessParent);
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);
}

void
EmbedLiteAppProcessParent::OnChannelConnected(int32_t pid)
{
  LOGT();
  ProcessHandle handle;
  if (!base::OpenPrivilegedProcessHandle(pid, &handle)) {
    NS_WARNING("Can't open handle to child process.");
  }
  else {
    // we need to close the existing handle before setting a new one.
    base::CloseProcessHandle(OtherProcess());
    SetOtherProcess(handle);

#if defined(ANDROID) || defined(LINUX)
    // Check nice preference
    int32_t nice = Preferences::GetInt("dom.ipc.content.nice", 0);

    // Environment variable overrides preference
    char* relativeNicenessStr = getenv("MOZ_CHILD_PROCESS_RELATIVE_NICENESS");
    if (relativeNicenessStr) {
      nice = atoi(relativeNicenessStr);
    }

    /* make the GUI thread have higher priority on single-cpu devices */
    nsCOMPtr<nsIPropertyBag2> infoService = do_GetService(NS_SYSTEMINFO_CONTRACTID);
    if (infoService) {
      int32_t cpus;
      nsresult rv = infoService->GetPropertyAsInt32(NS_LITERAL_STRING("cpucount"), &cpus);
      if (NS_FAILED(rv)) {
        cpus = 1;
      }
      if (nice != 0 && cpus == 1) {
        setpriority(PRIO_PROCESS, pid, getpriority(PRIO_PROCESS, pid) + nice);
      }
    }
#endif
  }

  // Set a reply timeout. The only time the parent process will actually
  // timeout is through urgent messages (which are used by CPOWs).
  SetReplyTimeoutMs(Preferences::GetInt("dom.ipc.cpow.timeout", 3000));
}


bool
EmbedLiteAppProcessParent::RecvInitialized()
{
  LOGT();
  return true;
}

bool
EmbedLiteAppProcessParent::RecvReadyToShutdown()
{
  LOGT();
  return false;
}

bool
EmbedLiteAppProcessParent::RecvCreateWindow(const uint32_t& parentId,
                                            const nsCString& uri,
                                            const uint32_t& chromeFlags,
                                            const uint32_t& contextFlags,
                                            uint32_t* createdID,
                                            bool* cancel)
{
  LOGT();
  return false;
}

bool
EmbedLiteAppProcessParent::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT();
  return false;
}

PEmbedLiteViewParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteViewParent(const uint32_t& id, const uint32_t& parentId)
{
  LOGT();
  return 0;
}

bool
EmbedLiteAppProcessParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* aActor)
{
  LOGT();
  return false;
}

void
EmbedLiteAppProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT();
}

} // namespace embedlite
} // namespace mozilla

