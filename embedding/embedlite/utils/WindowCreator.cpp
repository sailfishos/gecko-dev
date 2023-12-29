/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "nsIWebBrowserChrome.h"
#include "nsIURI.h"
#include "WindowCreator.h"
#include "nsString.h"
#include <stdio.h>
#include "EmbedLiteViewChildIface.h"
#include "EmbedLiteAppChildIface.h"
#include "nsCOMPtr.h"
#include "nsIOpenWindowInfo.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"           // for NS_GetCurrentThread
#include <sys/syscall.h>

using namespace mozilla::embedlite;

WindowCreator::WindowCreator(EmbedLiteAppChildIface* aChild)
  : mChild(aChild)
{
  LOGT();
}

WindowCreator::~WindowCreator()
{
  LOGT();
}

NS_IMPL_ISUPPORTS(WindowCreator, nsIWindowCreator)

NS_IMETHODIMP
WindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                  uint32_t aChromeFlags,
                                  nsIOpenWindowInfo *aOpenWindowInfo,
                                  bool *aCancel,
                                  nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  NS_ENSURE_ARG_POINTER(_retval);
  *aCancel = false;
  *_retval = 0;

  /*
      See bug 80707
      Desktop FF allow to create popup window if aChromeFlags == 1670, aContextFlags == 0
  */


  EmbedLiteViewChildIface* parent = mChild->GetViewByChromeParent(aParent);
  uint32_t createdID = 0;
  uint32_t parentID = parent ? parent->GetID() : 0;

  RefPtr<BrowsingContext> parentBrowsingContext = aOpenWindowInfo->GetParent();

  LOGT("parent: %p, chrome flags: %u, thread id: %ld parent opener id: %" PRId64 "", aParent, aChromeFlags, syscall(SYS_gettid), parentBrowsingContext->Id());

  const bool isForPrinting = aOpenWindowInfo->GetIsForPrinting();

  mChild->CreateWindow(parentID, reinterpret_cast<uintptr_t>(parentBrowsingContext.get()), aChromeFlags, isForPrinting, &createdID, aCancel);

  if (*aCancel) {
    return NS_OK;
  }

  nsresult rv(NS_OK);
  nsCOMPtr<nsIWebBrowserChrome> browser;
  nsCOMPtr<nsIThread> thread;
  NS_GetCurrentThread(getter_AddRefs(thread));
  while (!browser && NS_SUCCEEDED(rv)) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(true, &processedEvent);
    if (NS_SUCCEEDED(rv) && !processedEvent) {
      rv = NS_ERROR_UNEXPECTED;
    }
    EmbedLiteViewChildIface* view = mChild->GetViewByID(createdID);
    if (view) {
      view->GetBrowserChrome(getter_AddRefs(browser));
    }
  }

  // check to make sure that we made a new window
  if (_retval) {
      NS_ADDREF(*_retval = browser);
      return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}
