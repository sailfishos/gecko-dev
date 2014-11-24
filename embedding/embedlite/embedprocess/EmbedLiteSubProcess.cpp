/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "nsXPCOMPrivate.h"
#include "EmbedLiteSubProcess.h"
#include "GeckoLoader.h"
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"
#include "EmbedLiteAppProcessParent.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "nsThreadManager.h"
#include "nsAutoPtr.h"
#include "base/command_line.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

using namespace mozilla::ipc;
using namespace mozilla::dom;
using namespace mozilla::layers;
static BrowserProcessSubThread* sIOThread;

namespace mozilla {
namespace embedlite {

//
// EmbedLiteSubProcess
//

EmbedLiteSubProcess::EmbedLiteSubProcess()
  : GeckoChildProcessHost(GeckoProcessType_Content)
{
  LOGT();
}

EmbedLiteSubProcess::~EmbedLiteSubProcess()
{
  LOGT();
}

void EmbedLiteSubProcess::StartEmbedProcess()
{
  LOGT();
  if (!BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)) {
      UniquePtr<BrowserProcessSubThread> ioThread(new BrowserProcessSubThread(BrowserProcessSubThread::IO));
    if (!ioThread.get()) {
      return;
    }

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    if (!ioThread->StartWithOptions(options)) {
      return;
    }
    sIOThread = ioThread.release();
  }

  // Establish the main thread here.
  if (NS_FAILED(nsThreadManager::get()->Init())) {
    NS_ERROR("Could not initialize thread manager");
    return;
  }
  NS_SetMainThread();
  mAppParent = new EmbedLiteAppProcessParent();

  // set gGREBinPath
  gGREBinPath = ToNewUnicode(nsDependentCString(getenv("GRE_HOME")));

  if (!CommandLine::IsInitialized()) {
    CommandLine::Init(0, nullptr);
  }

  LaunchAndWaitForProcessHandle();
  mAppParent->Open(GetChannel(), GetOwnedChildProcessHandle());
}

} // namespace embedlite
} // namespace mozilla
