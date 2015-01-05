/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorProcessParent.h"
#include "mozilla/layers/CompositorParent.h" // for CompositorParent
#include "mozilla/layers/LayerTransactionParent.h"     // for LayerTransactionParent

namespace mozilla {
namespace embedlite {

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;

static void
OpenCompositor(EmbedLiteCompositorProcessParent* aCompositor,
               Transport* aTransport, ProcessHandle aHandle,
               MessageLoop* aIOLoop)
{
  LOGT();
  DebugOnly<bool> ok = aCompositor->Open(aTransport, aHandle, aIOLoop);
  MOZ_ASSERT(ok);
}

/*static*/ PCompositorParent*
EmbedLiteCompositorProcessParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  LOGT();
  nsRefPtr<EmbedLiteCompositorProcessParent> cpcp =
    new EmbedLiteCompositorProcessParent(aTransport, aOtherProcess);
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  cpcp->mSelfRef = cpcp;
  CompositorParent::CompositorLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(OpenCompositor, cpcp.get(),
                        aTransport, handle, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

EmbedLiteCompositorProcessParent::EmbedLiteCompositorProcessParent(Transport* aTransport, ProcessId aOtherProcess)
  : mTransport(aTransport)
  , mChildProcessId(aOtherProcess)
  , mCompositorThreadHolder(sCompositorThreadHolder)
  , mNotifyAfterRemotePaint(false)
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
}

bool
EmbedLiteCompositorProcessParent::RecvGetTileSize(int32_t* aWidth, int32_t* aHeight)
{
  LOGT();
  *aWidth = 256;
  *aHeight = 256;
  return true;
}

bool
EmbedLiteCompositorProcessParent::RecvRequestNotifyAfterRemotePaint()
{
  LOGT();
  mNotifyAfterRemotePaint = true;
  return true;
}

void
EmbedLiteCompositorProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT();
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &EmbedLiteCompositorProcessParent::DeferredDestroy));
}

PLayerTransactionParent*
EmbedLiteCompositorProcessParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>&,
                                                           const uint64_t& aId,
                                                           TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                           bool *aSuccess)
{
  LOGT("Implement me");
  MOZ_ASSERT(aId != 0);

  return nullptr;
}

bool
EmbedLiteCompositorProcessParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  LOGT("Implement me");

  return true;
}

bool
EmbedLiteCompositorProcessParent::RecvNotifyChildCreated(const uint64_t& child)
{
  LOGT("Implement me");

  return false;
}

void
EmbedLiteCompositorProcessParent::ShadowLayersUpdated(
  LayerTransactionParent* aLayerTree,
  const uint64_t& aTransactionId,
  const TargetConfig& aTargetConfig,
  bool aIsFirstPaint,
  bool aScheduleComposite,
  uint32_t aPaintSequenceNumber,
  bool aIsRepeatTransaction)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();

  MOZ_ASSERT(id != 0);
}

void
EmbedLiteCompositorProcessParent::DidComposite(uint64_t aId)
{
  LOGT("Implement me");
}

void
EmbedLiteCompositorProcessParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}

bool
EmbedLiteCompositorProcessParent::SetTestSampleTime(
  LayerTransactionParent* aLayerTree, const TimeStamp& aTime)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  return false;
}

void
EmbedLiteCompositorProcessParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");

  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}

void
EmbedLiteCompositorProcessParent::GetAPZTestData(const LayerTransactionParent* aLayerTree,
                                             APZTestData* aOutData)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}


AsyncCompositionManager*
EmbedLiteCompositorProcessParent::GetCompositionManager(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  return nullptr;
}

void
EmbedLiteCompositorProcessParent::DeferredDestroy()
{
  LOGT("Implement me");
}

EmbedLiteCompositorProcessParent::~EmbedLiteCompositorProcessParent()
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetIOMessageLoop());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(mTransport));
}

IToplevelProtocol*
EmbedLiteCompositorProcessParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                                                base::ProcessHandle aPeerProcess,
                                                mozilla::ipc::ProtocolCloneContext* aCtx)
{
  LOGT("Implement me");

  return nullptr;
}

} // namespace embedlite
} // namespace mozilla

