/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "DirProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsSimpleEnumerator.h"

#define NS_SYSTEM_SEARCH_DIR_KEY "SystemSearchDir"
#define NS_USER_SEARCH_DIR_KEY "UserSearchDir"

nsIDirectoryServiceProvider* DirProvider::sAppFileLocProvider = nullptr;
nsCOMPtr<nsIFile> DirProvider::sProfileDir = nullptr;
nsCOMPtr<nsIFile> DirProvider::sGREDir = nullptr;
nsISupports* DirProvider::sProfileLock = nullptr;

NS_IMPL_QUERY_INTERFACE(DirProvider,
                        nsIDirectoryServiceProvider,
                        nsIDirectoryServiceProvider2)

NS_IMETHODIMP_(MozExternalRefCountType)
DirProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(MozExternalRefCountType)
DirProvider::Release()
{
  return 0;
}

NS_IMETHODIMP
DirProvider::GetFile(const char* aKey, bool* aPersist,
                     nsIFile* *aResult)
{
  if (!strcmp(aKey, NS_SYSTEM_SEARCH_DIR_KEY)) {
    nsCOMPtr<nsIFile> file;
    nsresult rv;
#if defined(HAVE_USR_LIB64_DIR) && defined(__LP64__)
    rv = NS_NewNativeLocalFile(nsDependentCString("/usr/lib64/mozembedlite/chrome/embedlite/content"), false, getter_AddRefs(file));
#else
    rv = NS_NewNativeLocalFile(nsDependentCString("/usr/lib/mozembedlite/chrome/embedlite/content"), false, getter_AddRefs(file));
#endif
    if (file && NS_SUCCEEDED(rv))
      file.forget(aResult);
    return rv;
  }

  if (!strcmp(aKey, NS_USER_SEARCH_DIR_KEY)) {
    nsCOMPtr<nsIFile> file;
    nsresult rv;
    if (sProfileDir) {
      *aPersist = true;
      rv = sProfileDir->Clone(getter_AddRefs(file));
      if (NS_SUCCEEDED(rv)) {
        rv = file->AppendNative("searchEngines"_ns);
      }
    } else {
      rv = NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), true, getter_AddRefs(file));
      if (NS_SUCCEEDED(rv)) {
        rv = file->AppendRelativeNativePath(".local/share/org.sailfishos/browser/searchEngines"_ns);
      }

    }
    if (file && NS_SUCCEEDED(rv)) {
      file.forget(aResult);
    }
    return rv;
  }

  if (sAppFileLocProvider) {
    nsresult rv = sAppFileLocProvider->GetFile(aKey, aPersist, aResult);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }
  }

  if ((sGREDir && !strcmp(aKey, NS_GRE_DIR)) || !strcmp(aKey, NS_XPCOM_CURRENT_PROCESS_DIR)) {
    *aPersist = true;
    return sGREDir->Clone(aResult);
  }

  if (!sProfileDir) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_50_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_LOCAL_50_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_PROFILE_DIR_STARTUP)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_CACHE_PARENT_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_PREF_DEFAULTS_50_DIR)) {
    nsCOMPtr<nsIFile> file;
    nsresult rv = sGREDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative("defaults"_ns);
      if (NS_SUCCEEDED(rv)) {
        rv = file->AppendNative("pref"_ns);
        NS_ADDREF(*aResult = file);
        return rv;
      }
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

// Enumerator copied from nsAppFileLocationProvider.cpp
class nsAppDirectoryEnumerator : public nsSimpleEnumerator {
 public:
  /**
   * aKeyList is a null-terminated list of properties which are provided by
   * aProvider They do not need to be publicly defined keys.
   */
  nsAppDirectoryEnumerator(nsIDirectoryServiceProvider* aProvider,
                           const char* aKeyList[])
      : mProvider(aProvider), mCurrentKey(aKeyList) {}

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

  NS_IMETHOD HasMoreElements(bool* aResult) override {
    while (!mNext && *mCurrentKey) {
      bool dontCare;
      nsCOMPtr<nsIFile> testFile;
      (void)mProvider->GetFile(*mCurrentKey++, &dontCare,
                               getter_AddRefs(testFile));
      mNext = testFile;
    }
    *aResult = mNext != nullptr;
    return NS_OK;
  }

  NS_IMETHOD GetNext(nsISupports** aResult) override {
    if (NS_WARN_IF(!aResult)) {
      return NS_ERROR_INVALID_ARG;
    }
    *aResult = nullptr;

    bool hasMore;
    HasMoreElements(&hasMore);
    if (!hasMore) {
      return NS_ERROR_FAILURE;
    }

    *aResult = mNext;
    NS_IF_ADDREF(*aResult);
    mNext = nullptr;

    return *aResult ? NS_OK : NS_ERROR_FAILURE;
  }

 protected:
  nsCOMPtr<nsIDirectoryServiceProvider> mProvider;
  const char** mCurrentKey;
  nsCOMPtr<nsIFile> mNext;
};

NS_IMETHODIMP
DirProvider::GetFiles(const char* aKey,
                      nsISimpleEnumerator* *aResult)
{
  if (!strcmp(aKey, NS_APP_DISTRIBUTION_SEARCH_DIR_LIST)) {
    static const char* keys[] = {NS_USER_SEARCH_DIR_KEY, NS_SYSTEM_SEARCH_DIR_KEY, nullptr};
    *aResult = new nsAppDirectoryEnumerator(this, keys);
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  nsCOMPtr<nsIDirectoryServiceProvider2>
  dp2(do_QueryInterface(sAppFileLocProvider));

  if (!dp2) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return dp2->GetFiles(aKey, aResult);
}
