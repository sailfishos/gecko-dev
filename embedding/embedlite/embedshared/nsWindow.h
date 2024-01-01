/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EmbedLite_nsWindow_h__
#define mozilla_widget_EmbedLite_nsWindow_h__

#include "PuppetWidgetBase.h"

#include "mozilla/WidgetUtils.h"           // for InputContext
#include <list>

namespace mozilla {

namespace gl {
class GLContext;
}

namespace embedlite {

class EmbedLiteWindowChild;
class EmbedLiteWindowListener;
class EmbedContentController;

class nsWindow : public PuppetWidgetBase
{
public:
  nsWindow(EmbedLiteWindowChild* window);

  NS_DECL_ISUPPORTS_INHERITED


  using PuppetWidgetBase::Create; // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(nsIWidget*        aParent,
                                        nsNativeWidget    aNativeParent,
                                        const LayoutDeviceIntRect& aRect,
                                        nsWidgetInitData* aInitData = nullptr) override;

  virtual void Destroy() override;
  virtual void Show(bool aState) override;
  virtual void Resize(double aWidth,
                      double aHeight,
                      bool aRepaint) override;

  virtual nsresult  DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                  nsEventStatus& aStatus) override;

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;

  virtual LayoutDeviceIntRect GetNaturalBounds() override;

  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual LayerManager *GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

#if 0
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
#endif

  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) override;
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext) override;

  void AddObserver(EmbedLitePuppetWidgetObserver* aObserver);
  void RemoveObserver(EmbedLitePuppetWidgetObserver* aObserver);

  uint32_t GetUniqueID() const;
  layers::LayersId GetRootLayerId() const;

  void Activate(EmbedContentController* aController);
  void Deactivate(EmbedContentController* aController);
  RefPtr<mozilla::layers::IAPZCTreeManager> GetAPZCTreeManager();
  void SetFirstViewCreated() { mFirstViewCreated = true; }
  bool IsFirstViewCreated() const { return mFirstViewCreated; }

protected:
  virtual ~nsWindow() override;

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController() override;

  virtual bool UseExternalCompositingSurface() const override;

  const char *Type() const override;

  CompositorBridgeParent* GetCompositorBridgeParent() const;

private:
  nsWindow();
  mozilla::gl::GLContext* GetGLContext() const;
  nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);

  static void CreateGLContextEarly(EmbedLiteWindowListener *aListener);

  bool mFirstViewCreated;
  EmbedLiteWindowChild* mWindow; // Not owned, can be null.
  InputContext mInputContext;

  typedef std::list<EmbedContentController *> ControllerList;
  ControllerList mControllers;

  friend already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow();
  friend already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow();
};

}  // namespace embedlite
}  // namespace mozilla

#endif // mozilla_widget_EmbedLite_nsWindow_h__
