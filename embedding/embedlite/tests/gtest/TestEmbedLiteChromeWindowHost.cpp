/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "embedshared/EmbedLitePuppetWidget.h"
#include "embedshared/nsWindow.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
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
  void WillPaintWindow(nsIWidget*) override { ++mWillPaintCount; }
  void DidPaintWindow() override { ++mDidPaintCount; }

  int mWillPaintCount = 0;
  int mDidPaintCount = 0;
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
