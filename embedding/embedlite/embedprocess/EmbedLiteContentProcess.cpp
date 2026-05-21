/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "nsIFile.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

#include "EmbedLiteContentProcess.h"
#include "EmbedLiteAppProcessChild.h"

namespace mozilla {
namespace embedlite {

EmbedLiteContentProcess::EmbedLiteContentProcess(ProcessId aParentHandle,
                                                 const nsID& aMessageChannelId)
  : ProcessChild(aParentHandle, aMessageChannelId)
{
  mContent = new EmbedLiteAppProcessChild();
}

EmbedLiteContentProcess::~EmbedLiteContentProcess()
{
  delete mContent;
}

void
EmbedLiteContentProcess::SetAppDir(const nsACString& aPath)
{
  mAppDir.Assign(aPath);
}

bool
EmbedLiteContentProcess::Init(int aArgc, char* aArgv[])
{
  (void)aArgc;
  (void)aArgv;

  LOGT();
  mContent->Init(ParentPid(), TakeInitialEndpoint());

  nsCOMPtr<nsIFile> appDir;
  if (!mAppDir.IsEmpty()) {
    nsresult rv = XRE_GetFileFromPath(mAppDir.get(), getter_AddRefs(appDir));
    if (NS_FAILED(rv)) {
      return false;
    }
  }

  nsresult rv = NS_InitXPCOM(nullptr, appDir, nullptr);
  if (NS_FAILED(rv)) {
    return false;
  }

  mContent->InitXPCOM();

  return true;
}

void
EmbedLiteContentProcess::CleanUp()
{
  LOGT();
  NS_ShutdownXPCOM(nullptr);
}

} // namespace embedlite
} // namespace mozilla
