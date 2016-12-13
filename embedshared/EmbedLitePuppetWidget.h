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
  virtual void WidgetBoundsChanged(const nsIntRect&) {};
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

  NS_IMETHOD Create(nsIWidget*        aParent,
                    nsNativeWidget    aNativeParent,
                    const LayoutDeviceIntRect& aRect,
                    nsWidgetInitData* aInitData = nullptr) override;

  virtual already_AddRefed<nsIWidget>
  CreateChild(const LayoutDeviceIntRect&  aRect,
              nsWidgetInitData* aInitData = nullptr,
              bool              aForceUseIWidgetParent = false) override;

  NS_IMETHOD Destroy();

  NS_IMETHOD Show(bool aState) override;
  virtual bool IsVisible() const override {
    return mVisible;
  }
  NS_IMETHOD ConstrainPosition(bool     /*ignored aAllowSlop*/,
                               int32_t* aX,
                               int32_t* aY) override {
    *aX = kMaxDimension;
    *aY = kMaxDimension;
    LOGNI();
    return NS_OK;
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
  NS_IMETHOD CaptureRollupEvents(nsIRollupListener* aListener,
                                 bool aDoCapture) override {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;

  NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent) override {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }

  virtual LayoutDeviceIntRect GetNaturalBounds() override;
  virtual bool NeedsPaint() override;

  virtual LayerManager*
  GetLayerManager(PLayerTransactionChild* aShadowManager,
                  LayersBackend aBackendHint,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                  bool* aAllowRetaining = nullptr) override;

  virtual mozilla::layers::CompositorParent* NewCompositorParent(int aSurfaceWidth,
                                                                 int aSurfaceHeight) override;
  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual float GetDPI() override;

  virtual bool AsyncPanZoomEnabled() const override;

  /**
   * Called before the LayerManager draws the layer tree.
   *
   * Always called from the compositing thread. Puppet Widget passes the call
   * forward to the EmbedLiteCompositorParent.
   */
  virtual void DrawWindowUnderlay(LayerManagerComposite* aManager, LayoutDeviceIntRect aRect) override;

  /**
   * Called after the LayerManager draws the layer tree
   *
   * Always called from the compositing thread. Puppet Widget passes the call
   * forward to the EmbedLiteCompositorParent.
   */
  virtual void DrawWindowOverlay(LayerManagerComposite* aManager, LayoutDeviceIntRect aRect) override;

  virtual bool PreRender(LayerManagerComposite* aManager) override;
  virtual void PostRender(LayerManagerComposite* aManager) override;

  NS_IMETHOD         SetParent(nsIWidget* aNewParent) override;
  virtual nsIWidget* GetParent(void) override;

  void SetRotation(mozilla::ScreenRotation);
  void SetMargins(const nsIntMargin& marins);
  void UpdateSize();
  void SetActive(bool active);

  void AddObserver(EmbedLitePuppetWidgetObserver*);
  void RemoveObserver(EmbedLitePuppetWidgetObserver*);

  static void DumpWidgetTree();
  static void DumpWidgetTree(const nsTArray<EmbedLitePuppetWidget*>&, int indent = 0);
  static void LogWidget(EmbedLitePuppetWidget *widget, int index, int indent);

protected:
  EmbedLitePuppetWidget(EmbedLiteWindowBaseChild*, EmbedLiteViewChildIface*);
  virtual ~EmbedLitePuppetWidget() override;
  EmbedLiteViewChildIface* GetEmbedLiteChildView() const;

private:
  typedef nsTArray<EmbedLitePuppetWidget*> ChildrenArray;
  typedef nsTArray<EmbedLitePuppetWidgetObserver*> ObserverArray;

  mozilla::gl::GLContext* GetGLContext() const;
  static void CreateGLContextEarly(uint32_t aWindowId);

  EmbedLitePuppetWidget* TopWindow();
  bool IsTopLevel();
  void RemoveIMEComposition();

  EmbedLiteWindowBaseChild* mWindow; // Not owned, can be null.
  EmbedLiteViewChildIface* mView; // Not owned, can be null.

  bool mVisible;
  bool mEnabled;
  bool mActive;
  bool mHasCompositor;
  InputContext mInputContext;
  bool mIMEComposing;
  nsString mIMEComposingText;
  ChildrenArray mChildren;
  EmbedLitePuppetWidget* mParent;
  mozilla::ScreenRotation mRotation;
  nsIntRect mNaturalBounds;
  nsIntMargin mMargins;
  ObserverArray mObservers;
  float mDPI;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EmbedLitePuppetWidget_h__
