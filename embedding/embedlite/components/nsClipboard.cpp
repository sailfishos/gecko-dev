/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"

#include <cstring>

#include "mozilla/UniquePtr.h"
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

struct nsEmbedClipboard::PendingAsyncGetData {
  nsCOMPtr<nsITransferable> mTransferable;
  MozPromiseHolder<GenericPromise> mPromise;
};

NS_IMPL_ISUPPORTS_INHERITED(nsEmbedClipboard, ClipboardSetDataHelper,
                            nsIObserver)

nsEmbedClipboard::nsEmbedClipboard()
  : ClipboardSetDataHelper()
  , mModalDepth(0)
  , mWaitingForClipboardData(false)
  , mActive(true)
{
  if (!mService) {
    mService = do_GetService("@mozilla.org/embedlite-app-service;1");
  }
  if (!mObserverService) {
    mObserverService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  }
  mObserverService->AddObserver(this, "outer-window-destroyed", false);
}

nsEmbedClipboard::~nsEmbedClipboard()
{
  CancelPendingAsyncGetData(NS_ERROR_ABORT);
  StopObservingClipboardData();
}

NS_IMETHODIMP
nsEmbedClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                         nsIClipboardOwner* anOwner,
                                         int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsISupports> tmp;
  nsresult rv  = aTransferable->GetTransferData(kTextMime, getter_AddRefs(tmp));
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
  root->SetPropertyAsAString(u"data"_ns, buffer);
  root->SetPropertyAsBool(u"private"_ns, isPrivateData);

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

  CancelPendingAsyncGetData(NS_ERROR_ABORT);

  nsresult rv = RequestClipboardData();
  NS_ENSURE_SUCCESS(rv, rv);

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
    rv = SetTransferableText(aTransferable, mBuffer);
    NS_ENSURE_SUCCESS(rv, rv);
    mBuffer.Truncate();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
{
    if (!strcmp(aTopic, "embedui:clipboard")) {
      StopObservingClipboardData();
      if (mPendingAsyncGetData) {
        CompletePendingAsyncGetData(nsDependentString(aData));
      } else {
        mBuffer.Assign(aData);
        mModalDepth--;
      }
    }
    else if (!strcmp(aTopic, "outer-window-destroyed")) {
      mObserverService->RemoveObserver(this, "outer-window-destroyed");
      mActive = false;
      CancelPendingAsyncGetData(NS_ERROR_ABORT);
      StopObservingClipboardData();
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

  nsString message;
  nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
  nsCOMPtr<nsIWritablePropertyBag2> root;
  json->CreateObject(getter_AddRefs(root));
  root->SetPropertyAsAString(u"data"_ns, u""_ns);
  root->SetPropertyAsBool(u"private"_ns, false);

  json->CreateJSON(root, message);
  mObserverService->NotifyObservers(nullptr, "clipboard:setdata", message.get());
  mBuffer.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsEmbedClipboard::IsClipboardTypeSupported(int32_t aWhichClipboard, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = aWhichClipboard == kGlobalClipboard;
  return NS_OK;
}

RefPtr<GenericPromise>
nsEmbedClipboard::AsyncGetData(nsITransferable* aTransferable, int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
  }
  if (!aTransferable) {
    return GenericPromise::CreateAndReject(NS_ERROR_INVALID_ARG, __func__);
  }

  CancelPendingAsyncGetData(NS_ERROR_ABORT);
  mPendingAsyncGetData = MakeUnique<PendingAsyncGetData>();
  mPendingAsyncGetData->mTransferable = aTransferable;

  RefPtr<GenericPromise> promise =
    mPendingAsyncGetData->mPromise.Ensure(__func__);

  nsresult rv = RequestClipboardData();
  if (NS_FAILED(rv)) {
    mPendingAsyncGetData->mPromise.Reject(rv, __func__);
    mPendingAsyncGetData = nullptr;
  }

  return promise;
}

RefPtr<DataFlavorsPromise>
nsEmbedClipboard::AsyncHasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                              int32_t aWhichClipboard)
{
  nsTArray<nsCString> results;
  for (const auto& flavor : aFlavorList) {
    bool hasMatchingFlavor = false;
    nsresult rv = HasDataMatchingFlavors(AutoTArray<nsCString, 1>{flavor},
                                         aWhichClipboard, &hasMatchingFlavor);
    if (NS_SUCCEEDED(rv) && hasMatchingFlavor) {
      results.AppendElement(flavor);
    }
  }

  return DataFlavorsPromise::CreateAndResolve(std::move(results), __func__);
}

nsresult
nsEmbedClipboard::RequestClipboardData()
{
  NS_ENSURE_TRUE(mObserverService, NS_ERROR_FAILURE);

  if (!mWaitingForClipboardData) {
    nsresult rv =
      mObserverService->AddObserver(this, "embedui:clipboard", false);
    NS_ENSURE_SUCCESS(rv, rv);
    mWaitingForClipboardData = true;
  }

  nsString message;
  nsresult rv =
    mObserverService->NotifyObservers(nullptr, "clipboard:getdata",
                                      message.get());
  if (NS_FAILED(rv)) {
    StopObservingClipboardData();
  }
  return rv;
}

void
nsEmbedClipboard::StopObservingClipboardData()
{
  if (!mWaitingForClipboardData || !mObserverService) {
    return;
  }

  mObserverService->RemoveObserver(this, "embedui:clipboard");
  mWaitingForClipboardData = false;
}

void
nsEmbedClipboard::CancelPendingAsyncGetData(nsresult aReason)
{
  if (!mPendingAsyncGetData) {
    return;
  }

  mPendingAsyncGetData->mPromise.Reject(aReason, __func__);
  mPendingAsyncGetData = nullptr;
}

void
nsEmbedClipboard::CompletePendingAsyncGetData(const nsAString& aData)
{
  UniquePtr<PendingAsyncGetData> pending = std::move(mPendingAsyncGetData);
  nsresult rv = SetTransferableText(pending->mTransferable, aData);
  pending->mTransferable = nullptr;

  if (NS_FAILED(rv)) {
    pending->mPromise.Reject(rv, __func__);
  } else {
    pending->mPromise.Resolve(true, __func__);
  }
}

nsresult
nsEmbedClipboard::SetTransferableText(nsITransferable* aTransferable,
                                      const nsAString& aData)
{
  NS_ENSURE_ARG(aTransferable);

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataWrapper->SetData(aData);
  NS_ENSURE_SUCCESS(rv, rv);

  // If our data flavor has already been added, this will fail. But we don't care
  aTransferable->AddDataFlavor(kTextMime);

  nsCOMPtr<nsISupports> nsisupportsDataWrapper =
    do_QueryInterface(dataWrapper);
  return aTransferable->SetTransferData(kTextMime, nsisupportsDataWrapper);
}
