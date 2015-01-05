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

#include "EmbedLog.h"

#include "nsXPCOMPrivate.h"
#include "GeckoLoader.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"
#include "EmbedLiteAppProcessParent.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "nsThreadManager.h"
#include "nsAutoPtr.h"
#include "base/command_line.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"

#include "EmbedLiteViewProcessParent.h"
#include "EmbedLiteCompositorProcessParent.h"

static BrowserProcessSubThread* sIOThread;

using namespace mozilla::dom;
using namespace base;
using base::ChildPrivileges;
using base::KillProcess;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessParent*
EmbedLiteAppProcessParent::CreateEmbedLiteAppProcessParent()
{
  LOGT();
  // Establish the main thread here.
  if (NS_FAILED(nsThreadManager::get()->Init())) {
    NS_ERROR("Could not initialize thread manager");
    return nullptr;
  }

  NS_SetMainThread();

  return new EmbedLiteAppProcessParent();
}

EmbedLiteAppProcessParent::EmbedLiteAppProcessParent()
  : mApp(EmbedLiteApp::GetInstance())
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppProcessParent);

  mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, base::PRIVILEGES_DEFAULT);

  PR_SetEnv("NECKO_SEPARATE_STACKS=1");
  if (!BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)) {
      UniquePtr<BrowserProcessSubThread> ioThread(new BrowserProcessSubThread(BrowserProcessSubThread::IO));
    if (!ioThread.get()) {
      return;
    }

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    if (!ioThread->StartWithOptions(options)) {
      return;
    }
    sIOThread = ioThread.release();
  }

  IToplevelProtocol::SetTransport(mSubprocess->GetChannel());

  // set gGREBinPath
  gGREBinPath = ToNewUnicode(nsDependentCString(getenv("GRE_HOME")));

  if (!CommandLine::IsInitialized()) {
    CommandLine::Init(0, nullptr);
  }

  std::vector<std::string> extraArgs;
  extraArgs.push_back("-embedlite");
  mSubprocess->LaunchAndWaitForProcessHandle(extraArgs);
  Open(mSubprocess->GetChannel(), mSubprocess->GetOwnedChildProcessHandle());
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);
  if (OtherProcess())
    base::CloseProcessHandle(OtherProcess());

  mApp->ChildReadyToDestroy();
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
  mApp->Initialized();
  return true;
}

bool
EmbedLiteAppProcessParent::RecvReadyToShutdown()
{
  LOGT();
  MessageLoop::current()->PostTask(
    FROM_HERE, NewRunnableMethod(this, &EmbedLiteAppProcessParent::ShutDownProcess, /* force */ false));

  return true;
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
  *createdID = mApp->CreateWindowRequested(chromeFlags, uri.get(), contextFlags, parentId);
  *cancel = !*createdID;
  return true;
}

bool
EmbedLiteAppProcessParent::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT();
  return false;
}

PEmbedLiteViewParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteViewParent(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
{
  LOGT();

  static bool sCompositorCreated = false;
  if (!sCompositorCreated) {
    sCompositorCreated = true;
    mozilla::layers::CompositorParent::StartUp();
    bool useOffMainThreadCompositing = !!CompositorParent::CompositorLoop();
    LOGT("useOffMainThreadCompositing:%i", useOffMainThreadCompositing);
    if (useOffMainThreadCompositing)
    {
      DebugOnly<bool> opened = PCompositor::Open(this);
      MOZ_ASSERT(opened);
    }
  }

  EmbedLiteViewProcessParent* p = new EmbedLiteViewProcessParent(id, parentId, isPrivateWindow);
  p->AddRef();
  return p;
}

bool
EmbedLiteAppProcessParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* aActor)
{
  LOGT();
  EmbedLiteViewProcessParent* p = static_cast<EmbedLiteViewProcessParent*>(aActor);
  p->Release();
  return true;
}

namespace {

void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
  LOGT();
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
    new DeleteTask<GeckoChildProcessHost>(aSubprocess));
}

// This runnable only exists to delegate ownership of the
// EmbedLiteAppProcessParent to this runnable, until it's deleted by the event
// system.
struct DelayedDeleteContentParentTask : public nsRunnable
{
  explicit DelayedDeleteContentParentTask(EmbedLiteAppProcessParent* aObj) : mObj(aObj) { }

  // No-op
  NS_IMETHODIMP Run() { return NS_OK; }

  nsRefPtr<EmbedLiteAppProcessParent> mObj;
};

}

void
EmbedLiteAppProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("Reason:%d", aWhy);

  if (aWhy != NormalShutdown) {
    ShutDownProcess(true);
  }

  MessageLoop::current()->
    PostTask(FROM_HERE,
             NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
  mSubprocess = nullptr;
}

void
EmbedLiteAppProcessParent::ShutDownProcess(bool aCloseWithError)
{
  LOGT();
  // If Close() fails with an error, we'll end up back in this function, but
  // with aCloseWithError = true.  It's important that we call
  // CloseWithError() in this case; see bug 895204.

  if (!aCloseWithError) {
    // Close() can only be called once: It kicks off the destruction
    // sequence.
    Close();
  }

  if (aCloseWithError) {
    MessageChannel* channel = GetIPCChannel();
    if (channel) {
      channel->CloseWithError();
    }
  }
}

PCompositorParent*
EmbedLiteAppProcessParent::AllocPCompositorParent(Transport* aTransport,
                                                  ProcessId aOtherProcess)
{
  LOGT();
  return EmbedLiteCompositorProcessParent::Create(aTransport, aOtherProcess);
}


} // namespace embedlite
} // namespace mozilla

