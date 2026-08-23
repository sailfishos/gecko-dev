/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_CHROME_ENDPOINT_IDS_H
#define MOZ_EMBEDLITE_CHROME_ENDPOINT_IDS_H

#include <map>
#include <stdint.h>

namespace mozilla::embedlite {

class EmbedLiteChromeEndpointIds final
{
public:
  explicit EmbedLiteChromeEndpointIds(uint32_t aFirst = 0x80000000u)
    : mNext(aFirst)
  {}

  template<typename IsExternallyOccupied>
  uint32_t Allocate(uint64_t aBrowserId,
                    IsExternallyOccupied&& aIsExternallyOccupied)
  {
    if (!aBrowserId || mBrowserToEndpoint.count(aBrowserId)) {
      return 0;
    }
    for (uint64_t attempts = 0; attempts <= UINT32_MAX; ++attempts) {
      const uint32_t candidate = mNext++;
      if (!candidate || ContainsEndpoint(candidate) ||
          aIsExternallyOccupied(candidate)) {
        continue;
      }
      mEndpointToBrowser.emplace(candidate, aBrowserId);
      mBrowserToEndpoint.emplace(aBrowserId, candidate);
      return candidate;
    }
    return 0;
  }

  bool Rebind(uint32_t aEndpointId, uint64_t aBrowserId)
  {
    const auto endpoint = mEndpointToBrowser.find(aEndpointId);
    if (endpoint == mEndpointToBrowser.end() || !aBrowserId) {
      return false;
    }
    const auto collision = mBrowserToEndpoint.find(aBrowserId);
    if (collision != mBrowserToEndpoint.end() &&
        collision->second != aEndpointId) {
      return false;
    }
    mBrowserToEndpoint.erase(endpoint->second);
    endpoint->second = aBrowserId;
    mBrowserToEndpoint.insert_or_assign(aBrowserId, aEndpointId);
    return true;
  }

  bool Remove(uint32_t aEndpointId)
  {
    const auto endpoint = mEndpointToBrowser.find(aEndpointId);
    if (endpoint == mEndpointToBrowser.end()) {
      return false;
    }
    mBrowserToEndpoint.erase(endpoint->second);
    mEndpointToBrowser.erase(endpoint);
    return true;
  }

  bool ContainsEndpoint(uint32_t aEndpointId) const
  {
    return mEndpointToBrowser.count(aEndpointId);
  }

  uint32_t FindByBrowserId(uint64_t aBrowserId) const
  {
    const auto browser = mBrowserToEndpoint.find(aBrowserId);
    return browser == mBrowserToEndpoint.end() ? 0 : browser->second;
  }

private:
  std::map<uint32_t, uint64_t> mEndpointToBrowser;
  std::map<uint64_t, uint32_t> mBrowserToEndpoint;
  uint32_t mNext;
};

} // namespace mozilla::embedlite

#endif
