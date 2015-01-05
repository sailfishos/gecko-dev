/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"
#include "mozilla/layers/PCompositorChild.h"

namespace mozilla {
namespace embedlite {

static EmbedLiteAppThreadChild* sAppThreadChild = nullptr;

EmbedLiteAppThreadChild*
EmbedLiteAppThreadChild::GetInstance()
{
  return sAppThreadChild;
}

EmbedLiteAppThreadChild::EmbedLiteAppThreadChild(MessageLoop* aParentLoop)
  : EmbedLiteAppBaseChild(aParentLoop)
{
  LOGT();
  sAppThreadChild = this;
}

EmbedLiteAppThreadChild::~EmbedLiteAppThreadChild()
{
  LOGT();
  sAppThreadChild = nullptr;
}

PEmbedLiteViewChild*
EmbedLiteAppThreadChild::AllocPEmbedLiteViewChild(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
{
  LOGT("id:%u, parentId:%u", id, parentId);
  EmbedLiteViewThreadChild* view = new EmbedLiteViewThreadChild(id, parentId, isPrivateWindow);
  mWeakViewMap[id] = view;
  view->AddRef();
  return view;
}

mozilla::layers::PCompositorChild*
EmbedLiteAppThreadChild::AllocPCompositorChild(Transport* aTransport, ProcessId aOtherProcess)
{
  LOGNI();
  return 0;
}

} // namespace embedlite
} // namespace mozilla

