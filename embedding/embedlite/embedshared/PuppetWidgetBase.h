/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedlite_PuppetWidgetBase_h__
#define mozilla_embedlite_PuppetWidgetBase_h__

#include "nsIWidget.h"
#include "nsThreadUtils.h"

namespace mozilla {

namespace embedlite {

class EmbedLitePuppetWidgetObserver
{
public:
  virtual void WidgetBoundsChanged(const LayoutDeviceIntRect&) {};
  virtual void WidgetRotationChanged(const mozilla::ScreenRotation&) {};
};

class PuppetWidgetBase : public nsIWidget
{
  typedef nsIWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  PuppetWidgetBase();

  NS_DECL_ISUPPORTS_INHERITED

  using nsIWidget::Create; // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(nsIWidget*        aParent,
                                       const LayoutDeviceIntRect& aRect,
                                       const widget::InitData& aInitData) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;

  virtual bool IsVisible() const override;

  virtual void ConstrainPosition(DesktopIntPoint& aPoint) override;

  virtual void Move(const DesktopPoint& aPoint) override;

  virtual void Resize(const DesktopSize& aSize, bool aRepaint) override;
  virtual void Resize(const DesktopRect& aRect, bool aRepaint) override;

  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;

  virtual nsSizeMode SizeMode() override { return mSizeMode; }
  virtual void SetSizeMode(nsSizeMode aMode) override { mSizeMode = aMode; }

  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual nsresult SetTitle(const nsAString& aTitle) override;

  virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual LayoutDeviceIntRect GetBounds() override { return mBounds; }
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;

  virtual void CaptureRollupEvents(bool aDoCapture) override;

  void SetRotation(mozilla::ScreenRotation);
  void SetMargins(const LayoutDeviceIntMargin& margins);
  void UpdateBounds(bool aRepaint);
  virtual mozilla::LayoutDeviceIntMargin GetSafeAreaInsets() const override;
  void SetSafeAreaInsets(
      const mozilla::LayoutDeviceIntMargin& aSafeAreaInsets);
  void SetSize(double aWidth, double aHeight);
  void SetActive(bool active);
  void NotifyBackingScaleFactorChanged();

  virtual WindowRenderer* GetWindowRenderer() override;

  static void DumpWidgetTree();
  static void DumpWidgetTree(const nsTArray<PuppetWidgetBase *> &widgets, int indent = 0);
  static void LogWidget(PuppetWidgetBase *widget, int index, int indent);

protected:
  virtual ~PuppetWidgetBase() override;

  void DidClearParent(nsIWidget* aOldParent) override;

  typedef nsTArray<PuppetWidgetBase*> ChildrenArray;
  typedef nsTArray<EmbedLitePuppetWidgetObserver*> ObserverArray;

  bool WillShow(bool aState);

  virtual const char *Type() const = 0;

  bool mVisible;
  bool mEnabled;
  bool mActive;

  ChildrenArray mChildren;
  ObserverArray mObservers;

  mozilla::ScreenRotation mRotation;
  LayoutDeviceIntRect mBounds;
  LayoutDeviceIntRect mNaturalBounds;
  LayoutDeviceIntMargin mMargins;
  mozilla::LayoutDeviceIntMargin mSafeAreaInsets;

private:
  class WidgetPaintTask final : public Runnable
  {
  public:
    NS_DECL_NSIRUNNABLE

    explicit WidgetPaintTask(PuppetWidgetBase* aWidget)
      : Runnable("PuppetWidgetBase::WidgetPaintTask")
      , mWidget(aWidget)
    {
    }

    void Revoke() { mWidget = nullptr; }

  private:
    PuppetWidgetBase* mWidget;
  };

  void Paint();
  bool IsTopLevel();
  nsRevocableEventPtr<WidgetPaintTask> mWidgetPaintTask;
  nsSizeMode mSizeMode;
};

}  // namespace embedlite
}  // namespace mozilla

#endif // mozilla_embedlite_PuppetWidgetBase_h__
