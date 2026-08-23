/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "nsIFile.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsAppRunner.h"
#include "nsCategoryManagerUtils.h"

#include "mozilla/GeckoArgs.h"
#include "mozilla/Omnijar.h"
#include "mozilla/ipc/ProcessUtils.h"

#include "EmbedLiteContentProcess.h"
#include "EmbedLiteAppProcessChild.h"

namespace mozilla {
namespace embedlite {

static nsresult GetGREDir(nsIFile** aResult)
{
  nsCOMPtr<nsIFile> current;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(current));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> parent;
  rv = current->GetParent(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

  parent.forget(aResult);
  return NS_OK;
}

EmbedLiteContentProcess::EmbedLiteContentProcess(
    IPC::Channel::ChannelHandle aClientChannel,
    ProcessId aParentHandle,
    const nsID& aMessageChannelId)
  : ProcessChild(std::move(aClientChannel), aParentHandle, aMessageChannelId)
{
  mContent = new EmbedLiteAppProcessChild();
}

EmbedLiteContentProcess::~EmbedLiteContentProcess()
{
  delete mContent;
}

bool
EmbedLiteContentProcess::Init(int aArgc, char* aArgv[])
{
  LOGT();

  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  Maybe<mozilla::ipc::ReadOnlySharedMemoryHandle> jsInitHandle =
      geckoargs::sJsInitHandle.Get(aArgc, aArgv);

  nsCOMPtr<nsIFile> appDir;
  Maybe<const char*> appDirArg = geckoargs::sAppDir.Get(aArgc, aArgv);
  if (appDirArg.isSome()) {
    bool exists;
    nsresult rv = XRE_GetFileFromPath(*appDirArg, getter_AddRefs(appDir));
    if (NS_FAILED(rv) || NS_FAILED(appDir->Exists(&exists)) || !exists) {
      return false;
    }
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  if (jsInitHandle &&
      !mozilla::ipc::ImportSharedJSInit(jsInitHandle.extract())) {
    return false;
  }

  if (!mContent->Init(ParentPid(), TakeInitialEndpoint(), *parentBuildID)) {
    return false;
  }

  nsCOMPtr<nsIFile> greDir;
  nsresult rv = GetGREDir(getter_AddRefs(greDir));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIFile> xpcomAppDir = appDir ? appDir : greDir;

  rv = mDirProvider.Initialize(xpcomAppDir, greDir);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!Omnijar::IsInitialized()) {
    Omnijar::ChildProcessInit(aArgc, aArgv);
  }

  rv = NS_InitXPCOM(nullptr, xpcomAppDir, &mDirProvider);
  if (NS_FAILED(rv)) {
    return false;
  }

  NS_CreateServicesFromCategory("app-startup", nullptr, "app-startup",
                                nullptr);

  mContent->InitXPCOM();

  return true;
}

void
EmbedLiteContentProcess::CleanUp()
{
  LOGT();
  mDirProvider.DoShutdown();
  NS_ShutdownXPCOM(nullptr);
}

} // namespace embedlite
} // namespace mozilla
