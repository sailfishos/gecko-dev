/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteWindowThreadParent.h"
#include "EmbedLiteAppParent.h"
#include "EmbedLiteApp.h"
#include "mozilla/layers/PCompositorBridgeParent.h"


using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {


EmbedLiteAppParent::EmbedLiteAppParent()
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppParent);
}

EmbedLiteAppParent::~EmbedLiteAppParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppParent);
}

void EmbedLiteAppParent::PushBrowserInit(
    const EmbedLiteBrowserInitData& aBrowserInit)
{
  mPendingBrowserInits.AppendElement(Some(aBrowserInit));
}

Maybe<EmbedLiteBrowserInitData> EmbedLiteAppParent::TakeBrowserInit()
{
  if (mPendingBrowserInits.IsEmpty()) {
    return Nothing();
  }

  Maybe<EmbedLiteBrowserInitData>& pending = mPendingBrowserInits.LastElement();
  Maybe<EmbedLiteBrowserInitData> result = std::move(pending);
  pending = Nothing();
  return result;
}

void EmbedLiteAppParent::PopBrowserInit()
{
  MOZ_ASSERT(!mPendingBrowserInits.IsEmpty());
  mPendingBrowserInits.RemoveLastElement();
}


} // namespace embedlite
} // namespace mozilla
