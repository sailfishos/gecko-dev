/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteAppThread"
#include "EmbedLog.h"

#include "EmbedLiteAppThread.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteAppThreadParent.h"

namespace mozilla {
namespace embedlite {

EmbedLiteAppThread::EmbedLiteAppThread(MessageLoop* aParentLoop)
  : mParentLoop(aParentLoop)
{
  LOGT();
  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableMethod(this, &EmbedLiteAppThread::Init));
}

EmbedLiteAppThread::~EmbedLiteAppThread()
{
  LOGT();
}

void
EmbedLiteAppThread::Init()
{
  LOGT();
  mParentThread = new EmbedLiteAppThreadParent(mParentLoop);
  mChildThread = new EmbedLiteAppThreadChild(mParentLoop);
  mChildThread->Init(mParentThread);
}

void
EmbedLiteAppThread::Destroy()
{
  LOGT();
  mChildThread->Close();
  mParentThread = nullptr;
  mChildThread = nullptr;
}

} // namespace embedlite
} // namespace mozilla
