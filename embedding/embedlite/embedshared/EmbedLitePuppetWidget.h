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
#include "nsRect.h"

namespace mozilla {

namespace embedlite {

class EmbedLiteWindowBaseChild;

class EmbedLitePuppetWidget : public PuppetWidgetBase
{
public:
  EmbedLitePuppetWidget(EmbedLiteViewChildIface* view);

  NS_DECL_ISUPPORTS_INHERITED

  virtual already_AddRefed<nsIWidget>
  CreateChild(const LayoutDeviceIntRect&  aRect,
              nsWidgetInitData* aInitData = nullptr,
              bool              aForceUseIWidgetParent = false) override;

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
                             const FrameMetrics::ViewID& aViewId,
                             const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual LayerManager *GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  bool DoSendContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid& aGuid,
                                       uint64_t aInputBlockId,
                                       bool aPreventDefault);
  bool DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId,
                                     const nsTArray<mozilla::layers::TouchBehaviorFlags>& aFlags);

protected:
  virtual ~EmbedLitePuppetWidget() override;
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

  InputContext mInputContext;
  NativeIMEContext mNativeIMEContext;

  bool mIMEComposing;
  nsString mIMEComposingText;

  float mDPI;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EmbedLitePuppetWidget_h__
