/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsEmbedClipboard_h__
#define nsEmbedClipboard_h__

#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"
#include "nsBaseClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"
#include "nsIEmbedAppService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"

/* Native Qt Clipboard wrapper */
class nsEmbedClipboard : public ClipboardSetDataHelper, public nsIObserver
{
public:
    nsEmbedClipboard();
    //nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    // nsIClipboard
    NS_IMETHOD GetData(nsITransferable* aTransferable,
                       int32_t aWhichClipboard) override;
    NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override;
    NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                      int32_t aWhichClipboard,
                                      bool* _retval) override;
    NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                        bool* _retval) override;
    RefPtr<mozilla::GenericPromise> AsyncGetData(
        nsITransferable* aTransferable, int32_t aWhichClipboard) override;
    RefPtr<DataFlavorsPromise> AsyncHasDataMatchingFlavors(
        const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;

private:
    struct PendingAsyncGetData;

    virtual ~nsEmbedClipboard();

    NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                      nsIClipboardOwner* anOwner,
                                      int32_t aWhichClipboard) override;
    nsresult RequestClipboardData();
    void StopObservingClipboardData();
    void CancelPendingAsyncGetData(nsresult aReason);
    void CompletePendingAsyncGetData(const nsAString& aData);
    nsresult SetTransferableText(nsITransferable* aTransferable,
                                 const nsAString& aData);

    nsCOMPtr<nsIEmbedAppService> mService;
    nsCOMPtr<nsIObserverService> mObserverService;
    mozilla::UniquePtr<PendingAsyncGetData> mPendingAsyncGetData;
    nsString mBuffer;
    int mModalDepth;
    bool mWaitingForClipboardData;
    bool mActive;
};

#endif // nsEmbedClipboard_h__
