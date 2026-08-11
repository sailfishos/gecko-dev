/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "embedshared/EmbedLitePuppetWidget.h"
#include "embedshared/nsWindow.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"

using mozilla::embedlite::AutoEmbedLiteChromeWindowHost;
using mozilla::embedlite::EmbedLitePuppetWidget;
using mozilla::embedlite::nsWindow;

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
