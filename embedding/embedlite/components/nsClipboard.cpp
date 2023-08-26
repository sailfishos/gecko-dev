/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"

#include "imgIContainer.h"
#include "gfxImageSurface.h"
#include "nsWidgetsCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIEmbedLiteJSON.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIThread.h"

using namespace mozilla;

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);
static const char* sClipboardTextFlavors[] = { kUnicodeMime };

NS_IMPL_ISUPPORTS(nsEmbedClipboard, nsIClipboard)

nsEmbedClipboard::nsEmbedClipboard() : nsIClipboard()
{
  if (!mService) {
    mService = do_GetService("@mozilla.org/embedlite-app-service;1");
  }
  if (!mObserverService) {
    mObserverService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  }
  mObserverService->AddObserver(this, "outer-window-destroyed", false);
  mModalDepth = 0;
  mActive = true;
}

nsEmbedClipboard::~nsEmbedClipboard()
{
}

NS_IMETHODIMP
nsEmbedClipboard::SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsISupports> tmp;
  nsresult rv  = aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(tmp));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
  // No support for non-text data
  NS_ENSURE_TRUE(supportsString, NS_ERROR_NOT_IMPLEMENTED);
  nsAutoString buffer;
  supportsString->GetData(buffer);

  bool isPrivateData = aTransferable->GetIsPrivateData();
  nsString message;
  // Just simple property bag support still
  nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
  nsCOMPtr<nsIWritablePropertyBag2> root;
  json->CreateObject(getter_AddRefs(root));
  root->SetPropertyAsAString(NS_LITERAL_STRING("data"), buffer);
  root->SetPropertyAsBool(NS_LITERAL_STRING("private"), isPrivateData);

  json->CreateJSON(root, message);
  // Possible we can avoid json stuff for this case and send uri directly
  mObserverService->NotifyObservers(nullptr, "clipboard:setdata", message.get());

  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::GetData(nsITransferable* aTransferable, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  mObserverService->AddObserver(this, "embedui:clipboard", false);
  nsString message;
  mObserverService->NotifyObservers(nullptr, "clipboard:getdata", message.get());

  nsresult rv;
  int origModalDepth = mModalDepth;
  nsCOMPtr<nsIThread> thread;
  NS_GetCurrentThread(getter_AddRefs(thread));
  while (mActive && mModalDepth == origModalDepth && NS_SUCCEEDED(rv)) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(true, &processedEvent);
    if (NS_SUCCEEDED(rv) && !processedEvent) {
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  if (mActive) {
    nsCOMPtr<nsISupportsString> dataWrapper =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dataWrapper->SetData(mBuffer);
    NS_ENSURE_SUCCESS(rv, rv);

    // If our data flavor has already been added, this will fail. But we don't care
    aTransferable->AddDataFlavor(kUnicodeMime);

    nsCOMPtr<nsISupports> nsisupportsDataWrapper =
      do_QueryInterface(dataWrapper);
    rv = aTransferable->SetTransferData(kUnicodeMime, nsisupportsDataWrapper);
    NS_ENSURE_SUCCESS(rv, rv);

    mBuffer.Truncate();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
{
    if (!strcmp(aTopic, "embedui:clipboard")) {
      mObserverService->RemoveObserver(this, "embedui:clipboard");
      mBuffer.Assign(aData);
      mModalDepth--;
    }
    else if (!strcmp(aTopic, "outer-window-destroyed")) {
      mObserverService->RemoveObserver(this, "outer-window-destroyed");
      mActive = false;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard, bool* aHasText)
{
  NS_ENSURE_ARG_POINTER(aHasText);
  *aHasText = true;
  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);

  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::SupportsSelectionClipboard(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::SupportsFindClipboard(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}
