/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteView.h"
#include "EmbedLiteViewParent.h"
#include "EmbedLiteWindowParent.h"
#include "EmbedLiteWindowChild.h"
#include "nsWindow.h"

#include "EmbedLiteCompositorBridgeParent.h"
#include "mozilla/Unused.h"
#include "EmbedContentController.h"
#include "mozilla/layers/APZThreadUtils.h"

#include <sys/syscall.h>

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

EmbedLiteViewParent::EmbedLiteViewParent(const uint32_t& windowId, const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
  : mWindowId(windowId)
  , mId(id)
  , mViewAPIDestroyed(false)
  , mWindow(*EmbedLiteWindowParent::From(windowId))
  , mCompositor(nullptr)
  , mDPI(-1.0)
  , mUILoop(MessageLoop::current())
  , mLastIMEState(0)
  , mUploadTexture(0)
  , mApzcTreeManager(nullptr)
  , mContentController(new EmbedContentController(this, mUILoop))
{
  MOZ_COUNT_CTOR(EmbedLiteViewParent);

  APZThreadUtils::SetControllerThread(mUILoop);

  /// XXX: Fix this
  if (mWindow.GetCompositor()) {
    SetCompositor(mWindow.GetCompositor());
  }

  mWindow.AddObserver(this);
}

NS_IMETHODIMP EmbedLiteViewParent::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  LOGT("Implement me");
  return NS_OK;
}

EmbedLiteViewParent::~EmbedLiteViewParent()
{
  MOZ_COUNT_DTOR(EmbedLiteViewParent);
  LOGT("mView:%p, mCompositor:%p", mView, mCompositor.get());
  mContentController = nullptr;
  mWindow.RemoveObserver(this);
}

void
EmbedLiteViewParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason: %i", aWhy);
  mContentController = nullptr;
}

void
EmbedLiteViewParent::SetCompositor(EmbedLiteCompositorBridgeParent* aCompositor)
{
  mCompositor = aCompositor;
  LOGT("compositor: %p", mCompositor.get());
  UpdateScrollController();
}

void
EmbedLiteViewParent::UpdateScrollController()
{
  LOGT("destroyed: %d mVIew: %p compositor: %p\n", mViewAPIDestroyed, mView, mCompositor.get());
  if (mViewAPIDestroyed || !mView) {
    return;
  }

  if (mCompositor) {
    if ((mDPI > 0) && GetApzcTreeManager()) {
      GetApzcTreeManager()->SetDPI(mDPI);
    }

    nsWindow *window = GetWindowWidget();
    if (window) {
      window->SetContentController(mContentController);
    }
  }
}

mozilla::layers::IAPZCTreeManager *EmbedLiteViewParent::GetApzcTreeManager()
{
  nsWindow *window = GetWindowWidget();

  if (!mApzcTreeManager && mCompositor && window) {
    mApzcTreeManager = window->GetAPZCTreeManager();
  }
  return mApzcTreeManager.get();
}

// Child notification

mozilla::ipc::IPCResult
EmbedLiteViewParent::RecvInitialized()
{
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->ViewInitialized();
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteViewParent::RecvDestroyed()
{
  LOGT("view destroyed: %d", mViewAPIDestroyed);
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->Destroyed();
  return IPC_OK();
}

mozilla::ipc::IPCResult
EmbedLiteViewParent::RecvOnLocationChanged(const nsCString& aLocation,
                                           const bool& aCanGoBack,
                                           const bool& aCanGoForward)
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnLocationChanged(aLocation.get(), aCanGoBack, aCanGoForward);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnLoadStarted(const nsCString& aLocation)
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnLoadStarted(aLocation.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnLoadFinished()
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnLoadFinished();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnWindowCloseRequested()
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnWindowCloseRequested();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnLoadRedirect()
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnLoadRedirect();
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnLoadProgress(const int32_t &aProgress, const int32_t &aCurTotal, const int32_t &aMaxTotal)
{
  LOGNI("progress:%i", aProgress);
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnLoadProgress(aProgress, aCurTotal, aMaxTotal);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnSecurityChanged(const nsCString &aStatus,
                                                                   const uint32_t &aState)
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnSecurityChanged(aStatus.get(), aState);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnFirstPaint(const int32_t &aX,
                                                              const int32_t &aY)
{
  LOGNI();
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnFirstPaint(aX, aY);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnScrolledAreaChanged(const uint32_t &aWidth,
                                                                       const uint32_t &aHeight)
{
  LOGNI("area[%u,%u]", aWidth, aHeight);
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnScrolledAreaChanged(aWidth, aHeight);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnScrollChanged(const int32_t& offSetX,
                                                                 const int32_t& offSetY)
{
  LOGNI("off[%i,%i]", offSetX, offSetY);
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnScrollChanged(offSetX, offSetY);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvOnTitleChanged(const nsString& aTitle)
{
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->OnTitleChanged(aTitle.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvUpdateZoomConstraints(const uint32_t &aPresShellId,
                                                                       const ViewID &aViewId,
                                                                       const Maybe<ZoomConstraints> &aConstraints)
{
  LOGT("manager: %p", GetApzcTreeManager());
  nsWindow *window = GetWindowWidget();
  if (GetApzcTreeManager() && window) {
    GetApzcTreeManager()->UpdateZoomConstraints(ScrollableLayerGuid(window->GetRootLayerId(),
                                                                    aPresShellId,
                                                                    aViewId),
                                                aConstraints);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvZoomToRect(const uint32_t &aPresShellId,
                                                            const ViewID &aViewId,
                                                            const CSSRect &aRect)
{
  LOGT("thread id: %ld", syscall(SYS_gettid));
  nsWindow *window = GetWindowWidget();
  if (GetApzcTreeManager() && window) {
    GetApzcTreeManager()->ZoomToRect(ScrollableLayerGuid(window->GetRootLayerId(),
                                                         aPresShellId,
                                                         aViewId),
                                     aRect);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvContentReceivedInputBlock(const ScrollableLayerGuid &aGuid,
                                                                           const uint64_t &aInputBlockId,
                                                                           const bool &aPreventDefault)
{
  nsWindow *window = GetWindowWidget();
  if (window && (aGuid.mLayersId != window->GetRootLayerId())) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvContentReceivedInputBlock; dropping message...");
    return IPC_OK();
  }

  if (GetApzcTreeManager()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<uint64_t, bool>
                                          ("IAPZCTreeManager::ContentReceivedInputBlock",
                                           GetApzcTreeManager(),
                                           &IAPZCTreeManager::ContentReceivedInputBlock,
                                           aInputBlockId,
                                           aPreventDefault));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvSetBackgroundColor(const nscolor &aColor)
{
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mView->GetListener()->SetBackgroundColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
  return IPC_OK();
}

// Incoming API calls

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvAsyncMessage(const nsString &aMessage,
                                                              const nsString &aData)
{
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

#if EMBEDLITE_LOG_SENSITIVE
  LOGF("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
#endif

  mView->GetListener()->RecvAsyncMessage(aMessage.get(), aData.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvSyncMessage(const nsString &aMessage,
                                                             const nsString &aJSON,
                                                             nsTArray<nsString> *aJSONRetVal)
{
#if EMBEDLITE_LOG_SENSITIVE
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aJSON).get());
#endif
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  char* retval = mView->GetListener()->RecvSyncMessage(aMessage.get(), aJSON.get());
  if (retval && aJSONRetVal) {
    aJSONRetVal->AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(retval)));
    delete retval;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvRpcMessage(const nsString &aMessage,
                                                            const nsString &aJSON,
                                                            nsTArray<nsString> *aJSONRetVal)
{
  return RecvSyncMessage(aMessage, aJSON, aJSONRetVal);
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvSetTargetAPZC(const uint64_t &aInputBlockId,
                                                               nsTArray<ScrollableLayerGuid> &&aTargets)
{
  LOGT("view destoyed: %d", mViewAPIDestroyed);
  NS_ENSURE_TRUE(!mViewAPIDestroyed, IPC_OK());

  nsWindow *window = GetWindowWidget();

  for (size_t i = 0; i < aTargets.Length(); i++) {
    if (window && (aTargets[i].mLayersId != window->GetRootLayerId())) {
      // Guard against bad data from hijacked child processes
      NS_ERROR("Unexpected layers id in SetTargetAPZC; dropping message...");
      return IPC_OK();
    }
  }

  if (GetApzcTreeManager()) {
    // need a local var to disambiguate between the SetTargetAPZC overloads.
    void (IAPZCTreeManager::*setTargetApzcFunc)(uint64_t, const nsTArray<ScrollableLayerGuid>&)
        = &IAPZCTreeManager::SetTargetAPZC;
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                          <uint64_t,
                                           StoreCopyPassByRRef<nsTArray<ScrollableLayerGuid>>>
                                          ("IAPZCTreeManager::SetTargetAPZC",
                                           GetApzcTreeManager(),
                                           setTargetApzcFunc,
                                           aInputBlockId,
                                           aTargets));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvSetAllowedTouchBehavior(const uint64_t &aInputBlockId,
                                                                         nsTArray<mozilla::layers::TouchBehaviorFlags> &&aFlags)
{
  LOGT("view destoyed: %d", mViewAPIDestroyed);
  NS_ENSURE_TRUE(!mViewAPIDestroyed, IPC_OK());

  if (GetApzcTreeManager()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<uint64_t,
                                          StoreCopyPassByRRef<nsTArray<TouchBehaviorFlags>>>
                                          ("IAPZCTreeManager::SetAllowedTouchBehavior",
                                           GetApzcTreeManager(),
                                           &IAPZCTreeManager::SetAllowedTouchBehavior,
                                           aInputBlockId,
                                           Move(aFlags)));
  }
  return IPC_OK();
}

NS_IMETHODIMP
EmbedLiteViewParent::SetDPI(float dpi)
{
  mDPI = dpi;

  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->SetDPI(mDPI);
  }

  return NS_OK;
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvGetDPI(float *aValue)
{
  *aValue = mDPI;
  return IPC_OK();
}

mozilla::embedlite::nsWindow *EmbedLiteViewParent::GetWindowWidget() const
{
  // Use this with care!! Only CompositorSession (and related bits)
  // may be tampered via this.
  return EmbedLiteWindowChild::From(mWindowId)->GetWidget();
}

void
EmbedLiteViewParent::CompositorCreated()
{
  // XXX: Move compositor handling entirely to EmbedLiteWindowParent
  SetCompositor(mWindow.GetCompositor());
}

NS_IMETHODIMP
EmbedLiteViewParent::ReceiveInputEvent(const mozilla::InputData& aEvent)
{
  LOGT("thread: %ld apz: %p", syscall(SYS_gettid), GetApzcTreeManager());
  APZThreadUtils::AssertOnControllerThread();

  if (!GetApzcTreeManager()) {
    // In general mAPZC should not be null, but during initial setup
    // it might be, so we handle that case by ignoring touch input there.
    return NS_OK;
  }

  ScrollableLayerGuid guid;
  uint64_t outInputBlockId;

  mozilla::MultiTouchInput multiTouchInput = aEvent.AsMultiTouchInput();
  nsEventStatus apzResult = GetApzcTreeManager()->InputBridge()->ReceiveInputEvent(multiTouchInput, &guid, &outInputBlockId);

  // If the APZ says to drop it, then we drop it
  if (apzResult == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  if (multiTouchInput.mInputType == MULTITOUCH_INPUT) {
    if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_MOVE) {
      Unused << SendInputDataTouchMoveEvent(guid, multiTouchInput, outInputBlockId, apzResult);
    } else {
      Unused << SendInputDataTouchEvent(guid, multiTouchInput, outInputBlockId, apzResult);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::TextEvent(const char* composite, const char* preEdit)
{
  if (mViewAPIDestroyed) {
    return NS_OK;
  }

  LOGT("commit:%s, pre:%s, mLastIMEState:%i", composite, preEdit, mLastIMEState);
  if (mLastIMEState) {
    Unused << SendHandleTextEvent(NS_ConvertUTF8toUTF16(nsDependentCString(composite)),
                                  NS_ConvertUTF8toUTF16(nsDependentCString(preEdit)));
  } else {
    NS_ERROR("Text event must not be sent while IME disabled");
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::ViewAPIDestroyed()
{
  if (mContentController) {
    mContentController->ClearRenderFrame();
  }
  mViewAPIDestroyed = true;
  mView = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  Unused << SendHandleKeyPressEvent(domKeyCode, gmodifiers, charCode);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  Unused << SendHandleKeyReleaseEvent(domKeyCode, gmodifiers, charCode);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  if (mViewAPIDestroyed) {
    return NS_OK;
  }

  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_START, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));

  GetApzcTreeManager()->InputBridge()->ReceiveInputEvent(event, nullptr, nullptr);
  Unused << SendMouseEvent(NS_LITERAL_STRING("mousedown"),
                           x, y, buttons, 1, modifiers,
                           true);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  if (mViewAPIDestroyed) {
    return NS_OK;
  }

  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_END, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));

  GetApzcTreeManager()->InputBridge()->ReceiveInputEvent(event, nullptr, nullptr);
  Unused << SendMouseEvent(NS_LITERAL_STRING("mouseup"),
                           x, y, buttons, 1, modifiers,
                           true);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  if (mViewAPIDestroyed) {
    return NS_OK;
  }

  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_MOVE, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));

  GetApzcTreeManager()->InputBridge()->ReceiveInputEvent(event, nullptr, nullptr);
  Unused << SendMouseEvent(NS_LITERAL_STRING("mousemove"),
                           x, y, buttons, 1, modifiers,
                           true);
  return NS_OK;
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvGetInputContext(int32_t *aIMEEnabled,
                                                                 int32_t *aIMEOpen)
{
  LOGT("mLastIMEState:%i view: %p listener: %p", mLastIMEState, mView, (mView ? mView->GetListener() : 0));
  NS_ASSERTION(aIMEEnabled, "Passing nullptr for aIMEEnabled. A bug in EmbedLitePuppetWidget.");
  NS_ASSERTION(aIMEOpen, "Passing nullptr for aIMEOpen. A bug in EmbedLitePuppetWidget.");

  *aIMEEnabled = mLastIMEState;
  *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;

  if (mView && mView->GetListener()) {
    mView->GetListener()->GetIMEStatus(aIMEEnabled, aIMEOpen);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteViewParent::RecvSetInputContext(const int32_t &aIMEEnabled,
                                                                 const int32_t &aIMEOpen,
                                                                 const nsString &aType,
                                                                 const nsString &aInputmode,
                                                                 const nsString &aActionHint,
                                                                 const int32_t &aCause,
                                                                 const int32_t &aFocusChange)
{
  LOGT("IMEEnabled:%i, IMEOpen:%i, type:%s, imMode:%s, actHint:%s, cause:%i, focusChange:%i, mLastIMEState:%i->%i",
       aIMEEnabled, aIMEOpen, NS_ConvertUTF16toUTF8(aType).get(), NS_ConvertUTF16toUTF8(aInputmode).get(),
       NS_ConvertUTF16toUTF8(aActionHint).get(), aCause, aFocusChange, mLastIMEState, aIMEEnabled);
  NS_ENSURE_TRUE(mView && !mViewAPIDestroyed, IPC_OK());

  mLastIMEState = aIMEEnabled;
  mView->GetListener()->IMENotification(aIMEEnabled, aIMEOpen, aCause, aFocusChange, aType.get(), aInputmode.get());
  return IPC_OK();
}

NS_IMETHODIMP
EmbedLiteViewParent::GetUniqueID(uint32_t *aId)
{
  *aId = mId;

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewParent::SetEmbedAPIView(EmbedLiteView *aView)
{
  LOGT();
  mView = aView;
  return NS_OK;
}

} // namespace embedlite
} // namespace mozilla
