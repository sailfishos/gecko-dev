/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This "puppet widget" isn't really a platform widget.  It's intended
 * to be used in widgetless rendering contexts, such as sandboxed
 * content processes.  If any "real" widgetry is needed, the request
 * is forwarded to and/or data received from elsewhere.
 */

#ifndef mozilla_widget_EmbedLitePuppetWidget_h__
#define mozilla_widget_EmbedLitePuppetWidget_h__

#include "EmbedLog.h"

#include "EmbedLiteViewChildIface.h"
#include "mozilla/Attributes.h"
#include "mozilla/WidgetUtils.h"
#include "PuppetWidgetBase.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsRect.h"

namespace mozilla {

namespace embedlite {

class EmbedLiteWindowChild;

class EmbedLitePuppetWidget : public PuppetWidgetBase
{
public:
  EmbedLitePuppetWidget(EmbedLiteViewChildIface* view);

  static already_AddRefed<nsIWidget> CreateForChromeHost(nsIWidget* aHost);

  NS_DECL_ISUPPORTS_INHERITED

  using PuppetWidgetBase::Create;
  [[nodiscard]] nsresult Create(nsIWidget* aParent,
                                const LayoutDeviceIntRect& aRect,
                                widget::InitData* aInitData = nullptr) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;

  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual nsresult DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus) override;

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual NativeIMEContext GetNativeIMEContext() override;

  virtual bool NeedsPaint() override;

  virtual float GetDPI() override;

  virtual bool AsyncPanZoomEnabled() const override;

  virtual void SetConfirmedTargetAPZC(uint64_t aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) const override;

  virtual void UpdateZoomConstraints(const uint32_t& aPresShellId,
                             const ScrollableLayerGuid::ViewID &aViewId,
                             const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual WindowRenderer* GetWindowRenderer() override;

  bool DoSendContentReceivedInputBlock(uint64_t aInputBlockId,
                                       bool aPreventDefault);
  bool DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId,
                                     const nsTArray<mozilla::layers::TouchBehaviorFlags>& aFlags);

  void AddObserver(EmbedLitePuppetWidgetObserver *aObserver);
  void RemoveObserver(EmbedLitePuppetWidgetObserver *aObserver);
  void NotifyChromeWindowFocusChanged(bool aFocused);

protected:
  virtual ~EmbedLitePuppetWidget() override;
  already_AddRefed<nsIWidget> AllocateChildPuppetWidget(
      widget::InitData& aInitData) override;
  EmbedLiteViewChildIface* GetEmbedLiteChildView() const;

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController() override;

  const char *Type() const override;

private:
  EmbedLitePuppetWidget();
  void RemoveIMEComposition();
  EmbedLitePuppetWidget *GetParentPuppetWidget() const;

  EmbedLiteViewChildIface* mView; // Not owned, can be null.
  nsCOMPtr<nsIWidget> mPendingChromeHost;

  InputContext mInputContext;
  NativeIMEContext mNativeIMEContext;

  bool mIMEComposing;
  nsString mIMEComposingText;

  float mDPI;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EmbedLitePuppetWidget_h__
