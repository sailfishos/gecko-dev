/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "GeckoLoader.h"
#include "DirProvider.h"
#include "mozilla/Unused.h"

#include <stdio.h>
#include "nscore.h"

// getenv
#include <stdlib.h>
#include <iostream>

// XRE_ Functions
#include "nsXULAppAPI.h"
#include "nsXREDirProvider.h"

// String
#include "nsString.h"

#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "nsDirectoryServiceDefs.h"
#include "EmbedLiteXulAppInfo.h"
#include "nsXULAppAPI.h"
#include "EmbedLiteXulAppInfo.h"
#include "mozilla/ModuleUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "GeckoProfiler.h"
#include "IOInterposer.h"

#ifdef XP_MACOSX
#include "MacQuirks.h"
#endif

#ifdef WIN32
//TODO: make this file fully X platform
#  include <windows.h>
#  undef MAX_PATH
#  define MAX_PATH _MAX_PATH
#else
#  include <unistd.h>
#  include <string.h>
#  ifndef PATH_MAX
#    define PATH_MAX 1024
#  endif
#  define MAX_PATH PATH_MAX
#endif

using namespace mozilla::embedlite;

static DirProvider kDirectoryProvider;
static bool sInitialized = false;

bool
GeckoLoader::InitEmbedding(const char* aProfilePath)
{
  if (sInitialized) {
    LOGW("Already initialized embedding\n");
    return false;
  }
  sInitialized = true;
  nsresult rv;

  static const char* sleepBeforeGeckoInit = getenv("SLEEP_BEFORE_EMBEDDING");
  if (sleepBeforeGeckoInit) {
    LOGF("Sleep for: %ss\n", sleepBeforeGeckoInit);
    PR_Sleep(atoi(sleepBeforeGeckoInit));
    printf("Start XRE Init Embedding\n");
  }

  // Get rid of the bogus TLS warnings
  // This will set this thread as the main thread.
  NS_LogInit();

  // This guard ensures that all threads that attempt to register themselves
  // with the IOInterposer will be properly tracked.
  // As TermEmbedding is called to stop embedding, let's do not use mozilla::IOInterposerInit
  // helper class here.
#if !defined(RELEASE_OR_BETA)
  IOInterposer::Init();
#endif

  // Android FF is using baseprofiler, I'm not sure if we'd benefit of it.
  // This call must happen before any other profiler calls and main thread
  // must be set. See GeckoProfiler.h.
#ifdef MOZ_GECKO_PROFILER
  char aLocal;
  profiler_init(&aLocal);
#endif

  const char* greHome = getenv("GRE_HOME");
  if (!greHome) {
    LOGE("GRE_HOME is not defined\n");
    return false;
  }

  nsCOMPtr<nsIFile> xuldir;
  rv = XRE_GetFileFromPath(greHome, getter_AddRefs(xuldir));
  if (NS_FAILED(rv)) {
    LOGE("Unable to create nsIFile for xuldir: %s\n", greHome);
    return false;
  }

  // create nsIFile pointing to appdir
  char self[MAX_PATH] = "";
#ifdef WIN32
  GetModuleFileNameA(GetModuleHandle(NULL), self, sizeof(self));
#else
  // TODO: works on linux, need solution for unices which do not support this
  ssize_t len;
  if ((len = readlink("/proc/self/exe", self, sizeof(self)-1)) != -1) {
    self[len] = '\0';
  }
#endif
  std::string selfPath(self);
  size_t lastslash_t = selfPath.find_last_of("/\\");
  if (lastslash_t == std::string::npos) {
    LOGE("Invalid module filename: %s", self);
#ifdef WIN32
    return false;
#else
    // A hackish way to get this initialized when starting through booster.
    selfPath = std::string("/usr/bin");
#endif
  } else {
    selfPath = selfPath.substr(0, lastslash_t);
  }

  nsCOMPtr<nsIFile> appdir;
  rv = XRE_GetFileFromPath(selfPath.c_str(), getter_AddRefs(appdir));
  if (NS_FAILED(rv)) {
    LOGE("Unable to create nsIFile for appdir: %s", selfPath.c_str());
    return false;
  }

  // setup profile dir
  if (aProfilePath) {
    nsCString pr(aProfilePath);
    if (!pr.IsEmpty()) {
      if (pr.First() != '/') {
#ifdef XP_WIN
        pr.Assign("c:");
#else
        pr.Assign(getenv("HOME"));
#endif
      }
      LOGF("Creating profile in:%s\n", pr.get());
      rv = NS_NewNativeLocalFile(pr, PR_FALSE,
                                 getter_AddRefs(kDirectoryProvider.sProfileDir));
      if (NS_FAILED(rv)) {
        LOGE("NS_NewNativeLocalFile failed.");
        return false;
      }
      kDirectoryProvider.sProfileDir->AppendNative(".mozilla"_ns);
      kDirectoryProvider.sProfileDir->AppendNative(nsDependentCString(aProfilePath));
    } else {
      // for now use a subdir under appdir
      nsCOMPtr<nsIFile> profFile;
      rv = appdir->Clone(getter_AddRefs(profFile));
      if (NS_FAILED(rv)) {
        LOGE("Unable to clone nsIFile.");
        return false;
      }

      kDirectoryProvider.sProfileDir = do_QueryInterface(profFile);
      kDirectoryProvider.sProfileDir->AppendNative("mozembed"_ns);
    }

    // create dir if needed
    bool dirExists = true;
    rv = kDirectoryProvider.sProfileDir->Exists(&dirExists);
    if (!dirExists) {
      mozilla::Unused << kDirectoryProvider.sProfileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    }

    // Lock profile directory
    if (kDirectoryProvider.sProfileDir && !kDirectoryProvider.sProfileLock) {
      rv = XRE_LockProfileDirectory(kDirectoryProvider.sProfileDir, &kDirectoryProvider.sProfileLock);
      if (NS_FAILED(rv)) {
        LOGE("Unable to lock profile directory.");
        return false;
      }
    }
  }

  nsCString greHomeCSTR(getenv("GRE_HOME"));
#ifdef XP_WIN
  greHomeCSTR.ReplaceChar('/', '\\');
#endif
  rv = NS_NewNativeLocalFile(greHomeCSTR, PR_FALSE,
                             getter_AddRefs(kDirectoryProvider.sGREDir));

  // xul application info component defined in embedding/embedlite/components/components.conf

  // init embedding
  rv = XRE_InitEmbedding2(xuldir, appdir,
                          const_cast<DirProvider*>(&kDirectoryProvider));
  if (NS_FAILED(rv)) {
    LOGE("XRE_InitEmbedding2 failed.");
    return false;
  }
  // XRE_InitEmbedding2 creates and sets global nsXREDirProvider
  RefPtr<nsXREDirProvider> XREDirProvider(nsXREDirProvider::GetSingleton());
  NS_WARN_IF(!XREDirProvider);
  if (XREDirProvider) {
    XREDirProvider->InitializeUserPrefs();
  }

  if (aProfilePath) {
    // initialize profile:
    XRE_NotifyProfile();
  }

  LOGF("InitEmbedding successfully");
  return true;
}

bool
GeckoLoader::TermEmbedding()
{
  if (!sInitialized) {
    LOGE("Not initialized embedding\n");
    return false;
  }
  sInitialized = false;

  // make sure this is freed before shutting down xpcom
  NS_IF_RELEASE(kDirectoryProvider.sProfileLock);
  kDirectoryProvider.sProfileDir = nullptr;
  kDirectoryProvider.sGREDir = nullptr;

  XRE_TermEmbedding();

#ifdef MOZ_GECKO_PROFILER
  // This must precede NS_LogTerm().
  profiler_shutdown();
#endif

  NS_LogTerm();

#if !defined(RELEASE_OR_BETA)
  IOInterposer::Clear();
#endif

  return true;
}
