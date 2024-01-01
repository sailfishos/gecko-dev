
#ifndef __gen_EmbedLiteViewChildIface_h__
#define __gen_EmbedLiteViewChildIface_h__

#include "mozilla/layers/APZUtils.h"    // for TouchBehaviorFlags
#include "mozilla/TouchEvents.h"         // for WidgetTouchEvent

class nsIWebNavigation;
class nsIWidget;
class nsIWebBrowserChrome;
class nsIWebBrowser;
namespace mozilla {

namespace layers {
struct FrameMetrics;
struct ZoomTarget;
} // namespace layers

namespace embedlite {
class EmbedLiteViewChildIface
{
public:
/*-------------Widget-------------------*/
  virtual bool
  SetInputContext(const int32_t& IMEEnabled,
                  const int32_t& IMEOpen,
                  const nsString& type,
                  const nsString& inputmode,
                  const nsString& actionHint,
                  const int32_t& cause,
                  const int32_t& focusChange) = 0;

  virtual bool
  GetInputContext(int32_t* IMEEnabled,
                  int32_t* IMEOpen) = 0;
  virtual void ResetInputState() = 0;

/*-------------TabChild-------------------*/

  virtual bool
  ZoomToRect(const uint32_t& aPresShellId,
             const mozilla::layers::ScrollableLayerGuid::ViewID &aViewId,
             const mozilla::layers::ZoomTarget& aRect) = 0;

  virtual bool
  SetTargetAPZC(uint64_t aInputBlockId,
                const nsTArray<mozilla::layers::ScrollableLayerGuid>& aTargets) = 0;

  virtual bool
  UpdateZoomConstraints(const uint32_t& aPresShellId,
                        const mozilla::layers::ScrollableLayerGuid::ViewID &aViewId,
                        const Maybe<mozilla::layers::ZoomConstraints>& aConstraints) = 0;

  virtual bool HasMessageListener(const nsAString& aMessageName) = 0;

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage) = 0;
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 nsTArray<nsString>* aJSONRetVal) = 0;
  virtual bool GetDPI(float* aDPI) = 0;

  /**
   * Relay given frame metrics to listeners subscribed via EmbedLiteAppService
   */
  virtual void RelayFrameMetrics(const mozilla::layers::FrameMetrics& aFrameMetrics) = 0;

  virtual nsIWidget* WebWidget() = 0;
/*----------------------WindowCreator-------------------------*/
  virtual uint32_t GetID() = 0;
  virtual nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome) = 0;
  virtual nsresult GetBrowser(nsIWebBrowser** outBrowser) = 0;
  virtual uint64_t GetOuterID() = 0;

  virtual bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::ScrollableLayerGuid::ViewID *aViewId) = 0;
  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMessage, const nsString& aData) = 0;
  virtual bool ContentReceivedInputBlock(const uint64_t &aInputBlockId, const bool &aPreventDefault) = 0;

  virtual bool DoSendContentReceivedInputBlock(uint64_t aInputBlockId,
                                               bool aPreventDefault) = 0;

  virtual bool DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags) = 0;
};

}}

#endif // __gen_EmbedLiteViewChildIface_h__
