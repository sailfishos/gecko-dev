/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "embedshared/EmbedLitePuppetWidget.h"
#include "embedshared/nsWindow.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/widget/IMEData.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsTArray.h"
#include "WindowRenderer.h"

using mozilla::embedlite::AutoEmbedLiteChromeWindowHost;
using mozilla::embedlite::EmbedLitePuppetWidget;
using mozilla::embedlite::nsWindow;
using mozilla::FallbackRenderer;
using mozilla::LayoutDeviceIntRect;
using mozilla::widget::InitData;
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

class CountingWidgetListener final : public nsIWidgetListener
{
public:
  void WindowActivated() override { ++mActivatedCount; }
  void WindowDeactivated() override { ++mDeactivatedCount; }
  void WillPaintWindow(nsIWidget*) override { ++mWillPaintCount; }
  void DidPaintWindow() override { ++mDidPaintCount; }
  nsEventStatus HandleEvent(mozilla::WidgetGUIEvent* aEvent, bool) override
  {
    ++mEventCount;
    mMessages.AppendElement(aEvent->mMessage);
    mLastEventWidget = aEvent->mWidget;
    return nsEventStatus_eConsumeDoDefault;
  }

  int mActivatedCount = 0;
  int mDeactivatedCount = 0;
  int mWillPaintCount = 0;
  int mDidPaintCount = 0;
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
  ASSERT_EQ(host->Create(nullptr, bounds, &hostInit), NS_OK);
  ASSERT_TRUE(host->IsVisible());

  AutoEmbedLiteChromeWindowHost reservation(host);
  ASSERT_TRUE(reservation.IsValid());

  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);

  InitData hostedInit;
  hostedInit.mWindowType = WindowType::TopLevel;
  ASSERT_EQ(hosted->Create(nullptr, bounds, &hostedInit), NS_OK);
  EXPECT_FALSE(hosted->IsVisible());

  hosted->Show(true);
  EXPECT_TRUE(hosted->IsVisible());

  hosted->Destroy();
  host->Destroy();
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

  EXPECT_EQ(primaryListener.mWillPaintCount, 0);
  EXPECT_EQ(primaryListener.mDidPaintCount, 0);
  EXPECT_EQ(attachedListener.mWillPaintCount, 0);
  EXPECT_EQ(attachedListener.mDidPaintCount, 0);

  ASSERT_TRUE(mozilla::SpinEventLoopUntil(
    "EmbedLiteChromeWindowHostTest::InvalidateUsesAttachedListener"_ns,
    [&]() { return attachedListener.mDidPaintCount == 1; }));

  EXPECT_EQ(primaryListener.mWillPaintCount, 0);
  EXPECT_EQ(primaryListener.mDidPaintCount, 0);
  EXPECT_EQ(attachedListener.mWillPaintCount, 1);
  EXPECT_EQ(attachedListener.mDidPaintCount, 1);

  widget->SetAttachedWidgetListener(nullptr);
  widget->SetWidgetListener(nullptr);
  widget->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, InputUsesHostedAttachedListener)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, &init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, &init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::WidgetTouchEvent event(true, mozilla::eTouchStart, host);
  nsEventStatus status = nsEventStatus_eIgnore;
  ASSERT_EQ(host->DispatchEvent(&event, status), NS_OK);
  EXPECT_EQ(status, nsEventStatus_eConsumeDoDefault);
  EXPECT_EQ(hostedListener.mEventCount, 1);
  EXPECT_EQ(event.mWidget.get(), hosted.get());

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, FocusCallbacksAreDeduplicated)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, &init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, &init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  EXPECT_TRUE(host->SetChromeFocused(true));
  EXPECT_TRUE(host->SetChromeFocused(true));
  EXPECT_EQ(hostedListener.mActivatedCount, 1);
  EXPECT_EQ(hostedListener.mDeactivatedCount, 0);

  EXPECT_TRUE(host->SetChromeFocused(false));
  EXPECT_TRUE(host->SetChromeFocused(false));
  EXPECT_EQ(hostedListener.mActivatedCount, 1);
  EXPECT_EQ(hostedListener.mDeactivatedCount, 1);

  hosted->SetAttachedWidgetListener(nullptr);
  hosted->Destroy();
  host->Destroy();
}

TEST(EmbedLiteChromeWindowHostTest, KeyboardUsesTextEventDispatcher)
{
  const LayoutDeviceIntRect bounds(0, 0, 100, 100);
  InitData init;
  init.mWindowType = WindowType::TopLevel;

  RefPtr<nsWindow> host = new nsWindow(nullptr);
  ASSERT_EQ(host->Create(nullptr, bounds, &init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, &init), NS_OK);

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
  ASSERT_EQ(host->Create(nullptr, bounds, &init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, &init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::widget::InputContext inputContext;
  inputContext.mIMEState.mEnabled = mozilla::widget::IMEEnabled::Enabled;
  inputContext.mIMEState.mOpen = mozilla::widget::IMEState::OPEN;
  mozilla::widget::InputContextAction action;
  action.mCause = mozilla::widget::InputContextAction::CAUSE_TOUCH;
  action.mFocusChange = mozilla::widget::InputContextAction::GOT_FOCUS;
  hosted->SetInputContext(inputContext, action);

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
  ASSERT_EQ(host->Create(nullptr, bounds, &init), NS_OK);

  AutoEmbedLiteChromeWindowHost reservation(host);
  nsCOMPtr<nsIWidget> hosted = nsIWidget::CreateTopLevelWindow();
  ASSERT_NE(hosted, nullptr);
  ASSERT_EQ(hosted->Create(nullptr, bounds, &init), NS_OK);
  host->InitializeChromeInput();

  CountingWidgetListener hostedListener;
  hosted->SetAttachedWidgetListener(&hostedListener);
  mozilla::widget::InputContext inputContext;
  inputContext.mIMEState.mEnabled = mozilla::widget::IMEEnabled::Enabled;
  inputContext.mIMEState.mOpen = mozilla::widget::IMEState::OPEN;
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
