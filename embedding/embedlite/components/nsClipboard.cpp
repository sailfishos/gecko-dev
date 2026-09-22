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

static Result<nsCOMPtr<nsISupports>, nsresult>
CreateTextData(const nsAString& aData)
{
  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  rv = dataWrapper->SetData(aData);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  return nsCOMPtr<nsISupports>(std::move(dataWrapper));
}

struct nsEmbedClipboard::PendingAsyncGetData {
  explicit PendingAsyncGetData(GetNativeDataCallback&& aCallback)
    : mCallback(std::move(aCallback))
  {
  }

  GetNativeDataCallback mCallback;
};

NS_IMPL_ISUPPORTS_INHERITED(nsEmbedClipboard, nsBaseClipboard,
                            nsIObserver)

nsEmbedClipboard::nsEmbedClipboard()
  : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
      false /* supportsSelectionClipboard */,
      false /* supportsFindClipboard */,
      false /* supportsSelectionCache */))
  , mSequenceNumber(0)
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
                                         ClipboardType aWhichClipboard)
{
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

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
  rv = mObserverService->NotifyObservers(nullptr, "clipboard:setdata",
                                         message.get());
  NS_ENSURE_SUCCESS(rv, rv);
  ++mSequenceNumber;

  return NS_OK;
}

Result<nsCOMPtr<nsISupports>, nsresult>
nsEmbedClipboard::GetNativeClipboardData(const nsACString& aFlavor,
                                         ClipboardType aWhichClipboard,
                                         uint64_t aThreshold)
{
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (!aFlavor.EqualsLiteral(kTextMime)) {
    return nsCOMPtr<nsISupports>{};
  }

  CancelPendingAsyncGetData(NS_ERROR_ABORT);

  const int origModalDepth = mModalDepth;
  nsresult rv = RequestClipboardData();
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCOMPtr<nsIThread> thread;
  rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    StopObservingClipboardData();
    return Err(rv);
  }
  while (mActive && mModalDepth == origModalDepth && NS_SUCCEEDED(rv)) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(true, &processedEvent);
    if (NS_SUCCEEDED(rv) && !processedEvent) {
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  if (NS_FAILED(rv)) {
    StopObservingClipboardData();
    return Err(rv);
  }
  if (!mActive) {
    return Err(NS_ERROR_ABORT);
  }
  if (aThreshold &&
      uint64_t(mBuffer.Length()) * sizeof(char16_t) > aThreshold) {
    mBuffer.Truncate();
    return Err(NS_ERROR_CLIPBOARD_TOO_BIG);
  }

  auto result = CreateTextData(mBuffer);
  mBuffer.Truncate();
  return result;
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

Result<bool, nsresult>
nsEmbedClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList,
    ClipboardType aWhichClipboard)
{
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  for (const auto& flavor : aFlavorList) {
    if (flavor.EqualsLiteral(kTextMime)) {
      return true;
    }
  }
  return false;
}

nsresult
nsEmbedClipboard::EmptyNativeClipboardData(ClipboardType aWhichClipboard)
{
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  nsString message;
  nsCOMPtr<nsIEmbedLiteJSON> json = do_GetService("@mozilla.org/embedlite-json;1");
  nsCOMPtr<nsIWritablePropertyBag2> root;
  json->CreateObject(getter_AddRefs(root));
  root->SetPropertyAsAString(u"data"_ns, u""_ns);
  root->SetPropertyAsBool(u"private"_ns, false);

  json->CreateJSON(root, message);
  nsresult rv = mObserverService->NotifyObservers(
    nullptr, "clipboard:setdata", message.get());
  NS_ENSURE_SUCCESS(rv, rv);

  mBuffer.Truncate();
  ++mSequenceNumber;

  return NS_OK;
}

Result<int32_t, nsresult>
nsEmbedClipboard::GetNativeClipboardSequenceNumber(
    ClipboardType aWhichClipboard)
{
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  return mSequenceNumber;
}

void
nsEmbedClipboard::AsyncGetNativeClipboardData(
    const nsACString& aFlavor, ClipboardType aWhichClipboard,
    GetNativeDataCallback&& aCallback)
{
  MOZ_DIAGNOSTIC_ASSERT(
    nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  CancelPendingAsyncGetData(NS_ERROR_ABORT);
  if (!aFlavor.EqualsLiteral(kTextMime)) {
    aCallback(nsCOMPtr<nsISupports>{});
    return;
  }

  mPendingAsyncGetData =
    MakeUnique<PendingAsyncGetData>(std::move(aCallback));

  nsresult rv = RequestClipboardData();
  if (NS_FAILED(rv)) {
    CancelPendingAsyncGetData(rv);
  }
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

  UniquePtr<PendingAsyncGetData> pending = std::move(mPendingAsyncGetData);
  pending->mCallback(Err(aReason));
}

void
nsEmbedClipboard::CompletePendingAsyncGetData(const nsAString& aData)
{
  UniquePtr<PendingAsyncGetData> pending = std::move(mPendingAsyncGetData);
  pending->mCallback(CreateTextData(aData));
}
