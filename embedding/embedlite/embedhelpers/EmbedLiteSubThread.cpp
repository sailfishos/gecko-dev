/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteSubThread.h"
#include "GeckoLoader.h"
#include "EmbedLiteApp.h"

namespace mozilla {
namespace embedlite {

namespace {

// EmbedLiteSubThread runs Gecko main-thread work for the in-process embedder.
// The XPCOM default thread stack is only 256 KiB in optimized builds, which is
// too small for layout on table-heavy email HTML. Match Gecko's worker stack
// sizing while staying below the 2 MiB huge-page boundary used on Linux.
constexpr size_t kEmbedLiteThreadStackSize = 2 * 1024 * 1024 - 2 * 4096;

}  // namespace

//
// EmbedLiteSubThread
//

EmbedLiteSubThread::EmbedLiteSubThread(EmbedLiteApp* aApp)
  : base::Thread("EmbedLiteSubThread")
  , mApp(aApp)
{
  LOGT();
}

EmbedLiteSubThread::~EmbedLiteSubThread()
{
  LOGT();
}

void EmbedLiteSubThread::Init()
{
  LOGT();
  if (!mApp->StartChildThread()) {
    LOGE("failed to start EmbedLite child thread");
  }
}

void EmbedLiteSubThread::CleanUp()
{
  LOGT();
  mApp->StopChildThread();
}

bool EmbedLiteSubThread::StartEmbedThread()
{
  LOGT();
  // StartChildThread() initializes XPCOM before TYPE_MOZILLA_CHILD enters
  // XRE_RunAppShell(), but it can block on the UI loop while wiring in-process
  // IPDL. Let StartWithOptions() return once the message loop exists so the UI
  // thread can keep pumping while Init() completes on this thread.
  Thread::Options options(MessageLoop::TYPE_MOZILLA_CHILD,
                          kEmbedLiteThreadStackSize);
  options.wait_for_init = false;
  if (!StartWithOptions(options)) {
    LOGE("failed to start EmbedLite child thread");
    return false;
  }

  return true;
}

} // namespace embedlite
} // namespace mozilla
