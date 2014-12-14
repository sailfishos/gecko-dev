#include "EmbedLiteViewProcessParent.h"

#include "EmbedLog.h"

namespace mozilla {
namespace embedlite {

MOZ_IMPLICIT EmbedLiteViewProcessParent::EmbedLiteViewProcessParent(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
  : mView(nullptr)
{
    LOGT();
    MOZ_COUNT_CTOR(EmbedLiteViewProcessParent);
}

MOZ_IMPLICIT EmbedLiteViewProcessParent::~EmbedLiteViewProcessParent()
{
    LOGT();
    MOZ_COUNT_DTOR(EmbedLiteViewProcessParent);
}

bool
EmbedLiteViewProcessParent::RecvInitialized()
{
    LOGT();
    NS_ENSURE_TRUE(mView, false);
    mView->GetListener()->ViewInitialized();
    return true;
}

bool
EmbedLiteViewProcessParent::RecvOnLocationChanged(
        const nsCString& aLocation,
        const bool& aCanGoBack,
        const bool& aCanGoForward)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnLoadStarted(const nsCString& aLocation)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnLoadFinished()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnLoadRedirect()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnLoadProgress(
        const int32_t& aProgress,
        const int32_t& aCurTotal,
        const int32_t& aMaxTotal)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnSecurityChanged(
        const nsCString& aStatus,
        const uint32_t& aState)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnFirstPaint(
        const int32_t& aX,
        const int32_t& aY)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnScrolledAreaChanged(
        const uint32_t& aWidth,
        const uint32_t& aHeight)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnScrollChanged(
        const int32_t& offSetX,
        const int32_t& offSetY)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnTitleChanged(const nsString& aTitle)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvOnWindowCloseRequested()
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvUpdateZoomConstraints(
        const uint32_t& aPresShellId,
        const ViewID& aViewId,
        const bool& aIsRoot,
        const ZoomConstraints& aConstraints)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvZoomToRect(
        const uint32_t& aPresShellId,
        const ViewID& aViewId,
        const CSSRect& aRect)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvSetBackgroundColor(const nscolor& color)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvContentReceivedTouch(
        const ScrollableLayerGuid& aGuid,
        const uint64_t& aInputBlockId,
        const bool& aPreventDefault)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvGetGLViewSize(gfxSize* aSize)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvSyncMessage(
        const nsString& aMessage,
        const nsString& aJSON,
        nsTArray<nsString>* retval)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvRpcMessage(
        const nsString& aMessage,
        const nsString& aJSON,
        nsTArray<nsString>* retval)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvGetInputContext(
        int32_t* IMEEnabled,
        int32_t* IMEOpen,
        intptr_t* NativeIMEContext)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvSetInputContext(
        const int32_t& IMEEnabled,
        const int32_t& IMEOpen,
        const nsString& type,
        const nsString& inputmode,
        const nsString& actionHint,
        const int32_t& cause,
        const int32_t& focusChange)
{
    LOGT();
    return false;
}

bool
EmbedLiteViewProcessParent::RecvAsyncMessage(
        const nsString& aMessage,
        const nsString& aData)
{
    LOGT();
    return false;
}

void
EmbedLiteViewProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
    LOGT();
}

/* void RenderToImage (in buffer aData, in int32_t aWidth, in int32_t aHeigth, in int32_t aStride, in int32_t aDepth); */
NS_IMETHODIMP EmbedLiteViewProcessParent::RenderToImage(unsigned char *aData, int32_t aWidth, int32_t aHeigth, int32_t aStride, int32_t aDepth)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetViewSize (in int32_t aWidth, in int32_t aHeight); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SetViewSize(int32_t aWidth, int32_t aHeight)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetGLViewPortSize (in int32_t aWidth, in int32_t aHeight); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SetGLViewPortSize(int32_t aWidth, int32_t aHeight)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ReceiveInputEvent ([const] in InputData aEvent); */
NS_IMETHODIMP EmbedLiteViewProcessParent::ReceiveInputEvent(const mozilla::InputData & aEvent)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void TextEvent (in string aComposite, in string aPreEdit); */
NS_IMETHODIMP EmbedLiteViewProcessParent::TextEvent(const char * aComposite, const char * aPreEdit)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SendKeyPress (in int32_t aDomKeyCode, in int32_t aModifiers, in int32_t aCharCode); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SendKeyPress(int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SendKeyRelease (in int32_t aDomKeyCode, in int32_t aModifiers, in int32_t aCharCode); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SendKeyRelease(int32_t aDomKeyCode, int32_t aModifiers, int32_t aCharCode)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void MousePress (in int32_t aX, in int32_t aY, in int32_t aTime, in uint32_t aButtons, in uint32_t aModifiers); */
NS_IMETHODIMP EmbedLiteViewProcessParent::MousePress(int32_t aX, int32_t aY, int32_t aTime, uint32_t aButtons, uint32_t aModifiers)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void MouseRelease (in int32_t aX, in int32_t aY, in int32_t aTime, in uint32_t aButtons, in uint32_t aModifiers); */
NS_IMETHODIMP EmbedLiteViewProcessParent::MouseRelease(int32_t aX, int32_t aY, int32_t aTime, uint32_t aButtons, uint32_t aModifiers)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void MouseMove (in int32_t aX, in int32_t aY, in int32_t aTime, in uint32_t aButtons, in uint32_t aModifiers); */
NS_IMETHODIMP EmbedLiteViewProcessParent::MouseMove(int32_t aX, int32_t aY, int32_t aTime, uint32_t aButtons, uint32_t aModifiers)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void UpdateScrollController (); */
NS_IMETHODIMP EmbedLiteViewProcessParent::UpdateScrollController()
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ViewAPIDestroyed (); */
NS_IMETHODIMP EmbedLiteViewProcessParent::ViewAPIDestroyed()
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetUniqueID (out uint32_t aId); */
NS_IMETHODIMP EmbedLiteViewProcessParent::GetUniqueID(uint32_t *aId)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetPlatformImage (out PlatformImage aImage, out int32_t aWidth, out int32_t aHeight); */
NS_IMETHODIMP EmbedLiteViewProcessParent::GetPlatformImage(void **aImage, int32_t *aWidth, int32_t *aHeight)
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SuspendRendering (); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SuspendRendering()
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ResumeRendering (); */
NS_IMETHODIMP EmbedLiteViewProcessParent::ResumeRendering()
{
    LOGT();
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetEmbedAPIView (in EmbedLiteView aView); */
NS_IMETHODIMP EmbedLiteViewProcessParent::SetEmbedAPIView(mozilla::embedlite::EmbedLiteView *aView)
{
    LOGT();
    mView = aView;
    return NS_OK;
}

}  // namespace embedlite
}  // namespace mozilla
