/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedInitGlue.h"

#include "mozilla/Bootstrap.h"
#include "application.ini.h"

// getenv
#include <stdlib.h>
#include <iostream>
#include <stdio.h>

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define MAX_PATH PATH_MAX

#define _NSR_TO_BOOL(_res) \
  _res == NS_OK ? true : false;

using namespace mozilla;

mozilla::Bootstrap::UniquePtr gBootstrap;

static bool IsLibXulInThePath(const char* path, std::string& xpcomPath) {
  xpcomPath = path;
  xpcomPath += "/libxul";
  xpcomPath += MOZ_DLL_SUFFIX;
  struct stat buf;
  return !stat(xpcomPath.c_str(), &buf);
}

static std::string ResolveXPCOMPath(int argc, char** argv) {
    // find xpcom shared lib (uses GRE_HOME env var if set, current DIR, or executable binary path)
    std::string xpcomPath;
    char temp[MAX_PATH];
    char* greHome = getenv("GRE_HOME");

    if (!greHome) {
      greHome = getenv("PWD");
      if (greHome) {
        printf("greHome from PWD:%s\n", greHome);
      }
    } else {
      printf("greHome from GRE_HOME:%s\n", greHome);
    }
    if (!greHome) {
      printf("GRE_HOME is not defined\n");
      return "";
    }
    if (!IsLibXulInThePath(greHome, xpcomPath)) {
      if (argv && argc) {
        printf("libxul.so not in gre home or PWD:%s, check in executable path\n", greHome);
        char* lastslash = strrchr(argv[0], '/');
        size_t path_size = &lastslash[0] - argv[0];
        strncpy(temp, argv[0], path_size);
        temp[path_size] = 0;
        greHome = &temp[0];
      }
    }
    if (!IsLibXulInThePath(greHome, xpcomPath)) {
      printf("libxul.so is not found, in %s\n", xpcomPath.c_str());
      greHome = getenv("BUILD_GRE_HOME");
    }

    if (!IsLibXulInThePath(greHome, xpcomPath)) {
      printf("libxul.so is not found, in %s return fail\n", xpcomPath.c_str());
      return "";
    }

    char* greHomeLeak = strdup(greHome);
    setenv("GRE_HOME", greHomeLeak, 1);
    setenv("MOZILLA_FIVE_HOME", greHomeLeak, 1);
    setenv("XRE_LIBXPCOM_PATH", strdup(xpcomPath.c_str()), 1);

    return xpcomPath;
}

bool LoadEmbedLite(int argc, char** argv)
{
  // start the glue, i.e. load and link against xpcom shared lib
  std::string xpcomPath = ResolveXPCOMPath(argc, argv);
  gBootstrap = mozilla::GetBootstrap(xpcomPath.c_str());
  if (!gBootstrap) {
    printf("Couldn't load XPCOM from %s\n", xpcomPath.c_str());
    return false;
  }
  return true;
}
