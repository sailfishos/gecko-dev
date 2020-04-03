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
#include "nsBaseWidget.h"
#include "nsCOMArray.h"
#include "nsRect.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace embedlite {

class EmbedLitePuppetWidgetObserver
{
public:
  virtual void WidgetBoundsChanged(const LayoutDeviceIntRect&) {};
  virtual void WidgetRotationChanged(const mozilla::ScreenRotation&) {};
};

class EmbedLiteWindowBaseChild;

class EmbedLitePuppetWidget : public nsBaseWidget
{
  typedef nsBaseWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  EmbedLitePuppetWidget(EmbedLiteWindowBaseChild* window);
  EmbedLitePuppetWidget(EmbedLiteViewChildIface* view);

  NS_DECL_ISUPPORTS_INHERITED

  using nsBaseWidget::Create; // for Create signature not overridden here
  virtual MOZ_MUST_USE nsresult Create(nsIWidget*        aParent,
                                       nsNativeWidget    aNativeParent,
                                       const LayoutDeviceIntRect& aRect,
                                       nsWidgetInitData* aInitData = nullptr) override;

  virtual already_AddRefed<nsIWidget>
  CreateChild(const LayoutDeviceIntRect&  aRect,
              nsWidgetInitData* aInitData = nullptr,
              bool              aForceUseIWidgetParent = false) override;

  virtual void Destroy() override;

  NS_IMETHOD Show(bool aState) override;
  virtual bool IsVisible() const override {
    return mVisible;
  }
  virtual void ConstrainPosition(bool     /*ignored aAllowSlop*/,
                                 int32_t* aX,
                                 int32_t* aY) override {
    *aX = kMaxDimension;
    *aY = kMaxDimension;
    LOGNI();
  }
  // We're always at <0, 0>, and so ignore move requests.
  NS_IMETHOD Move(double aX, double aY) override {
    LOGNI();
    return NS_OK;
  }
  NS_IMETHOD Resize(double aWidth,
                    double aHeight,
                    bool   aRepaint) override;
  NS_IMETHOD Resize(double aX,
                    double aY,
                    double aWidth,
                    double aHeight,
                    bool   aRepaint) override
  // (we're always at <0, 0>)
  {
    return Resize(aWidth, aHeight, aRepaint);
  }
  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  NS_IMETHOD Enable(bool aState) override {
    LOGNI();
    mEnabled = aState;
    return NS_OK;
  }
  virtual bool IsEnabled() const override {
    LOGNI();
    return mEnabled;
  }
  NS_IMETHOD SetFocus(bool aRaise = false) override ;
  // PuppetWidgets don't care about children.
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override {
    LOGNI();
    return NS_OK;
  }
  NS_IMETHOD Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual void* GetNativeData(uint32_t aDataType) override;
  // PuppetWidgets don't have any concept of titles..
  NS_IMETHOD SetTitle(const nsAString& aTitle) override {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }
  // PuppetWidgets are always at <0, 0>.
  virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() override {
    LOGF();
    return LayoutDeviceIntPoint(0, 0);
  }
  NS_IMETHOD DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus) override;
  virtual void CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) override {
    LOGNI();
  }
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;
  NS_IMETHOD_(NativeIMEContext) GetNativeIMEContext() override;
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() override;

  virtual void ReparentNativeWidget(nsIWidget* aNewParent) override {
    (void)aNewParent;
    LOGNI();
  }

  virtual LayoutDeviceIntRect GetNaturalBounds() override;
  virtual bool NeedsPaint() override;

  virtual LayerManager *GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  // TODO: Re-write this
  virtual mozilla::layers::CompositorBridgeParent* NewCompositorParent(int aSurfaceWidth,
                                                                       int aSurfaceHeight);
  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual float GetDPI() override;

  virtual bool AsyncPanZoomEnabled() const override;

  virtual void SetConfirmedTargetAPZC(uint64_t aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) const override;

  virtual void UpdateZoomConstraints(const uint32_t& aPresShellId,
                             const FrameMetrics::ViewID& aViewId,
                             const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  /**
   * Called before the LayerManager draws the layer tree.
   *
   * Always called from the compositing thread. Puppet Widget passes the call
   * forward to the EmbedLiteCompositorBridgeParent.
   */
  virtual void DrawWindowUnderlay(mozilla::widget::WidgetRenderingContext* aContext, LayoutDeviceIntRect aRect) override;

  /**
   * Called after the LayerManager draws the layer tree
   *
   * Always called from the compositing thread. Puppet Widget passes the call
   * forward to the EmbedLiteCompositorBridgeParent.
   */
  virtual void DrawWindowOverlay(mozilla::widget::WidgetRenderingContext* aContext, LayoutDeviceIntRect aRect) override;

  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) override;
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext) override;

  NS_IMETHOD         SetParent(nsIWidget* aNewParent) override;
  virtual nsIWidget* GetParent(void) override;

  void SetRotation(mozilla::ScreenRotation);
  void SetMargins(const LayoutDeviceIntMargin& margins);
  void UpdateSize();
  void SetActive(bool active);

  void AddObserver(EmbedLitePuppetWidgetObserver*);
  void RemoveObserver(EmbedLitePuppetWidgetObserver*);

  static void DumpWidgetTree();
  static void DumpWidgetTree(const nsTArray<EmbedLitePuppetWidget*>&, int indent = 0);
  static void LogWidget(EmbedLitePuppetWidget *widget, int index, int indent);

  bool DoSendContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid& aGuid,
                                       uint64_t aInputBlockId,
                                       bool aPreventDefault);
  bool DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId,
                                     const nsTArray<mozilla::layers::TouchBehaviorFlags>& aFlags);

protected:
  EmbedLitePuppetWidget(EmbedLiteWindowBaseChild*, EmbedLiteViewChildIface*);
  virtual ~EmbedLitePuppetWidget() override;
  EmbedLiteViewChildIface* GetEmbedLiteChildView() const;

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController() override;

private:
  EmbedLitePuppetWidget();
  typedef nsTArray<EmbedLitePuppetWidget*> ChildrenArray;
  typedef nsTArray<EmbedLitePuppetWidgetObserver*> ObserverArray;

  mozilla::gl::GLContext* GetGLContext() const;
  static void CreateGLContextEarly(uint32_t aWindowId);

  bool IsTopLevel();
  void RemoveIMEComposition();

  EmbedLiteWindowBaseChild* mWindow; // Not owned, can be null.
  EmbedLiteViewChildIface* mView; // Not owned, can be null.

  bool mVisible;
  bool mEnabled;
  bool mActive;
  bool mHasCompositor;
  InputContext mInputContext;
  NativeIMEContext mNativeIMEContext;

  bool mIMEComposing;
  nsString mIMEComposingText;
  ChildrenArray mChildren;
  EmbedLitePuppetWidget* mParent;
  mozilla::ScreenRotation mRotation;
  LayoutDeviceIntRect mNaturalBounds;
  LayoutDeviceIntMargin mMargins;
  ObserverArray mObservers;
  float mDPI;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EmbedLitePuppetWidget_h__
