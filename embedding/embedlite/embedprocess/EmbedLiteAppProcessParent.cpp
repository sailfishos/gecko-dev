/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "EmbedLiteAppProcessParent.h"
#include "EmbedLiteApp.h"
#include "mozilla/unused.h"

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
namespace embedlite {

EmbedLiteAppProcessParent::EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppProcessParent);
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);
}

bool
EmbedLiteAppProcessParent::RecvInitialized()
{
    return false;
}

bool
EmbedLiteAppProcessParent::RecvReadyToShutdown()
{
    return false;
}

bool
EmbedLiteAppProcessParent::RecvCreateWindow(
        const uint32_t& parentId,
        const nsCString& uri,
        const uint32_t& chromeFlags,
        const uint32_t& contextFlags,
        uint32_t* createdID,
        bool* cancel)
{
    return false;
}

bool
EmbedLiteAppProcessParent::RecvObserve(
        const nsCString& topic,
        const nsString& data)
{
    return false;
}

PEmbedLiteViewParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteViewParent(
        const uint32_t& id,
        const uint32_t& parentId)
{
    return 0;
}

bool
EmbedLiteAppProcessParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* aActor)
{
    return false;
}

void
EmbedLiteAppProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace embedlite
} // namespace mozilla

