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
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"
#include "EmbedLiteAppProcessParent.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h" // for mozilla::Runnable
#include "base/command_line.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/ImageBridgeParent.h"

#include "EmbedLiteViewProcessParent.h"
#include "EmbedLiteCompositorProcessParent.h"

static BrowserProcessSubThread* sIOThread;

using namespace mozilla::dom;
using namespace base;
using base::KillProcess;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace embedlite {

// Temporary manager which allows to call InitLog
class EmbedLiteAppProcessParentManager final : public mozilla::layers::LayerManager
{
public:
  explicit EmbedLiteAppProcessParentManager()
  {
  }

protected:
  virtual bool BeginTransaction(const nsCString &aURL = nsCString()) override { return false; }
  virtual bool BeginTransactionWithTarget(gfxContext*, const nsCString &aURL = nsCString()) override { return false; }
  virtual bool EndEmptyTransaction(mozilla::layers::LayerManager::EndTransactionFlags) override { return false; }
  virtual void EndTransaction(mozilla::layers::LayerManager::DrawPaintedLayerCallback,
                              void *aCallbackData,
                              mozilla::layers::LayerManager::EndTransactionFlags = mozilla::layers::LayerManager::END_DEFAULT) override {
    Unused << aCallbackData;
  }
  virtual void SetRoot(mozilla::layers::Layer*) override {}
  virtual already_AddRefed<mozilla::layers::PaintedLayer> CreatePaintedLayer() override { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ContainerLayer> CreateContainerLayer() override { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ImageLayer> CreateImageLayer() override { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ColorLayer> CreateColorLayer() override { return nullptr; }
  virtual already_AddRefed<mozilla::layers::CanvasLayer> CreateCanvasLayer() override { return nullptr; }
  virtual mozilla::layers::LayersBackend GetBackendType() override { return LayersBackend::LAYERS_OPENGL; }
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

  NS_ASSERTION(false, "Fix IToplevelProtocol::SetTransport(mSubprocess->GetChannel())");
  //IToplevelProtocol::SetTransport(mSubprocess->GetChannel());

  // set gGREBinPath
  gGREBinPath = ToNewUnicode(nsDependentCString(getenv("GRE_HOME")));

  if (!CommandLine::IsInitialized()) {
    CommandLine::Init(0, nullptr);
  }

  std::vector<std::string> extraArgs;
  extraArgs.push_back("-embedlite");
  mSubprocess->LaunchAndWaitForProcessHandle(extraArgs);
  Open(mSubprocess->TakeInitialPort(), base::GetProcId(mSubprocess->GetChildProcessHandle()));
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);

  ProcessHandle otherProcessHandle;
  if (base::OpenProcessHandle(OtherPid(), &otherProcessHandle)) {
    base::CloseProcessHandle(otherProcessHandle);
  }

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
                                            const uintptr_t &parentBrowsingContext,
                                            const uint32_t &chromeFlags,
                                            uint32_t *createdID,
                                            bool *cancel)
{
  LOGT();
  *createdID = mApp->CreateWindowRequested(chromeFlags, parentId, parentBrowsingContext);
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
                                                     const uintptr_t &parentBrowsingContext,
                                                     const bool &isPrivateWindow,
                                                     const bool &isDesktopMode)
{
  LOGT();

  static bool sCompositorCreated = false;
  if (!sCompositorCreated) {
    sCompositorCreated = true;
    mozilla::layers::CompositorThreadHolder::Start();
  }

  EmbedLiteViewProcessParent* p = new EmbedLiteViewProcessParent(windowId, id, parentId, parentBrowsingContext, isPrivateWindow, isDesktopMode);
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
EmbedLiteAppProcessParent::AllocPEmbedLiteWindowParent(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener)
{
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
      channel->CloseWithError();
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

