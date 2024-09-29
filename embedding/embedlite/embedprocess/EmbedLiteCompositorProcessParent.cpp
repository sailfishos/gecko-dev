/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorProcessParent.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include <stdio.h>                      // for fprintf, stdout
#include <stdint.h>                     // for uint64_t
#include <map>                          // for _Rb_tree_iterator, etc
#include <utility>                      // for pair

#include "VsyncSource.h"

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
               ProcessHandle aHandle,
               MessageLoop* aIOLoop)
{
  LOGT();
}

/*static*/ PCompositorBridgeParent*
EmbedLiteCompositorProcessParent::Create(ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
{
  LOGT();

  gfxPlatform::InitLayersIPC();

  RefPtr<EmbedLiteCompositorProcessParent> cpcp =
    new EmbedLiteCompositorProcessParent(aOtherProcess, aSurfaceWidth, aSurfaceHeight, id);
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  cpcp->mSelfRef = cpcp;
  CompositorThread()->Dispatch(
    NewRunnableFunction("mozilla::embedlite::EmbedLiteAppProcessParent::OpenCompositor",
                        &OpenCompositor, cpcp.get(),
                        handle, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

EmbedLiteCompositorProcessParent::EmbedLiteCompositorProcessParent(ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
/*
  : CompositorBridgeParent(nullptr,
                           CSSToLayoutDeviceScale(1.0),
                           gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher()->GetVsyncRate(),
                           CompositorOptions(true, false),
                           true,
                           IntSize(aSurfaceWidth, aSurfaceHeight))
 */
  : mChildProcessId(aOtherProcess)
{
  LOGT();
  LOGT("mbedLiteCompositorProcessParent");
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform();
}

void
EmbedLiteCompositorProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT();
  MessageLoop::current()->PostTask(
        NewRunnableMethod("mozilla::embedlite::EmbedLiteCompositorProcessParent::DeferredDestroy",
                          this, &EmbedLiteCompositorProcessParent::DeferredDestroy));
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
EmbedLiteCompositorProcessParent::GetFrameUniformity(const LayersId& aLayersId,
                                                  FrameUniformityData* aOutData) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->GetFrameUniformity(aLayersId, aOutData);
}

void
EmbedLiteCompositorProcessParent::SetConfirmedTargetAPZC(const LayersId& aLayersId, const uint64_t& aInputBlockId, nsTArray<ScrollableLayerGuid>&& aTargets)
{
  LOGT("Implement me");
  Unused << aLayersId;
  Unused << aInputBlockId;
  Unused << aTargets;
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
  MOZ_ASSERT(IToplevelProtocol::GetTransport());
}

} // namespace embedlite
} // namespace mozilla

