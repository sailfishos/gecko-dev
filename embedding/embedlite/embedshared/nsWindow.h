/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EmbedLite_nsWindow_h__
#define mozilla_widget_EmbedLite_nsWindow_h__

#include "PuppetWidgetBase.h"

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WidgetUtils.h"           // for InputContext
#include "nsStringFwd.h"

namespace mozilla {

namespace widget {
class TextEventDispatcher;
}

namespace embedlite {

class EmbedLiteHostedWindow;
class EmbedLitePuppetWidget;
class EmbedLiteChromeInputTransactionListener;
class nsWindow;

class MOZ_RAII AutoEmbedLiteChromeWindowHost final
{
public:
  explicit AutoEmbedLiteChromeWindowHost(nsWindow* aHost);
  ~AutoEmbedLiteChromeWindowHost();

  bool IsValid() const;
  bool WasConsumed() const { return mConsumed; }

private:
  static already_AddRefed<nsIWidget> ConsumePending();
  already_AddRefed<nsIWidget> Consume();

  static AutoEmbedLiteChromeWindowHost* sPendingHost;

  RefPtr<nsWindow> mHost;
  bool mConsumed;

  friend already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow();
};

class nsWindow : public PuppetWidgetBase
{
public:
  nsWindow(EmbedLiteHostedWindow* window);

  NS_DECL_ISUPPORTS_INHERITED


  using PuppetWidgetBase::Create; // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(nsIWidget*        aParent,
                                        const LayoutDeviceIntRect& aRect,
                                        const widget::InitData& aInitData) override;

  virtual void Destroy() override;
  virtual void Show(bool aState) override;
  virtual void Resize(const DesktopSize& aSize, bool aRepaint) override;

  virtual nsEventStatus DispatchEvent(
      mozilla::WidgetGUIEvent* aEvent) override;

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;

  virtual LayoutDeviceIntRect GetNaturalBounds() override;
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;
  void BackingScaleFactorChanged();

  virtual void CreateCompositor() override;
  virtual void CreateCompositor(int aWidth, int aHeight) override;

  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual WindowRenderer* GetWindowRenderer() override;
  void ScheduleWebRenderComposite();

  virtual bool PreRender(mozilla::widget::WidgetRenderingContext* aContext) override;
  virtual void PostRender(mozilla::widget::WidgetRenderingContext* aContext) override;

  void AddObserver(EmbedLitePuppetWidgetObserver* aObserver);
  void RemoveObserver(EmbedLitePuppetWidgetObserver* aObserver);

  uint32_t GetUniqueID() const;
  layers::LayersId GetRootLayerId() const;

  RefPtr<mozilla::layers::IAPZCTreeManager> GetAPZCTreeManager();
  void AttachChromeHostedWidget(EmbedLitePuppetWidget* aWidget);
  void DetachChromeHostedWidget(EmbedLitePuppetWidget* aWidget);
  void InitializeChromeInput();
  bool DispatchChromeInputEvent(WidgetInputEvent* aEvent);
  bool SetChromeMargins(const LayoutDeviceIntMargin& aMargins);
  bool SetChromeSafeAreaInsets(const LayoutDeviceIntMargin& aInsets);
  bool DispatchChromeTextEvent(const nsAString& aCommit,
                               const nsAString& aPreEdit,
                               int32_t aReplacementStart,
                               int32_t aReplacementLength);
  bool DispatchChromeTextEventAtOffset(const nsAString& aCommit,
                                       const nsAString& aPreEdit,
                                       uint32_t aReplacementOffset,
                                       int32_t aReplacementLength);
  bool DispatchChromeKeyPress(int32_t aDomKeyCode, int32_t aModifiers,
                              int32_t aCharCode);
  bool DispatchChromeKeyRelease(int32_t aDomKeyCode, int32_t aModifiers,
                                int32_t aCharCode);
  bool SetChromeFocused(bool aFocused);
  void SetChromeInputContext(const InputContext& aContext,
                             const InputContextAction& aAction);
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
  bool DispatchChromeTextEventInternal(const nsAString& aCommit,
                                       const nsAString& aPreEdit,
                                       int32_t aReplacementStart,
                                       uint32_t aReplacementOffset,
                                       int32_t aReplacementLength,
                                       bool aUseReplacementOffset);
  void ConfigureChromeAPZ();
  void EndChromeInputTransaction();
  bool mFirstViewCreated;
  bool mChromeInputReady;
  bool mChromeWindowFocused;
  EmbedLiteHostedWindow* mWindow; // Not owned, can be null.
  EmbedLitePuppetWidget* mChromeHostedWidget; // Not owned.
  RefPtr<EmbedLiteChromeInputTransactionListener>
    mChromeInputTransactionListener;
  InputContext mInputContext;

  friend already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow();
  friend already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow();
};

}  // namespace embedlite
}  // namespace mozilla

#endif // mozilla_widget_EmbedLite_nsWindow_h__
