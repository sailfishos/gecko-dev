/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_APP_THREAD_H
#define EMBED_LITE_APP_THREAD_H

#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"
#include "base/message_loop.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteAppThreadParent;
class EmbedLiteAppThreadChild;
class EmbedLiteApp;

class EmbedLiteAppThread
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppThread)

  EmbedLiteAppThread(MessageLoop* aParentLoop);
  virtual ~EmbedLiteAppThread();

  void Init();
  void Destroy();

private:
  RefPtr<EmbedLiteAppThreadParent> mParentThread;
  RefPtr<EmbedLiteAppThreadChild> mChildThread;
  MessageLoop* mParentLoop;
};

} // namespace embedlite
} // namespace mozilla

#endif // EMBED_LITE_APP_THREAD_H
