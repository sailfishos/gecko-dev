/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedInitGlue.h"

#include "mozilla/Bootstrap.h"
#include "nsXPCOMPrivate.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <string>

using namespace mozilla;

Bootstrap::UniquePtr gBootstrap;

static bool FindLibXul(const char* aDirectory, std::string& aXPCOMPath) {
  if (!aDirectory || !*aDirectory) {
    return false;
  }

  aXPCOMPath = aDirectory;
  if (aXPCOMPath.back() != '/') {
    aXPCOMPath += '/';
  }
  aXPCOMPath += XPCOM_DLL;

  struct stat buf;
  return stat(aXPCOMPath.c_str(), &buf) == 0;
}

static std::string ResolveXPCOMPath() {
  std::string xpcomPath;

  // The embedder supplies its packaged, absolute GRE directory. Do not fall
  // back to the working or executable directory: either may be writable by an
  // untrusted process and must not influence library loading.
  const char* buildHome = getenv("BUILD_GRE_HOME");
  if (!buildHome || buildHome[0] != '/' ||
      !FindLibXul(buildHome, xpcomPath)) {
    return {};
  }

  setenv("GRE_HOME", buildHome, 1);
  setenv("MOZILLA_FIVE_HOME", buildHome, 1);
  setenv("XRE_LIBXPCOM_PATH", xpcomPath.c_str(), 1);
  return xpcomPath;
}

bool LoadEmbedLite(int, char**) {
  std::string xpcomPath = ResolveXPCOMPath();
  if (xpcomPath.empty()) {
    printf("Couldn't find %s\n", XPCOM_DLL);
    return false;
  }

  BootstrapResult bootstrapResult = mozilla::GetBootstrap(xpcomPath.c_str());
  if (bootstrapResult.isErr()) {
    printf("Couldn't load XPCOM from %s\n", xpcomPath.c_str());
    return false;
  }
  gBootstrap = bootstrapResult.unwrap();
  return true;
}
