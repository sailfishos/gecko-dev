/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_CHROME_CONTENT_EVENT_ORDER_H
#define MOZ_EMBEDLITE_CHROME_CONTENT_EVENT_ORDER_H

#include <stdint.h>

namespace mozilla::embedlite {

constexpr uint32_t kMaxContentNameLength = 1024;
constexpr uint32_t kMaxContentDataLength = 1024 * 1024;

constexpr bool IsChromeContentNotificationBounded(
    uint32_t aNameLength, uint32_t aDataLength)
{
  return aNameLength > 0 && aNameLength <= kMaxContentNameLength &&
         aDataLength <= kMaxContentDataLength;
}

constexpr bool IsChromeContentNotificationCurrent(
    uint64_t aTabId, uint64_t aPersistentId, uint64_t aLocationRevision,
    uint64_t aCurrentTabId, uint64_t aCurrentPersistentId,
    uint64_t aCurrentLocationRevision)
{
  return aTabId != 0 && aTabId == aCurrentTabId &&
         aPersistentId == aCurrentPersistentId &&
         aLocationRevision == aCurrentLocationRevision;
}

template <typename SnapshotSender, typename EventSender>
auto SendAfterPendingChromeTabSnapshot(
    bool aSnapshotPending, SnapshotSender&& aSendSnapshot,
    EventSender&& aSendEvent) -> decltype(aSendEvent())
{
  // Parent-side validation resolves the event identity against its latest
  // snapshot. Keep both sends on this actor in that strict order.
  if (aSnapshotPending) {
    aSendSnapshot();
  }
  return aSendEvent();
}

} // namespace mozilla::embedlite

#endif
