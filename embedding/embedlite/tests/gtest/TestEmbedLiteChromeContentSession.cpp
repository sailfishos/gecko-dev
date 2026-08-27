/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "embedshared/EmbedLiteChromeContentEventOrder.h"
#include "embedshared/EmbedLiteChromeContentRegistrations.h"
#include "embedshared/EmbedLiteChromeEndpointIds.h"
#include "modules/EmbedLiteAppService.h"

using mozilla::embedlite::EmbedLiteChromeContentRegistrations;
using mozilla::embedlite::EmbedLiteChromeEndpointIds;
using mozilla::embedlite::IsChromeContentNotificationBounded;
using mozilla::embedlite::IsChromeContentNotificationCurrent;
using mozilla::embedlite::kMaxContentDataLength;
using mozilla::embedlite::kMaxContentNameLength;
using mozilla::embedlite::SendAfterPendingChromeTabSnapshot;

TEST(EmbedLiteChromeContentSessionTest,
     PendingSnapshotPrecedesIdentityBearingEvent)
{
  nsTArray<nsCString> order;
  const bool sent = SendAfterPendingChromeTabSnapshot(
    true,
    [&order]() { order.AppendElement("snapshot"_ns); },
    [&order]() {
      order.AppendElement("event"_ns);
      return true;
    });

  EXPECT_TRUE(sent);
  ASSERT_EQ(order.Length(), 2u);
  EXPECT_EQ(order[0], "snapshot"_ns);
  EXPECT_EQ(order[1], "event"_ns);

  order.Clear();
  SendAfterPendingChromeTabSnapshot(
    false,
    [&order]() { order.AppendElement("snapshot"_ns); },
    [&order]() { order.AppendElement("event"_ns); });
  ASSERT_EQ(order.Length(), 1u);
  EXPECT_EQ(order[0], "event"_ns);
}

TEST(EmbedLiteChromeContentSessionTest,
     ContentNotificationBoundsAreInclusiveAndRejectEmptyNames)
{
  EXPECT_TRUE(IsChromeContentNotificationBounded(
    kMaxContentNameLength, kMaxContentDataLength));
  EXPECT_FALSE(IsChromeContentNotificationBounded(
    0, kMaxContentDataLength));
  EXPECT_FALSE(IsChromeContentNotificationBounded(
    kMaxContentNameLength + 1, 0));
  EXPECT_FALSE(IsChromeContentNotificationBounded(
    1, kMaxContentDataLength + 1));
}

TEST(EmbedLiteChromeContentSessionTest,
     ContentNotificationIdentityIncludesLocationRevision)
{
  EXPECT_TRUE(IsChromeContentNotificationCurrent(
    7, 11, 13, 7, 11, 13));
  EXPECT_FALSE(IsChromeContentNotificationCurrent(
    7, 11, 12, 7, 11, 13));
  EXPECT_FALSE(IsChromeContentNotificationCurrent(
    7, 10, 13, 7, 11, 13));
  EXPECT_FALSE(IsChromeContentNotificationCurrent(
    0, 11, 13, 0, 11, 13));
}

TEST(EmbedLiteChromeContentSessionTest,
     RegistrationsAreIdempotentAndRetainedForReplay)
{
  EmbedLiteChromeContentRegistrations registrations;

  EXPECT_TRUE(registrations.AddFrameScript("chrome://test/one.js"_ns));
  EXPECT_FALSE(registrations.AddFrameScript("chrome://test/one.js"_ns));
  EXPECT_TRUE(registrations.AddFrameScript("chrome://test/two.js"_ns));
  ASSERT_EQ(registrations.FrameScripts().Length(), 2u);
  EXPECT_EQ(registrations.FrameScripts()[0], "chrome://test/one.js"_ns);
  EXPECT_EQ(registrations.FrameScripts()[1], "chrome://test/two.js"_ns);

  EXPECT_TRUE(registrations.AddMessageListener("Content:ContextMenu"_ns));
  EXPECT_FALSE(registrations.AddMessageListener("Content:ContextMenu"_ns));
  EXPECT_TRUE(
    registrations.AddMessageListener("Content:SelectionCopied"_ns));
  // browser.js converts these legacy sync-only notifications to async Qt
  // delivery while returning an immediate empty sync result in Gecko. They
  // must remain cached so lazy/rematerialized tabs get the same conversion.
  EXPECT_TRUE(
    registrations.AddMessageListener("Content:HandlerShutdown"_ns));
  ASSERT_EQ(registrations.MessageListeners().Length(), 3u);

  EXPECT_TRUE(registrations.RemoveMessageListener(
    "Content:ContextMenu"_ns));
  EXPECT_FALSE(registrations.RemoveMessageListener(
    "Content:ContextMenu"_ns));
  ASSERT_EQ(registrations.MessageListeners().Length(), 2u);
  EXPECT_EQ(registrations.MessageListeners()[0],
            "Content:SelectionCopied"_ns);
  EXPECT_EQ(registrations.MessageListeners()[1],
            "Content:HandlerShutdown"_ns);
}

TEST(EmbedLiteChromeContentSessionTest, FrameScriptRemovalIsExplicit)
{
  EmbedLiteChromeContentRegistrations registrations;
  ASSERT_TRUE(registrations.AddFrameScript("file:///tmp/test.js"_ns));
  EXPECT_TRUE(registrations.RemoveFrameScript("file:///tmp/test.js"_ns));
  EXPECT_FALSE(registrations.RemoveFrameScript("file:///tmp/test.js"_ns));
  EXPECT_TRUE(registrations.FrameScripts().IsEmpty());
}

TEST(EmbedLiteChromeContentSessionTest,
     EndpointIdsSkipCollisionsAndRemainStableAcrossRebind)
{
  EmbedLiteChromeEndpointIds endpoints(0x7ffffffeu);
  const uint32_t first = endpoints.Allocate(
    10, [](uint32_t aCandidate) { return aCandidate == 0x7ffffffeu; });
  EXPECT_EQ(first, 0x7fffffffu);
  EXPECT_EQ(endpoints.FindByBrowserId(10), first);

  // Allocation wraps within the positive signed 32-bit range.
  const uint32_t second = endpoints.Allocate(
    20, [](uint32_t) { return false; });
  EXPECT_EQ(second, 1u);

  EXPECT_TRUE(endpoints.Rebind(first, 30));
  EXPECT_EQ(endpoints.FindByBrowserId(10), 0u);
  EXPECT_EQ(endpoints.FindByBrowserId(30), first);
  EXPECT_FALSE(endpoints.Rebind(first, 20));
}

TEST(EmbedLiteChromeContentSessionTest,
     DefaultEndpointIdsFitSignedQmlIntegers)
{
  EmbedLiteChromeEndpointIds endpoints;
  const uint32_t endpoint = endpoints.Allocate(
    55, [](uint32_t) { return false; });
  EXPECT_EQ(endpoint, EmbedLiteChromeEndpointIds::kFirstEndpointId);
  EXPECT_LE(endpoint, EmbedLiteChromeEndpointIds::kLastEndpointId);
}

TEST(EmbedLiteChromeContentSessionTest, EndpointRemovalClearsBothIndexes)
{
  EmbedLiteChromeEndpointIds endpoints;
  const uint32_t endpoint = endpoints.Allocate(
    55, [](uint32_t) { return false; });
  ASSERT_NE(endpoint, 0u);
  EXPECT_TRUE(endpoints.ContainsEndpoint(endpoint));
  EXPECT_TRUE(endpoints.Remove(endpoint));
  EXPECT_FALSE(endpoints.ContainsEndpoint(endpoint));
  EXPECT_EQ(endpoints.FindByBrowserId(55), 0u);
  EXPECT_FALSE(endpoints.Remove(endpoint));
}

TEST(EmbedLiteChromeContentSessionTest,
     HostedBrowsingContextUsesCanonicalStringTabIds)
{
  mozilla::RefPtr<EmbedLiteAppService> service =
    new EmbedLiteAppService();
  mozilla::dom::BrowsingContext* context = nullptr;

  // These are valid uint64 strings; the lookup proceeds to the deliberately
  // absent hosted window rather than rejecting their representation.
  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"1"_ns, &context), NS_ERROR_NOT_AVAILABLE);
  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"18446744073709551615"_ns, &context),
            NS_ERROR_NOT_AVAILABLE);

  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"0"_ns, &context), NS_ERROR_INVALID_ARG);
  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"01"_ns, &context), NS_ERROR_INVALID_ARG);
  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"+1"_ns, &context), NS_ERROR_INVALID_ARG);
  EXPECT_EQ(service->GetChromeTabBrowsingContext(
              1, u"18446744073709551616"_ns, &context),
            NS_ERROR_INVALID_ARG);
}
