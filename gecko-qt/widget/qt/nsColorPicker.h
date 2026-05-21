/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPicker_h__
#define nsColorPicker_h__

#include "nsCOMPtr.h"
#include "nsIColorPicker.h"
#include "nsIEmbedAppService.h"
#include "nsString.h"
#include "nsTArray.h"

class nsColorPicker final : public nsIColorPicker,
                            public nsIEmbedMessageListener {
 public:
  nsColorPicker();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOLORPICKER
  NS_DECL_NSIEMBEDMESSAGELISTENER

 private:
  ~nsColorPicker();

  nsresult EnsureWindowId(mozIDOMWindowProxy* aWindow);
  nsresult SendOpenMessage();
  void Finish(const nsAString& aColor);

  nsCOMPtr<nsIEmbedAppService> mEmbedAppService;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
  nsString mTitle;
  nsString mInitialColor;
  nsTArray<nsString> mDefaultColors;
  uint32_t mWinId;
  bool mListening;
};

#endif  // nsColorPicker_h__
