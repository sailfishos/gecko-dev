/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorProcessParent.h"
#include "mozilla/layers/LayerTransactionParent.h"     // for LayerTransactionParent
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include <stdio.h>                      // for fprintf, stdout
#include <stdint.h>                     // for uint64_t
#include <map>                          // for _Rb_tree_iterator, etc
#include <utility>                      // for pair

#include "VsyncSource.h"

#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/CompositorThread.h" // for CompositorThreadHolder
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL

namespace mozilla {
namespace embedlite {

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace std;

static void
OpenCompositor(EmbedLiteCompositorProcessParent* aCompositor,
               Transport* aTransport, ProcessHandle aHandle,
               MessageLoop* aIOLoop)
{
  LOGT();
}

/*static*/ PCompositorBridgeParent*
EmbedLiteCompositorProcessParent::Create(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
{
  LOGT();

  gfxPlatform::InitLayersIPC();

  RefPtr<EmbedLiteCompositorProcessParent> cpcp =
    new EmbedLiteCompositorProcessParent(aTransport, aOtherProcess, aSurfaceWidth, aSurfaceHeight, id);
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  cpcp->mSelfRef = cpcp;
  CompositorThread()->Dispatch(
    NewRunnableFunction("mozilla::embedlite::EmbedLiteAppProcessParent::OpenCompositor",
                        &OpenCompositor, cpcp.get(),
                        aTransport, handle, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

EmbedLiteCompositorProcessParent::EmbedLiteCompositorProcessParent(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
  : CompositorBridgeParent(nullptr,
                           CSSToLayoutDeviceScale(1.0),
                           gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().GetVsyncRate(),
                           CompositorOptions(true, false, false),
                           true,
                           IntSize(aSurfaceWidth, aSurfaceHeight))
  , mTransport(aTransport)
  , mChildProcessId(aOtherProcess)
  , mNotifyAfterRemotePaint(false)
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform();
}

mozilla::ipc::IPCResult EmbedLiteCompositorProcessParent::RecvRequestNotifyAfterRemotePaint()
{
  LOGT("Implement me");
  mNotifyAfterRemotePaint = true;
  return IPC_OK();
}

void
EmbedLiteCompositorProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT();
  MessageLoop::current()->PostTask(
        NewRunnableMethod("mozilla::embedlite::EmbedLiteCompositorProcessParent::DeferredDestroy",
                          this, &EmbedLiteCompositorProcessParent::DeferredDestroy));
}

void
EmbedLiteCompositorProcessParent::InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints)
{
  NS_ASSERTION(!mLayerManager, "Already initialised mLayerManager");
  NS_ASSERTION(!mCompositor,   "Already initialised mCompositor");

  for (size_t i = 0; i < aBackendHints.Length(); ++i) {
    RefPtr<Compositor> compositor;
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      compositor = new CompositorOGL(this,
                                     nullptr,
                                     mEGLSurfaceSize.width,
                                     mEGLSurfaceSize.height,
                                     true);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_BASIC) {
#ifdef MOZ_WIDGET_GTK
      if (gfxPlatformGtk::GetPlatform()->UseXRender()) {
        compositor = new X11BasicCompositor(this, nullptr);
      } else
#endif
      {
        compositor = new BasicCompositor(this, nullptr);
      }
#ifdef XP_WIN
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
      compositor = new CompositorD3D11(this, nullptr);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D9) {
      compositor = new CompositorD3D9(this, nullptr);
#endif
    }

    if (!compositor) {
      // We passed a backend hint for which we can't create a compositor.
      // For example, we sometime pass LayersBackend::LAYERS_NONE as filler in aBackendHints.
      continue;
    }

    RefPtr<LayerManagerComposite> layerManager = new LayerManagerComposite(compositor);

    nsCString failureReason;
    if (compositor->Initialize(&failureReason)) {
      if (failureReason.IsEmpty()){
        failureReason = "SUCCESS";
      }
      mLayerManager = layerManager;
      MOZ_ASSERT(compositor);
      mCompositor = compositor;
      return;
    }

    gfxCriticalNote << "[OPENGL] Failed to init compositor with reason: "
                    << failureReason.get();
  }
}

PLayerTransactionParent*
EmbedLiteCompositorProcessParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                               const LayersId& aId)
{
  LOGT();
  MOZ_ASSERT(aId != 0);

  InitializeLayerManager(aBackendHints);

  if (!mLayerManager) {
    NS_WARNING("Failed to initialise Compositor");

    // XXX: should be false, but that causes us to fail some tests on Mac w/ OMTC.
    // Bug 900745. change *aSuccess to false to see test failures.
    LayerTransactionParent *p = new LayerTransactionParent(nullptr, this, nullptr, aId, mVsyncRate);
    p->AddIPDLReference();
    return p;
  }

  // Check state etc from CompositorBridgeParent sIndirectLayerTrees
  // mCompositionManager = new AsyncCompositionManager(mLayerManager);
  LayerTransactionParent *p = new LayerTransactionParent(nullptr, this, nullptr, aId, mVsyncRate);
  p->AddIPDLReference();
  return p;
}

bool
EmbedLiteCompositorProcessParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  LOGT();
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

mozilla::ipc::IPCResult
EmbedLiteCompositorProcessParent::RecvNotifyChildCreated(const LayersId& child, CompositorOptions *aOptions)
{
  LOGT("Implement me");
  return IPC_OK();
}

bool
EmbedLiteCompositorProcessParent::SetTestSampleTime(const LayersId &aId, const TimeStamp& aTime)
{
  LOGT("Implement me");
  Unused << aId;
  Unused << aTime;
  return false;
}

void
EmbedLiteCompositorProcessParent::LeaveTestMode(const LayersId &aId)
{
  LOGT("Implement me");
  Unused << aId;
}

void
EmbedLiteCompositorProcessParent::ApplyAsyncProperties(LayerTransactionParent *aLayerTree, TransformsToSkip aSkip)
{
  LOGT("Implement me");
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  Unused << id;
}

void
EmbedLiteCompositorProcessParent::FlushApzRepaints(const LayersId &aLayersId)
{
  LOGT("Implement me");
  Unused << aLayersId;
}

void
EmbedLiteCompositorProcessParent::GetAPZTestData(const LayersId &aLayersId,
                                                 APZTestData* aOutData)
{
  LOGT("Implement me");
  Unused << aLayersId;
  Unused << aOutData;
}

void
EmbedLiteCompositorProcessParent::SetConfirmedTargetAPZC(const LayersId &aLayersId, const uint64_t &aInputBlockId, const nsTArray<ScrollableLayerGuid> &aTargets)
{
  LOGT("Implement me");
  Unused << aLayersId;
  Unused << aInputBlockId;
  Unused << aTargets;
}

AsyncCompositionManager*
EmbedLiteCompositorProcessParent::GetCompositionManager(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  Unused << id;
  return nullptr;
}

void
EmbedLiteCompositorProcessParent::DeferredDestroy()
{
  LOGT();
  mSelfRef = nullptr;
}

EmbedLiteCompositorProcessParent::~EmbedLiteCompositorProcessParent()
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetIOMessageLoop());
  RefPtr<DeleteTask<Transport>> task = new DeleteTask<Transport>(mTransport);
  XRE_GetIOMessageLoop()->PostTask(task.forget());
}

} // namespace embedlite
} // namespace mozilla

