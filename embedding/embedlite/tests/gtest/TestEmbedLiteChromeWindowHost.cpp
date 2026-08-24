/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "embedshared/EmbedLitePuppetWidget.h"
#include "embedshared/nsWindow.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/widget/IMEData.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsTArray.h"
#include "PuppetWidget.h"
#include "WindowRenderer.h"

using mozilla::embedlite::AutoEmbedLiteChromeWindowHost;
using mozilla::embedlite::EmbedLitePuppetWidget;
using mozilla::embedlite::nsWindow;
using mozilla::FallbackRenderer;
using mozilla::LayoutDeviceIntRect;
using mozilla::layers::APZInputBridge;
using mozilla::layers::AsyncDragMetrics;
using mozilla::layers::BrowserGestureResponse;
using mozilla::layers::IAPZCTreeManager;
using mozilla::layers::KeyboardMap;
using mozilla::layers::ScrollableLayerGuid;
using mozilla::layers::TouchBehaviorFlags;
using mozilla::layers::ZoomConstraints;
using mozilla::layers::ZoomTarget;
using mozilla::widget::InitData;
using mozilla::widget::PuppetWidget;
using mozilla::widget::WindowType;

namespace {

class TestEmbedLitePuppetWidget final : public EmbedLitePuppetWidget
{
public:
  TestEmbedLitePuppetWidget()
    : EmbedLitePuppetWidget(nullptr)
  {
    mWindowRenderer = new FallbackRenderer;
  }

private:
  ~TestEmbedLitePuppetWidget() override = default;
};

class RecordingAPZTreeManager final : public IAPZCTreeManager
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecordingAPZTreeManager, override)

  void SetKeyboardMap(const KeyboardMap&) override {}
  void ZoomToRect(const ScrollableLayerGuid&, const ZoomTarget&,
                  uint32_t) override {}
  void ContentReceivedInputBlock(uint64_t, bool) override {}
  void SetTargetAPZC(uint64_t,
                     const nsTArray<ScrollableLayerGuid>&) override {}
  void UpdateZoomConstraints(
      const ScrollableLayerGuid&,
      const mozilla::Maybe<ZoomConstraints>&) override {}
  void SetDPI(float aDPI) override { mDPI = aDPI; }
  void SetAllowedTouchBehavior(
      uint64_t, const nsTArray<TouchBehaviorFlags>&) override {}
  void SetBrowserGestureResponse(
      uint64_t, BrowserGestureResponse) override {}
  void StartScrollbarDrag(
      const ScrollableLayerGuid&, const AsyncDragMetrics&) override {}
  bool StartAutoscroll(
      const ScrollableLayerGuid&, const mozilla::ScreenPoint&) override {
    return false;
  }
  void StopAutoscroll(const ScrollableLayerGuid&) override {}
  void SetLongTapEnabled(bool) override {}
  void NotifyApzAwareListenerAdded(
      const ScrollableLayerGuid&) override {}
  APZInputBridge* InputBridge() override { return nullptr; }

  float mDPI = 0.0f;

private:
  ~RecordingAPZTreeManager() override = default;
};

class TestScaleWindow final : public nsWindow
{
public:
  TestScaleWindow(double aScale, float aDPI)
    : nsWindow(nullptr)
    , mScale(aScale)
    , mDPI(aDPI)
  {
  }

  double GetDefaultScaleInternal() override { return mScale; }
  float GetDPI() override { return mDPI; }
  void SetResolution(double aScale, float aDPI)
  {
    mScale = aScale;
    mDPI = aDPI;
  }
  void SetAPZTreeManager(IAPZCTreeManager* aManager) { mAPZC = aManager; }

private:
  ~TestScaleWindow() override = default;

  double mScale;
  float mDPI;
};

class CountingWidgetListener final : public nsIWidgetListener
{
public:
  void WindowActivated() override { ++mActivatedCount; }
  void WindowDeactivated() override { ++mDeactivatedCount; }
  void PaintWindow(nsIWidget*) override { ++mPaintCount; }
  nsEventStatus HandleEvent(mozilla::WidgetGUIEvent* aEvent, bool) override
  {
    ++mEventCount;
    mMessages.AppendElement(aEvent->mMessage);
    mLastEventWidget = aEvent->mWidget;
    return nsEventStatus_eConsumeDoDefault;
  }

  int mActivatedCount = 0;
  int mDeactivatedCount = 0;
  int mPaintCount = 0;
  int mEventCount = 0;
  nsTArray<mozilla::EventMessage> mMessages;
  nsIWidget* mLastEventWidget = nullptr;
};

} // namespace

TEST(EmbedLiteChromeWindowHostTest, ReservationIsOneShot)
{
  RefPtr<nsWindow> host = new nsWindow(nullptr);
  AutoEmbedLiteChromeWindowHost reservation(host);

  ASSERT_TRUE(reservation.IsValid());

  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  EXPECT_TRUE(reservation.WasConsumed());
  EXPECT_FALSE(reservation.IsValid());
  EXPECT_NE(dynamic_cast<EmbedLitePuppetWidget*>(hosted.get()), nullptr);

  nsCOMPtr<nsIWidget> unhosted = nsIWidget::CreateTopLevelWindow();
  EXPECT_NE(dynamic_cast<nsWindow*>(unhosted.get()), nullptr);
}

TEST(EmbedLiteChromeWindowHostTest, UnusedReservationIsCleared)
{
  RefPtr<nsWindow> host = new nsWindow(nullptr);
  {
    AutoEmbedLiteChromeWindowHost reservation(host);
    ASSERT_TRUE(reservation.IsValid());
  }

  nsCOMPtr<nsIWidget> unhosted = nsIWidget::CreateTopLevelWindow();
  EXPECT_NE(dynamic_cast<nsWindow*>(unhosted.get()), nullptr);
}

TEST(EmbedLiteChromeWindowHostTest, HostedWidgetStartsHidden)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData hostInit;
  hostInit.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, hostInit), NS_OK);
  ASSERT_TRUE(host->IsVisible());

  AutoEmbedLiteChromeWindowHost reservation(host);
  ASSERT_TRUE(reservation.IsValid());

  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);

  InitData hostedInit;
  hostedInit.mWindowType = WindowType::TopLevel;
  ASSERT_EQ(hosted->Create(nullptr, bounds, hostedInit), NS_OK);
  EXPECT_FALSE(hosted->IsVisible());

  hosted->Show(true);
  EXPECT_TRUE(hosted->IsVisible());

  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, HostedDescendantsInheritResolution)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;
  const bool hadScalePref =
    mozilla::Preferences::HasUserValue("layout.css.devPixelsPerPx");
  const float scalePref = mozilla::Preferences::GetFloat(
    "layout.css.devPixelsPerPx", -1.0f);

  RefPtr<TestScaleWindow> host = new TestScaleWindow(3.0, 540.0f);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);

  RefPtr<EmbedLitePuppetWidget> nested =
    new EmbedLitePuppetWidget(nullptr);
  ASSERT_EQ(nested->Create(hosted, bounds, init), NS_OK);

  EXPECT_DOUBLE_EQ(hosted->GetDefaultScale().scale, 3.0);
  EXPECT_FLOAT_EQ(hosted->GetDPI(), 540.0f);
  EXPECT_DOUBLE_EQ(nested->GetDefaultScale().scale, 3.0);
  EXPECT_FLOAT_EQ(nested->GetDPI(), 540.0f);

  host->SetResolution(2.5, 480.0f);
  EXPECT_DOUBLE_EQ(hosted->GetDefaultScale().scale, 2.5);
  EXPECT_FLOAT_EQ(hosted->GetDPI(), 480.0f);
  EXPECT_DOUBLE_EQ(nested->GetDefaultScale().scale, 2.5);
  EXPECT_FLOAT_EQ(nested->GetDPI(), 480.0f);

  host->SetSize(100.0, 200.0);
  host->SetRotation(mozilla::ROTATION_90);
  host->UpdateBounds(false);
  EXPECT_EQ(hosted->GetBounds().Size(),
            mozilla::LayoutDeviceIntSize(200, 100));
  EXPECT_EQ(nested->GetBounds().Size(),
            mozilla::LayoutDeviceIntSize(200, 100));
  EXPECT_EQ(mozilla::Preferences::HasUserValue(
              "layout.css.devPixelsPerPx"),
            hadScalePref);
  EXPECT_FLOAT_EQ(mozilla::Preferences::GetFloat(
                    "layout.css.devPixelsPerPx", -1.0f),
                  scalePref);

  nested->Destroy();
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, UnparentedWidgetUsesScreenFallbacks)
{
  RefPtr<TestEmbedLitePuppetWidget> widget =
    new TestEmbedLitePuppetWidget;

  EXPECT_DOUBLE_EQ(widget->GetDefaultScale().scale,
                   nsIWidget::GetFallbackDefaultScale().scale);
  EXPECT_FLOAT_EQ(widget->GetDPI(), nsIWidget::GetFallbackDPI());

  widget->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, BackingScaleChangeNotifiesAPZ)
{
  RefPtr<TestScaleWindow> host = new TestScaleWindow(3.0, 540.0f);
  RefPtr<RecordingAPZTreeManager> apz = new RecordingAPZTreeManager;
  host->SetAPZTreeManager(apz);

  host->BackingScaleFactorChanged();
  EXPECT_FLOAT_EQ(apz->mDPI, 540.0f);

  host->SetResolution(2.5, 480.0f);
  host->BackingScaleFactorChanged();
  EXPECT_FLOAT_EQ(apz->mDPI, 480.0f);

  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, RemotePuppetCachesResolution)
{
  RefPtr<PuppetWidget> remote = new PuppetWidget(nullptr);

  remote->UpdateBackingScaleCache(540.0f, 1, 3.0, 1.0);
  EXPECT_FLOAT_EQ(remote->GetDPI(), 540.0f);
  EXPECT_DOUBLE_EQ(remote->GetDefaultScale().scale, 3.0);

  remote->UpdateBackingScaleCache(480.0f, 1, 2.5, 1.0);
  EXPECT_FLOAT_EQ(remote->GetDPI(), 480.0f);
  EXPECT_DOUBLE_EQ(remote->GetDefaultScale().scale, 2.5);

  remote->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, InvalidateUsesAttachedListener)
{
  RefPtr<TestEmbedLitePuppetWidget> widget =
    new TestEmbedLitePuppetWidget;
  CountingWidgetListener primaryListener;
  CountingWidgetListener attachedListener;

  widget->SetWidgetListener(&primaryListener);
  widget->SetAttachedWidgetListener(&attachedListener);
  widget->Invalidate(LayoutDeviceIntRect(0, 0, 100, 100));

  EXPECT_EQ(primaryListener.mPaintCount, 0);
  EXPECT_EQ(attachedListener.mPaintCount, 0);

  ASSERT_TRUE(mozilla::SpinEventLoopUntil(
    "EmbedLiteChromeWindowHostTest::InvalidateUsesAttachedListener"_ns,
    [&]() { return attachedListener.mPaintCount == 1; }));

  EXPECT_EQ(primaryListener.mPaintCount, 0);
  EXPECT_EQ(attachedListener.mPaintCount, 1);

  widget->SetAttachedWidgetListener(nullptr);
  widget->SetWidgetListener(nullptr);
  widget->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, RotationSwapsNativeSizeForLayout)
{
  RefPtr<TestEmbedLitePuppetWidget> widget =
    new TestEmbedLitePuppetWidget;
  widget->SetSize(100, 200);

  widget->SetRotation(mozilla::ROTATION_0);
  widget->UpdateBounds(false);
  EXPECT_EQ(widget->GetBounds().width, 100);
  EXPECT_EQ(widget->GetBounds().height, 200);

  widget->SetRotation(mozilla::ROTATION_90);
  widget->UpdateBounds(false);
  EXPECT_EQ(widget->GetBounds().width, 200);
  EXPECT_EQ(widget->GetBounds().height, 100);

  widget->SetRotation(mozilla::ROTATION_180);
  widget->UpdateBounds(false);
  EXPECT_EQ(widget->GetBounds().width, 100);
  EXPECT_EQ(widget->GetBounds().height, 200);

  widget->SetRotation(mozilla::ROTATION_270);
  widget->UpdateBounds(false);
  EXPECT_EQ(widget->GetBounds().width, 200);
  EXPECT_EQ(widget->GetBounds().height, 100);

  widget->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, InputUsesHostedAttachedListener)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::WidgetTouchEvent event(true, mozilla::eTouchStart, host);
  EXPECT_EQ(host->DispatchEvent(&event), nsEventStatus_eConsumeDoDefault);
  EXPECT_EQ(hostedListener.mEventCount, 1);
  EXPECT_EQ(event.mWidget.get(), hosted.get());

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, PageInsetsUseHostedRootWidget)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);
  host->InitializeChromeInput();

  EXPECT_TRUE(host->SetChromeMargins(
    mozilla::LayoutDeviceIntMargin(1, 2, 3, 4)));
  const mozilla::LayoutDeviceIntMargin insets(5, 6, 7, 8);
  EXPECT_TRUE(host->SetChromeSafeAreaInsets(insets));
  EXPECT_EQ(hosted->GetSafeAreaInsets(), insets);

  hosted->Destroy();
  EXPECT_FALSE(host->SetChromeMargins(
    mozilla::LayoutDeviceIntMargin(1, 2, 3, 4)));
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, FocusCallbacksAreDeduplicated)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener primaryListener;
  CountingWidgetListener attachedListener;
  hosted->SetWidgetListener(&primaryListener);
  hosted->SetAttachedWidgetListener(&attachedListener);
  EXPECT_TRUE(host->SetChromeFocused(true));
  EXPECT_TRUE(host->SetChromeFocused(true));
  EXPECT_EQ(primaryListener.mActivatedCount, 1);
  EXPECT_EQ(primaryListener.mDeactivatedCount, 0);
  EXPECT_EQ(attachedListener.mActivatedCount, 0);
  EXPECT_EQ(attachedListener.mDeactivatedCount, 0);

  EXPECT_TRUE(host->SetChromeFocused(false));
  EXPECT_TRUE(host->SetChromeFocused(false));
  EXPECT_EQ(primaryListener.mActivatedCount, 1);
  EXPECT_EQ(primaryListener.mDeactivatedCount, 1);
  EXPECT_EQ(attachedListener.mActivatedCount, 0);
  EXPECT_EQ(attachedListener.mDeactivatedCount, 0);

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->SetWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, KeyboardUsesTextEventDispatcher)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  EXPECT_FALSE(host->DispatchChromeKeyPress(65, 0, 'a'));

  host->InitializeChromeInput();
  EXPECT_TRUE(host->DispatchChromeKeyPress(65, 0, 'a'));
  EXPECT_TRUE(host->DispatchChromeKeyRelease(65, 0, 'a'));
  ASSERT_EQ(hostedListener.mMessages.Length(), 3u);
  EXPECT_EQ(hostedListener.mMessages[0], mozilla::eKeyDown);
  EXPECT_EQ(hostedListener.mMessages[1], mozilla::eKeyPress);
  EXPECT_EQ(hostedListener.mMessages[2], mozilla::eKeyUp);
  EXPECT_EQ(hostedListener.mLastEventWidget, hosted.get());

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, PreeditAndCommitUseCompositionEvents)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::widget::InputContext inputContext;
  inputContext.mIMEState.mEnabled = mozilla::widget::IMEEnabled::Enabled;
  mozilla::widget::InputContextAction action;
  action.mCause = mozilla::widget::InputContextAction::CAUSE_TOUCH;
  action.mFocusChange = mozilla::widget::InputContextAction::GOT_FOCUS;
  hosted->SetInputContext(inputContext, action);
  EXPECT_EQ(hosted->GetInputContext().mIMEState.mEnabled,
            mozilla::widget::IMEEnabled::Enabled);

  EXPECT_TRUE(host->DispatchChromeTextEvent(
    mozilla::EmptyString(), u"text"_ns, 0, 0));
  EXPECT_TRUE(host->DispatchChromeTextEvent(
    u"text"_ns, mozilla::EmptyString(), 0, 0));
  ASSERT_EQ(hostedListener.mMessages.Length(), 3u);
  EXPECT_EQ(hostedListener.mMessages[0], mozilla::eCompositionStart);
  EXPECT_EQ(hostedListener.mMessages[1], mozilla::eCompositionChange);
  EXPECT_EQ(hostedListener.mMessages[2], mozilla::eCompositionCommit);
  EXPECT_EQ(hostedListener.mLastEventWidget, hosted.get());

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, IMERequestsEndComposition)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::widget::InputContext inputContext;
  inputContext.mIMEState.mEnabled = mozilla::widget::IMEEnabled::Enabled;
  mozilla::widget::InputContextAction action;
  action.mCause = mozilla::widget::InputContextAction::CAUSE_TOUCH;
  action.mFocusChange = mozilla::widget::InputContextAction::GOT_FOCUS;
  hosted->SetInputContext(inputContext, action);

  ASSERT_TRUE(host->DispatchChromeTextEvent(
    mozilla::EmptyString(), u"cancel"_ns, 0, 0));
  hostedListener.mMessages.Clear();
  EXPECT_EQ(hosted->NotifyIME(mozilla::widget::IMENotification(
              mozilla::widget::REQUEST_TO_CANCEL_COMPOSITION)),
            NS_OK);
  ASSERT_EQ(hostedListener.mMessages.Length(), 1u);
  EXPECT_EQ(hostedListener.mMessages[0], mozilla::eCompositionCommit);

  ASSERT_TRUE(host->DispatchChromeTextEvent(
    mozilla::EmptyString(), u"commit"_ns, 0, 0));
  hostedListener.mMessages.Clear();
  EXPECT_EQ(hosted->NotifyIME(mozilla::widget::IMENotification(
              mozilla::widget::REQUEST_TO_COMMIT_COMPOSITION)),
            NS_OK);
  ASSERT_EQ(hostedListener.mMessages.Length(), 1u);
  EXPECT_EQ(hostedListener.mMessages[0], mozilla::eCompositionCommitAsIs);

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}
