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

namespace mozilla {

namespace gl {
class GLContext;
}

namespace embedlite {

class EmbedLiteWindowBaseChild;

class nsWindow : public PuppetWidgetBase
{
public:
  nsWindow(EmbedLiteWindowBaseChild* window);

  NS_DECL_ISUPPORTS_INHERITED


  using PuppetWidgetBase::Create; // for Create signature not overridden here
  virtual MOZ_MUST_USE nsresult Create(nsIWidget*        aParent,
                                       nsNativeWidget    aNativeParent,
                                       const LayoutDeviceIntRect& aRect,
                                       nsWidgetInitData* aInitData = nullptr) override;

  virtual void Destroy() override;

  NS_IMETHOD Show(bool aState) override;

  NS_IMETHOD Resize(double aWidth,
                    double aHeight,
                    bool   aRepaint) override;

  NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus& aStatus) override;

  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;

  virtual LayoutDeviceIntRect GetNaturalBounds() override;

  // TODO: Re-write this
  virtual mozilla::layers::CompositorBridgeParent* NewCompositorParent(int aSurfaceWidth,
                                                                       int aSurfaceHeight);
  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual LayerManager *GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

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

  void AddObserver(EmbedLitePuppetWidgetObserver* aObserver);
  void RemoveObserver(EmbedLitePuppetWidgetObserver* aObserver);

protected:
  virtual ~nsWindow() override;

  virtual void ConfigureAPZCTreeManager();
  virtual void ConfigureAPZControllerThread();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController() override;

  const char *Type() const override;

private:
  nsWindow();
  mozilla::gl::GLContext* GetGLContext() const;
  nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);

  static void CreateGLContextEarly(uint32_t aWindowId);

  EmbedLiteWindowBaseChild* mWindow; // Not owned, can be null.
  InputContext mInputContext;

  bool mHasCompositor;
};

}  // namespace embedlite
}  // namespace mozilla

#endif // mozilla_widget_EmbedLite_nsWindow_h__
