/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsColorPicker.h"

#include <cstring>

#include "mozilla/dom/HTMLInputElement.h"
#include "nsIEmbedLiteJSON.h"
#include "nsIPropertyBag2.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

using mozilla::dom::HTMLInputElement;

NS_IMPL_ISUPPORTS(nsColorPicker, nsIColorPicker, nsIEmbedMessageListener)

namespace {

const char kColorPickerResponse[] = "embedui:colorpickerresponse";

bool NormalizeSimpleColor(const nsAString& aColor, nsAString& aResult) {
  if (HTMLInputElement::ParseSimpleColor(aColor).isNothing()) {
    return false;
  }

  aResult.Assign(aColor);
  ToLowerCase(aResult);
  return true;
}

}  // namespace

nsColorPicker::nsColorPicker() : mWinId(0), mListening(false) {}

nsColorPicker::~nsColorPicker() {
  if (mListening && mEmbedAppService) {
    mEmbedAppService->RemoveMessageListener(kColorPickerResponse, this);
  }
}

NS_IMETHODIMP
nsColorPicker::Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                    const nsAString& aInitialColor,
                    const nsTArray<nsString>& aDefaultColors) {
  NS_ENSURE_TRUE(aParent, NS_ERROR_FAILURE);

  if (!NormalizeSimpleColor(aInitialColor, mInitialColor)) {
    return NS_ERROR_FAILURE;
  }

  mTitle = aTitle;
  mDefaultColors.Clear();
  for (const nsString& color : aDefaultColors) {
    nsAutoString normalizedColor;
    if (NormalizeSimpleColor(color, normalizedColor)) {
      mDefaultColors.AppendElement(normalizedColor);
    }
  }

  mEmbedAppService = do_GetService("@mozilla.org/embedlite-app-service;1");
  NS_ENSURE_TRUE(mEmbedAppService, NS_ERROR_FAILURE);

  return EnsureWindowId(aParent);
}

NS_IMETHODIMP
nsColorPicker::Open(nsIColorPickerShownCallback* aCallback) {
  NS_ENSURE_ARG(aCallback);

  if (mCallback) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCallback = aCallback;

  nsresult rv =
      mEmbedAppService->AddMessageListener(kColorPickerResponse, this);
  if (NS_FAILED(rv)) {
    mCallback = nullptr;
    return rv;
  }
  mListening = true;

  rv = SendOpenMessage();
  if (NS_FAILED(rv)) {
    mEmbedAppService->RemoveMessageListener(kColorPickerResponse, this);
    mListening = false;
    mCallback = nullptr;
    return rv;
  }

  return NS_OK;
}

nsresult nsColorPicker::EnsureWindowId(mozIDOMWindowProxy* aWindow) {
  nsresult rv = mEmbedAppService->GetIDByWindow(aWindow, &mWinId);
  if (NS_SUCCEEDED(rv) && mWinId) {
    return NS_OK;
  }

  nsCOMPtr<mozIDOMWindowProxy> activeWindow;
  rv = mEmbedAppService->GetAnyEmbedWindow(true, getter_AddRefs(activeWindow));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(activeWindow, NS_ERROR_FAILURE);

  rv = mEmbedAppService->GetIDByWindow(activeWindow, &mWinId);
  NS_ENSURE_SUCCESS(rv, rv);
  return mWinId ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsColorPicker::SendOpenMessage() {
  nsAutoString message;
  message.AppendLiteral(u"{\"winId\":");
  message.AppendInt(mWinId);
  message.AppendLiteral(u",\"initialColor\":\"");
  message.Append(mInitialColor);
  message.AppendLiteral(u"\",\"defaultColors\":[");

  for (uint32_t i = 0; i < mDefaultColors.Length(); ++i) {
    if (i) {
      message.AppendLiteral(u",");
    }
    message.AppendLiteral(u"\"");
    message.Append(mDefaultColors[i]);
    message.AppendLiteral(u"\"");
  }

  message.AppendLiteral(u"]}");

  return mEmbedAppService->SendAsyncMessage(mWinId, u"embed:colorpicker",
                                            message.get());
}

NS_IMETHODIMP
nsColorPicker::OnMessageReceived(const char* aMessageName,
                                 const char16_t* aMessage) {
  if (strcmp(aMessageName, kColorPickerResponse)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEmbedLiteJSON> json =
      do_GetService("@mozilla.org/embedlite-json;1");
  if (!json) {
    Finish(EmptyString());
    return NS_OK;
  }

  nsCOMPtr<nsIPropertyBag2> response;
  nsresult rv =
      json->ParseJSON(nsDependentString(aMessage), getter_AddRefs(response));
  if (NS_FAILED(rv)) {
    Finish(EmptyString());
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
    Finish(EmptyString());
    return NS_OK;
  }

  nsAutoString color;
  rv = response->GetPropertyAsAString(u"color"_ns, color);
  if (NS_FAILED(rv)) {
    Finish(EmptyString());
    return NS_OK;
  }

  nsAutoString normalizedColor;
  if (NormalizeSimpleColor(color, normalizedColor)) {
    Finish(normalizedColor);
  } else {
    Finish(EmptyString());
  }
  return NS_OK;
}

void nsColorPicker::Finish(const nsAString& aColor) {
  if (mListening && mEmbedAppService) {
    mEmbedAppService->RemoveMessageListener(kColorPickerResponse, this);
    mListening = false;
  }

  nsCOMPtr<nsIColorPickerShownCallback> callback = mCallback.forget();
  if (callback) {
    callback->Done(aColor);
  }
}
