/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorProcessParent_h
#define mozilla_layers_EmbedLiteCompositorProcessParent_h

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "Layers.h"
#include "EmbedLiteViewThreadParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteCompositorProcessParent final : public CompositorBridgeParent
{
  friend class CompositorBridgeParent;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(EmbedLiteCompositorProcessParent)
public:
  EmbedLiteCompositorProcessParent(ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // FIXME/bug 774388: work out what shutdown protocol we need.
  virtual mozilla::ipc::IPCResult RecvPause() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvResume() override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvNotifyChildCreated(const LayersId& child, CompositorOptions* aOptions) override;
  virtual mozilla::ipc::IPCResult RecvAdoptChild(const LayersId& child) override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                                   const gfx::IntRect& aRect) { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvFlushRendering(const wr::RenderReasons& aReasons) override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvNotifyRegionInvalidated(
          const nsIntRegion& aRegion) { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvStartFrameTimeRecording(
          const int32_t& aBufferSize, uint32_t* aOutStartIndex) override { return IPC_OK(); }
  virtual mozilla::ipc::IPCResult RecvStopFrameTimeRecording(
          const uint32_t& aStartIndex, nsTArray<float>* intervals) override { return IPC_OK(); }

  virtual bool SetTestSampleTime(const LayersId& aId,
                                 const TimeStamp& aTime) override;
  virtual void LeaveTestMode(const LayersId& aId) override;
  virtual void FlushApzRepaints(const LayersId& aLayersId) override;
  virtual void GetAPZTestData(const LayersId& aLayersId,
                              APZTestData* aOutData) override;
  virtual void GetFrameUniformity(const LayersId& aLayersId,
                          FrameUniformityData* aOutData) override;
  virtual void SetConfirmedTargetAPZC(const LayersId& aLayersId,
                                      const uint64_t& aInputBlockId,
                                      nsTArray<ScrollableLayerGuid>&& aTargets) override;

  /**
   * A new child process has been configured to push transactions
   * directly to us.
   */
  static PCompositorBridgeParent*
  Create(ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id);

private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~EmbedLiteCompositorProcessParent();

  void DeferredDestroy();

  // There can be many CPCPs, and IPDL-generated code doesn't hold a
  // reference to top-level actors.  So we hold a reference to
  // ourself.  This is released (deferred) in ActorDestroy().
  RefPtr<EmbedLiteCompositorProcessParent> mSelfRef;
  // Child side's process Id.
  base::ProcessId mChildProcessId;

  // If true, we should send a RemotePaintIsReady message when the layer transaction
  // is received
  bool mNotifyAfterRemotePaint;

  RefPtr<Compositor> mCompositor;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorProcessParent_h
