/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZEMBED_WEBBROWSERCHROME_H
#define MOZEMBED_WEBBROWSERCHROME_H

#include "nsCOMPtr.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIWebProgressListener.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMEventListener.h"
#include "nsString.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"

#include "nsPoint.h"

#define kNotFound -1

namespace mozilla {
namespace embedlite {
  class BrowserChildHelper;
}
}

class nsIEmbedBrowserChromeListener;
class WebBrowserChrome : public nsIWebBrowserChrome,
                         public nsIWebProgressListener,
                         public nsIWebBrowserChromeFocus,
                         public nsIEmbeddingSiteWindow,
                         public nsIInterfaceRequestor,
                         public nsIDOMEventListener,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIWEBBROWSERCHROMEFOCUS
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIDOMEVENTLISTENER

  WebBrowserChrome(nsIEmbedBrowserChromeListener* aListener);


  void SetEventHandler();
  void RemoveEventHandler();

  void SetBrowserChildHelper(mozilla::embedlite::BrowserChildHelper* aHelper);
  NS_IMETHODIMP GetWebBrowser(nsIWebBrowser * *aWebBrowser);
  NS_IMETHODIMP SetWebBrowser(nsIWebBrowser* aWebBrowser);

protected:
  virtual ~WebBrowserChrome();

private:
  nsIntPoint GetScrollOffset(mozIDOMWindowProxy *aWindow);
  nsresult GetDocShellPtr(nsIDocShell **aDocShell);
  nsresult GetDocumentPtr(mozilla::dom::Document **aDocument);
  nsresult GetHttpUserAgent(nsIRequest* request, nsAString& aHttpUserAgent);

  void SendScroll();

  /* additional members */
  nsCOMPtr<nsIWebBrowser> mWebBrowser;
  uint32_t mChromeFlags;
  bool mIsVisible;
  bool mHandlerAdded;
  int mTotalRequests;
  int mFinishedRequests;
  bool mLocationHasChanged;
  nsCString mLastLocation;
  bool mFirstPaint;
  nsIntPoint mScrollOffset;
  nsIEmbedBrowserChromeListener* mListener;
  nsString mTitle;
  RefPtr<mozilla::embedlite::BrowserChildHelper> mHelper;
};

#endif /* Header guard */

