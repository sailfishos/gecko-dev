/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsEmbedClipboard_h__
#define nsEmbedClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"
#include "nsIEmbedAppService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsWidgetsCID.h"

/* Native Qt Clipboard wrapper */
class nsEmbedClipboard : public nsIClipboard, public nsIObserver
{
public:
    nsEmbedClipboard();
    //nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    // nsIClipboard
    NS_DECL_NSICLIPBOARD

private:
    virtual ~nsEmbedClipboard();

    nsCOMPtr<nsIEmbedAppService> mService;
    nsCOMPtr<nsIObserverService> mObserverService;
    nsString mBuffer;
    int mModalDepth;
    bool mActive;
};

// {8B5314BA-DB01-11d2-96CE-0060B0FB9956}
#define NS_EMBED_CLIPBOARD_SERVICE_CID NS_CLIPBOARD_CID

#endif // nsEmbedClipboard_h__
