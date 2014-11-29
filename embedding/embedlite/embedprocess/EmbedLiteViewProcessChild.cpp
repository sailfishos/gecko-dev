#include "EmbedLiteViewProcessChild.h"

#include "EmbedLog.h"
#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT EmbedLiteViewProcessChild::EmbedLiteViewProcessChild(const uint32_t& id, const uint32_t& parentId)
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteViewProcessChild);
  mInitWindowTask = NewRunnableMethod(this,
                                      &EmbedLiteViewProcessChild::InitGeckoWindow, parentId);
  MessageLoop::current()->PostTask(FROM_HERE, mInitWindowTask);
}

MOZ_IMPLICIT EmbedLiteViewProcessChild::~EmbedLiteViewProcessChild()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteViewProcessChild);
}

void
EmbedLiteViewProcessChild::InitGeckoWindow(const uint32_t& parentId)
{
  LOGT("parentID: %u", parentId);
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;

  // TODO initialize Gecko browser
  nsresult rv;
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  gfxPrefs::GetSingleton();
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  unused << SendInitialized();
}

bool
EmbedLiteViewProcessChild::RecvLoadURL(const nsString& url)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvGoBack()
{    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvGoForward()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvStopLoad()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvReload(const bool& hardReload)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvLoadFrameScript(const nsString& uri)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvSetViewSize(const gfxSize& aSize)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvSetGLViewSize(const gfxSize& aSize)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvSetIsActive(const bool& aIsActive)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvSetIsFocused(const bool& aIsFocused)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvSuspendTimeouts()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvResumeTimeouts()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvAsyncScrollDOMEvent(
        const gfxRect& contentRect,
        const gfxSize& scrollSize)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvUpdateFrame(const FrameMetrics& frame)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleDoubleTap(const nsIntPoint& point)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleSingleTap(const nsIntPoint& point)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleLongTap(
        const nsIntPoint& point,
        const ScrollableLayerGuid& aGuid,
        const uint64_t& aInputBlockId)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvAcknowledgeScrollUpdate(
        const ViewID& aScrollId,
        const uint32_t& aScrollGeneration)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleTextEvent(
        const nsString& commit,
        const nsString& preEdit)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleKeyPressEvent(
        const int& domKeyCode,
        const int& gmodifiers,
        const int& charCode)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvHandleKeyReleaseEvent(
                                                     const int& domKeyCode,
                                                     const int& gmodifiers,
                                                     const int& charCode)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvMouseEvent(
                                          const nsString& aType,
                                          const float& aX,
                                          const float& aY,
                                          const int32_t& aButton,
                                          const int32_t& aClickCount,
                                          const int32_t& aModifiers,
                                          const bool& aIgnoreRootScrollFrame)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvInputDataTouchEvent(
                                                   const ScrollableLayerGuid& aGuid,
                                                   const MultiTouchInput& event,
                                                   const uint64_t& aInputBlockId)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvInputDataTouchMoveEvent(
                                                       const ScrollableLayerGuid& aGuid,
                                                       const MultiTouchInput& event,
                                                       const uint64_t& aInputBlockId)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvAddMessageListener(const nsCString& name)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvRemoveMessageListener(const nsCString& name)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvAddMessageListeners(const nsTArray<nsString>& messageNames)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvRemoveMessageListeners(const nsTArray<nsString>& messageNames)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvDestroy()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessChild::RecvAsyncMessage(const nsString& aMessage,
                                            const nsString& aData)
{
    LOGT();
    return false;
}

}  // namespace embedlite
}  // namespace mozilla
