/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteBrowserInit_h
#define EmbedLiteBrowserInit_h

#include "mozilla/RefPtr.h"
#include "mozilla/embedlite/PEmbedLiteApp.h"
#include "nsCOMPtr.h"

class nsIOpenWindowInfo;

namespace mozilla::dom {
class BrowsingContext;
class WindowGlobalChild;
}

namespace mozilla::embedlite {

nsresult BuildEmbedLiteBrowserInit(nsIOpenWindowInfo* aOpenWindowInfo,
                                   uint32_t aChromeFlags,
                                   EmbedLiteBrowserInitData* aInitData);

nsresult RestoreEmbedLiteBrowserInit(
    const EmbedLiteBrowserInitData& aInitData,
    RefPtr<dom::BrowsingContext>& aBrowsingContext,
    RefPtr<dom::WindowGlobalChild>& aInitialWindowGlobal,
    nsCOMPtr<nsIOpenWindowInfo>& aInitialOpenWindowInfo,
    nsCOMPtr<nsIOpenWindowInfo>& aOriginalOpenWindowInfo);

void DiscardEmbedLiteBrowserInit(const EmbedLiteBrowserInitData& aInitData);

nsresult CreateEmbedLiteStandaloneInit(
    bool aIsPrivateWindow, RefPtr<dom::BrowsingContext>& aBrowsingContext,
    nsCOMPtr<nsIOpenWindowInfo>& aOpenWindowInfo);

bool IsUsableBrowserCreation(nsresult aResult, const void* aBrowser);

}  // namespace mozilla::embedlite

#endif  // EmbedLiteBrowserInit_h
