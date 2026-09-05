/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include "nsBaseFilePicker.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIEmbedAppService.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIFile;
class nsIURI;
class nsIWidget;

class nsFilePicker final : public nsBaseFilePicker,
                           public nsIEmbedMessageListener {
 public:
  nsFilePicker();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEMBEDMESSAGELISTENER

  NS_IMETHOD Init(mozilla::dom::BrowsingContext* aBrowsingContext,
                  const nsAString& aTitle,
                  nsIFilePicker::Mode aMode,
                  nsISupports* aGlobal) override;
  NS_IMETHOD Open(nsIFilePickerShownCallback* aCallback) override;
  NS_IMETHOD AppendFilter(const nsAString& aTitle,
                          const nsAString& aFilter) override;
  NS_IMETHOD SetDefaultString(const nsAString& aString) override;
  NS_IMETHOD GetDefaultString(nsAString& aString) override;
  NS_IMETHOD SetDefaultExtension(const nsAString& aExtension) override;
  NS_IMETHOD GetDefaultExtension(nsAString& aExtension) override;
  NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD GetFile(nsIFile** aFile) override;
  NS_IMETHOD GetFileURL(nsIURI** aFileURL) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;

  void InitNative(nsIWidget* aParent, const nsAString& aTitle) override;

 protected:
  ~nsFilePicker() override;

 private:
  nsresult EnsureWindowId(mozIDOMWindowProxy* aWindow);
  nsresult SendOpenMessage();
  void Finish(nsIFilePicker::ResultCode aResult);
  void AddSelectedFile(const nsAString& aPath);
  nsCString MimeType() const;

  nsCOMPtr<nsIEmbedAppService> mEmbedAppService;
  nsCOMPtr<nsIFilePickerShownCallback> mCallback;
  nsCOMArray<nsIFile> mFiles;
  nsTArray<nsString> mFilters;
  nsTArray<nsString> mFilterNames;
  nsString mTitle;
  nsString mDefault;
  nsString mDefaultExtension;
  int16_t mSelectedType;
  uint32_t mWinId;
  bool mListening;
};

#endif
