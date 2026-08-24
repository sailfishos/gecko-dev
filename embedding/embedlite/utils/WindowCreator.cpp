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
#include "EmbedLiteBrowserInit.h"
#include "base/message_loop.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIOpenWindowInfo.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"           // for NS_GetCurrentThread
#include "prthread.h"
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
  NS_ENSURE_ARG_POINTER(aOpenWindowInfo);
  *aCancel = false;
  *_retval = 0;

  /*
      See bug 80707
      Desktop FF allow to create popup window if aChromeFlags == 1670, aContextFlags == 0
  */


  EmbedLiteViewChildIface* parent = mChild->GetViewByChromeParent(aParent);
  uint32_t createdID = 0;
  uint32_t parentID = parent ? parent->GetID() : 0;

  const bool isForPrinting = aOpenWindowInfo->GetIsForPrinting();
  EmbedLiteBrowserInitData browserInit;
  nsresult rv = BuildEmbedLiteBrowserInit(aOpenWindowInfo, aChromeFlags,
                                           &browserInit);
  if (NS_FAILED(rv)) {
    LOGW("Rejecting unsupported EmbedLite popup: rv=0x%08x, remote=%d, "
         "fission=%d", static_cast<uint32_t>(rv),
         aOpenWindowInfo->GetIsRemote(),
         !!(aChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW));
    *aCancel = true;
    return NS_OK;
  }

  if (!mChild->CreateWindow(parentID, browserInit, aChromeFlags,
                            isForPrinting, &createdID, aCancel) || *aCancel) {
    DiscardEmbedLiteBrowserInit(browserInit);
    *aCancel = true;
    return NS_OK;
  }

  rv = NS_OK;
  nsCOMPtr<nsIWebBrowserChrome> browser;
  nsCOMPtr<nsIThread> thread;
  NS_GetCurrentThread(getter_AddRefs(thread));
  mozilla::TimeStamp start = mozilla::TimeStamp::Now();
  while (!browser && NS_SUCCEEDED(rv)) {
    bool processedEvent;
    if (MessageLoop* loop = MessageLoop::current()) {
      bool nestableTasksAllowed = loop->NestableTasksAllowed();
      loop->SetNestableTasksAllowed(true);
      RefPtr<nsIRunnable> quit = new MessageLoop::QuitTask();
      loop->PostTask(quit.forget());
      loop->Run();
      loop->SetNestableTasksAllowed(nestableTasksAllowed);
    }

    rv = thread->ProcessNextEvent(false, &processedEvent);
    if (NS_SUCCEEDED(rv) && !processedEvent) {
      PR_Sleep(PR_MillisecondsToInterval(1));
    }
    EmbedLiteViewChildIface* view = mChild->GetViewByID(createdID);
    if (view) {
      view->GetBrowserChrome(getter_AddRefs(browser));
    }
    if ((mozilla::TimeStamp::Now() - start).ToMilliseconds() > 5000) {
      LOGE("timed out waiting for chrome window, id: %u flags: %u", createdID, aChromeFlags);
      *aCancel = true;
      return NS_OK;
    }
  }

  if (!browser) {
    LOGW("EmbedLite popup browser creation failed: rv=0x%08x",
         static_cast<uint32_t>(rv));
    DiscardEmbedLiteBrowserInit(browserInit);
    *aCancel = true;
    return NS_OK;
  }

  NS_ADDREF(*_retval = browser);
  return NS_OK;
}
