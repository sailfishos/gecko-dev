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
#include "mozilla/Unused.h"
#if defined(ANDROID) || defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "EmbedLog.h"

#include "nsXPCOMPrivate.h"
#include "GeckoLoader.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/dom/RemoteType.h"
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"
#include "EmbedLiteAppProcessParent.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "base/command_line.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "WindowRenderer.h"

#include "EmbedLiteViewProcessParent.h"

using namespace mozilla::dom;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace embedlite {

// Temporary manager which allows to call InitLog
class EmbedLiteAppProcessParentManager final : public WindowRenderer
{
public:
  explicit EmbedLiteAppProcessParentManager()
  {
  }

protected:
  virtual bool BeginTransaction(const nsCString &aURL = nsCString()) override { return false; }
  virtual bool BeginTransactionWithTarget(gfxContext*, const nsCString &aURL = nsCString()) { return false; }
  virtual bool EndEmptyTransaction(WindowRenderer::EndTransactionFlags) override { return false; }
  virtual mozilla::layers::LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual int32_t GetMaxTextureSize() const override { return 0; }
  virtual void GetBackendName(nsAString&) override {}
};

static EmbedLiteAppProcessParent* sAppProcessParent = nullptr;

EmbedLiteAppProcessParent*
EmbedLiteAppProcessParent::GetInstance()
{
  return sAppProcessParent;
}

EmbedLiteAppProcessParent*
EmbedLiteAppProcessParent::CreateEmbedLiteAppProcessParent()
{
  LOGT();
  // Establish the main thread here.
  NS_ASSERTION(false, "nsThreadManager::get()->Init()");
#if 0
  if (NS_FAILED(nsThreadManager::get()->Init())) {
    NS_ERROR("Could not initialize thread manager");
    return nullptr;
  }
#endif

  NS_SetMainThread();

  return new EmbedLiteAppProcessParent();
}

EmbedLiteAppProcessParent::EmbedLiteAppProcessParent()
  : mApp(EmbedLiteApp::GetInstance())
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppProcessParent);
  sAppProcessParent = this;

  mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content);

  PR_SetEnv("NECKO_SEPARATE_STACKS=1");

  NS_ASSERTION(false, "Fix IToplevelProtocol::SetTransport(mSubprocess->GetChannel())");
  //IToplevelProtocol::SetTransport(mSubprocess->GetChannel());

  // NS_InitXPCOM sets this to the current GRE directory. The child launcher
  // uses it to locate plugin-container.
  MOZ_RELEASE_ASSERT(gGREBinPath);

  if (!CommandLine::IsInitialized()) {
    CommandLine::Init(0, nullptr);
  }

  geckoargs::ChildProcessArgs extraArgs;
  geckoargs::sEmbedLite.Put(true, extraArgs);

  SharedPreferenceSerializer prefSerializer;
  MOZ_RELEASE_ASSERT(prefSerializer.SerializeToSharedMemory(
      GeckoProcessType_Content, DEFAULT_REMOTE_TYPE));
  prefSerializer.AddSharedPrefCmdLineArgs(*mSubprocess, extraArgs);
  ExportSharedJSInit(*mSubprocess, extraArgs);
  ProcessChild::AddPlatformBuildID(extraArgs);

  MOZ_RELEASE_ASSERT(
      mSubprocess->LaunchAndWaitForProcessHandle(std::move(extraArgs)));
  bool opened = mSubprocess->TakeInitialEndpoint().Bind(this);
  MOZ_RELEASE_ASSERT(opened);
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);

  mApp->ChildReadyToDestroy();
}

mozilla::ipc::IPCResult
EmbedLiteAppProcessParent::RecvInitialized()
{
  LOGT();
  PR_SetEnv("MOZ_LAYERS_PREFER_OFFSCREEN=1");
  mApp->Initialized();
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteAppProcessParent::RecvReadyToShutdown()
{
  LOGT();
  MessageLoop::current()->PostTask(NewRunnableMethod<bool>("mozilla::embedlite::EmbedLiteAppProcessParent::ShutDownProcess",
                                                           this,
                                                           &EmbedLiteAppProcessParent::ShutDownProcess,
                                                           /* force */ false));

  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteAppProcessParent::RecvCreateWindow(const uint32_t &parentId,
                                            const EmbedLiteBrowserInitData &browserInit,
                                            const uint32_t &chromeFlags,
                                            const bool &hidden,
                                            uint32_t *createdID,
                                            bool *cancel)
{
  LOGT();
  PushBrowserInit(browserInit);
  *createdID = mApp->CreateWindowRequested(chromeFlags, hidden, parentId, 0);
  PopBrowserInit();
  *cancel = !*createdID;
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteAppProcessParent::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT();
  return IPC_OK();
}

PEmbedLiteViewParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteViewParent(const uint32_t &windowId,
                                                     const uint32_t &id,
                                                     const uint32_t &parentId,
                                                     const bool &isPrivateWindow,
                                                     const bool &isDesktopMode,
                                                     const bool &isHidden,
                                                     const Maybe<EmbedLiteBrowserInitData> &browserInit)
{
  LOGT();

  static bool sCompositorCreated = false;
  if (!sCompositorCreated) {
    sCompositorCreated = true;
    mozilla::layers::CompositorThreadHolder::Start();
  }

  (void) browserInit;
  EmbedLiteViewProcessParent* p = new EmbedLiteViewProcessParent(
    windowId, id, parentId, isPrivateWindow, isDesktopMode, isHidden);
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

PEmbedLiteWindowParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteWindowParent(
    const uint16_t &width, const uint16_t &height, const uint32_t &id,
    const uintptr_t &aListener, const bool &chromeHosted,
    const nsCString &initialContentURI)
{
  Unused << width;
  Unused << height;
  Unused << id;
  Unused << aListener;
  Unused << chromeHosted;
  Unused << initialContentURI;
  LOGNI();

  return nullptr;
}

bool
EmbedLiteAppProcessParent::DeallocPEmbedLiteWindowParent(PEmbedLiteWindowParent* aActor)
{
  LOGNI();

  return true;
}

namespace {

// This runnable only exists to delegate ownership of the
// EmbedLiteAppProcessParent to this runnable, until it's deleted by the event
// system.
struct DelayedDeleteContentParentTask : public mozilla::Runnable
{
 public:
  explicit DelayedDeleteContentParentTask(EmbedLiteAppProcessParent* aObj)
        : mozilla::Runnable("DelayedDeleteContentParentTask")
        , mObj(aObj) { }

  // No-op
  NS_IMETHODIMP Run() override { return NS_OK; }

  RefPtr<EmbedLiteAppProcessParent> mObj;
};

}

void
EmbedLiteAppProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("Reason:%d", aWhy);

  if (aWhy != NormalShutdown) {
    ShutDownProcess(true);
  }

  MessageLoop::current()->PostTask(NS_NewRunnableFunction("mozilla::embedlite::EmbedLiteAppProcessParent::DelayedDeleteSubprocess", [subprocess = mSubprocess] { subprocess->Destroy(); }));
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
      channel->InduceConnectionError();
    }
  }
}

mozilla::ipc::IPCResult
EmbedLiteAppProcessParent::RecvPrefsArrayInitialized(nsTArray<mozilla::dom::Pref>&& prefs)
{
  LOGT();
  mPrefs = std::move(prefs);
  return IPC_OK();
}

void
EmbedLiteAppProcessParent::GetPrefs(nsTArray<mozilla::dom::Pref> *prefs)
{
  prefs = &mPrefs;
}

} // namespace embedlite
} // namespace mozilla
