/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFilePicker.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "mozilla/dom/BrowsingContext.h"
#include "nsArrayEnumerator.h"
#include "nsComponentManagerUtils.h"
#include "nsIEmbedLiteJSON.h"
#include "nsIFile.h"
#include "nsIPropertyBag2.h"
#include "nsISimpleEnumerator.h"
#include "nsIURI.h"
#include "nsIVariant.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"

NS_IMPL_ISUPPORTS(nsFilePicker, nsIFilePicker, nsIEmbedMessageListener)

namespace {

const char kFilePickerResponse[] = "filepickerresponse";

bool ContainsAny(const nsCString& aValue,
                 const std::initializer_list<const char*>& aNeedles) {
  for (const char* needle : aNeedles) {
    if (aValue.Find(needle) != kNotFound) {
      return true;
    }
  }
  return false;
}

}  // namespace

nsFilePicker::nsFilePicker()
    : mSelectedType(0), mWinId(0), mListening(false) {}

nsFilePicker::~nsFilePicker() {
  if (mListening && mEmbedAppService) {
    mEmbedAppService->RemoveMessageListener(kFilePickerResponse, this);
  }
}

NS_IMETHODIMP
nsFilePicker::Init(mozilla::dom::BrowsingContext* aBrowsingContext,
                   const nsAString& aTitle, nsIFilePicker::Mode aMode,
                   nsISupports* aGlobal) {
  nsresult rv =
      nsBaseFilePicker::Init(aBrowsingContext, aTitle, aMode, aGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  mTitle = aTitle;

  mEmbedAppService = do_GetService("@mozilla.org/embedlite-app-service;1");
  NS_ENSURE_TRUE(mEmbedAppService, NS_ERROR_FAILURE);

  rv = mEmbedAppService->GetIDByBrowsingContext(aBrowsingContext, &mWinId);
  if (NS_SUCCEEDED(rv) && mWinId) {
    return NS_OK;
  }

  return EnsureWindowId(aBrowsingContext->GetDOMWindow());
}

void nsFilePicker::InitNative(nsIWidget* aParent, const nsAString& aTitle) {}

nsresult nsFilePicker::EnsureWindowId(mozIDOMWindowProxy* aWindow) {
  if (aWindow) {
    nsresult rv = mEmbedAppService->GetIDByWindow(aWindow, &mWinId);
    if (NS_SUCCEEDED(rv) && mWinId) {
      return NS_OK;
    }
  }

  nsCOMPtr<mozIDOMWindowProxy> activeWindow;
  nsresult rv =
      mEmbedAppService->GetAnyEmbedWindow(true, getter_AddRefs(activeWindow));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(activeWindow, NS_ERROR_FAILURE);

  rv = mEmbedAppService->GetIDByWindow(activeWindow, &mWinId);
  NS_ENSURE_SUCCESS(rv, rv);
  return mWinId ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle,
                           const nsAString& aFilter) {
  mFilterNames.AppendElement(aTitle);
  mFilters.AppendElement(aFilter);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString& aString) {
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString& aString) {
  aString = mDefault;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString& aExtension) {
  mDefaultExtension = aExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString& aExtension) {
  aExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFilterIndex(int32_t* aFilterIndex) {
  NS_ENSURE_ARG_POINTER(aFilterIndex);
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(int32_t aFilterIndex) {
  mSelectedType = aFilterIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFile(nsIFile** aFile) {
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nullptr;

  if (mFiles.Count() == 0) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> file = mFiles[0];
  file.forget(aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIURI** aFileURL) {
  NS_ENSURE_ARG_POINTER(aFileURL);
  *aFileURL = nullptr;

  if (mFiles.Count() == 0) {
    return NS_OK;
  }

  return NS_NewFileURI(aFileURL, mFiles[0]);
}

NS_IMETHODIMP
nsFilePicker::GetFiles(nsISimpleEnumerator** aFiles) {
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles, NS_GET_IID(nsIFile));
}

NS_IMETHODIMP
nsFilePicker::Open(nsIFilePickerShownCallback* aCallback) {
  NS_ENSURE_ARG_POINTER(aCallback);

  if (mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (MaybeBlockFilePicker(aCallback)) {
    return NS_OK;
  }

  mFiles.Clear();
  mCallback = aCallback;

  if (mMode != nsIFilePicker::modeOpen &&
      mMode != nsIFilePicker::modeOpenMultiple) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  nsresult rv =
      mEmbedAppService->AddMessageListener(kFilePickerResponse, this);
  if (NS_FAILED(rv)) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }
  mListening = true;

  rv = SendOpenMessage();
  if (NS_FAILED(rv)) {
    Finish(nsIFilePicker::returnCancel);
  }

  return NS_OK;
}

nsresult nsFilePicker::SendOpenMessage() {
  nsAutoString message;
  message.AppendLiteral(u"{\"winId\":");
  message.AppendInt(mWinId);
  message.AppendLiteral(u",\"mode\":");
  message.AppendInt(mMode);
  message.AppendLiteral(u",\"mimeType\":\"");
  AppendUTF8toUTF16(MimeType(), message);
  message.AppendLiteral(u"\"}");

  return mEmbedAppService->SendAsyncMessage(mWinId, u"embed:filepicker",
                                            message.get());
}

NS_IMETHODIMP
nsFilePicker::OnMessageReceived(const char* aMessageName,
                                const char16_t* aMessage) {
  if (strcmp(aMessageName, kFilePickerResponse)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEmbedLiteJSON> json =
      do_GetService("@mozilla.org/embedlite-json;1");
  if (!json) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  nsCOMPtr<nsIPropertyBag2> response;
  nsresult rv =
      json->ParseJSON(nsDependentString(aMessage), getter_AddRefs(response));
  if (NS_FAILED(rv)) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  uint32_t winId = 0;
  response->GetPropertyAsUint32(u"winId"_ns, &winId);
  if (winId && winId != mWinId) {
    return NS_OK;
  }

  bool accepted = false;
  response->GetPropertyAsBool(u"accepted"_ns, &accepted);
  if (!accepted) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  nsCOMPtr<nsIVariant> items;
  rv = response->Get(u"items"_ns, getter_AddRefs(items));
  if (NS_FAILED(rv) || !items) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  uint16_t elementType;
  nsIID elementIID;
  uint32_t itemCount;
  void* itemArray = nullptr;
  rv = items->GetAsArray(&elementType, &elementIID, &itemCount, &itemArray);
  if (NS_FAILED(rv)) {
    Finish(nsIFilePicker::returnCancel);
    return NS_OK;
  }

  if (elementType == nsIDataType::VTYPE_INTERFACE_IS &&
      elementIID.Equals(NS_GET_IID(nsIVariant))) {
    nsISupports** supportsArray = static_cast<nsISupports**>(itemArray);
    for (uint32_t i = 0; i < itemCount; ++i) {
      nsCOMPtr<nsIVariant> item = do_QueryInterface(supportsArray[i]);
      if (item) {
        nsAutoString path;
        if (NS_SUCCEEDED(item->GetAsAString(path))) {
          AddSelectedFile(path);
        }
      }
      NS_IF_RELEASE(supportsArray[i]);
    }
  }
  free(itemArray);

  Finish(mFiles.Count() ? nsIFilePicker::returnOK
                        : nsIFilePicker::returnCancel);
  return NS_OK;
}

void nsFilePicker::Finish(nsIFilePicker::ResultCode aResult) {
  if (mListening && mEmbedAppService) {
    mEmbedAppService->RemoveMessageListener(kFilePickerResponse, this);
    mListening = false;
  }

  nsCOMPtr<nsIFilePickerShownCallback> callback = mCallback.forget();
  if (callback) {
    callback->Done(aResult);
  }
}

void nsFilePicker::AddSelectedFile(const nsAString& aPath) {
  if (aPath.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aPath, getter_AddRefs(file));
  if (NS_SUCCEEDED(rv)) {
    mFiles.AppendObject(file);
  }
}

nsCString nsFilePicker::MimeType() const {
  nsAutoString joined;
  for (const nsString& filter : mRawFilters) {
    joined.Append(filter);
    joined.AppendLiteral(u" ");
  }
  for (const nsString& filter : mFilters) {
    joined.Append(filter);
    joined.AppendLiteral(u" ");
  }

  ToLowerCase(joined);

  nsAutoCString filters;
  CopyUTF16toUTF8(joined, filters);

  if (ContainsAny(filters, {"image/", "*.apng", "*.avif", "*.bmp", "*.gif",
                            "*.ico", "*.jpe", "*.jpg", "*.jpeg", "*.png",
                            "*.svg", "*.tif", "*.tiff", "*.webp"})) {
    return "image/*"_ns;
  }

  if (ContainsAny(filters, {"audio/", "*.aac", "*.flac", "*.m4a", "*.mp3",
                            "*.oga", "*.ogg", "*.opus", "*.wav", "*.weba"})) {
    return "audio/*"_ns;
  }

  if (ContainsAny(filters, {"video/", "*.avi", "*.m4v", "*.mkv", "*.mov",
                            "*.mp4", "*.mpeg", "*.mpg", "*.ogv", "*.webm"})) {
    return "video/*"_ns;
  }

  return ""_ns;
}
