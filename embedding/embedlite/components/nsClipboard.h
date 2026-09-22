/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsEmbedClipboard_h__
#define nsEmbedClipboard_h__

#include "mozilla/UniquePtr.h"
#include "nsBaseClipboard.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsIEmbedAppService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"

/* Native Qt Clipboard wrapper */
class nsEmbedClipboard final : public nsBaseClipboard, public nsIObserver
{
public:
    nsEmbedClipboard();
    //nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    mozilla::Result<int32_t, nsresult> GetNativeClipboardSequenceNumber(
        ClipboardType aWhichClipboard) override;

protected:
    NS_IMETHOD SetNativeClipboardData(
        nsITransferable* aTransferable,
        ClipboardType aWhichClipboard) override;
    mozilla::Result<nsCOMPtr<nsISupports>, nsresult> GetNativeClipboardData(
        const nsACString& aFlavor, ClipboardType aWhichClipboard,
        uint64_t aThreshold = 0) override;
    void AsyncGetNativeClipboardData(
        const nsACString& aFlavor, ClipboardType aWhichClipboard,
        GetNativeDataCallback&& aCallback) override;
    nsresult EmptyNativeClipboardData(
        ClipboardType aWhichClipboard) override;
    mozilla::Result<bool, nsresult> HasNativeClipboardDataMatchingFlavors(
        const nsTArray<nsCString>& aFlavorList,
        ClipboardType aWhichClipboard) override;

private:
    struct PendingAsyncGetData;

    virtual ~nsEmbedClipboard();

    nsresult RequestClipboardData();
    void StopObservingClipboardData();
    void CancelPendingAsyncGetData(nsresult aReason);
    void CompletePendingAsyncGetData(const nsAString& aData);

    nsCOMPtr<nsIEmbedAppService> mService;
    nsCOMPtr<nsIObserverService> mObserverService;
    mozilla::UniquePtr<PendingAsyncGetData> mPendingAsyncGetData;
    nsString mBuffer;
    int32_t mSequenceNumber;
    int mModalDepth;
    bool mWaitingForClipboardData;
    bool mActive;
};

#endif // nsEmbedClipboard_h__
