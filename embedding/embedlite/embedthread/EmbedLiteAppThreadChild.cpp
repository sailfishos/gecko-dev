/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteWindowThreadChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/layers/PCompositorBridgeChild.h"

namespace mozilla {
namespace embedlite {

static EmbedLiteAppThreadChild* sAppThreadChild = nullptr;

EmbedLiteAppThreadChild*
EmbedLiteAppThreadChild::GetInstance()
{
  return sAppThreadChild;
}

EmbedLiteAppThreadChild::EmbedLiteAppThreadChild(MessageLoop* aParentLoop)
  : EmbedLiteAppChild(aParentLoop)
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
EmbedLiteAppThreadChild::AllocPEmbedLiteViewChild(const uint32_t &windowId,
                                                  const uint32_t &id,
                                                  const uint32_t &parentId,
                                                  const uintptr_t &parentBrowsingContext,
                                                  const bool &isPrivateWindow,
                                                  const bool &isDesktopMode,
                                                  const bool &isHidden)
{
  LOGT("id:%u, parentId:%u", id, parentId);

  mozilla::dom::BrowsingContext *parentBrowsingContextPtr = nullptr;
  if (parentBrowsingContext) {
    parentBrowsingContextPtr = reinterpret_cast<mozilla::dom::BrowsingContext*>(parentBrowsingContext);
  }

  EmbedLiteViewThreadChild* view = new EmbedLiteViewThreadChild(windowId, id, parentId,
                                                                parentBrowsingContextPtr,
                                                                isPrivateWindow, isDesktopMode,
                                                                isHidden);
  mWeakViewMap[id] = view;
  view->AddRef();
  return view;
}

PEmbedLiteWindowChild*
EmbedLiteAppThreadChild::AllocPEmbedLiteWindowChild(const uint16_t &width, const uint16_t &height, const uint32_t &id, const uintptr_t &aListener)
{
  LOGT("id:%u", id);
  EmbedLiteWindowThreadChild *window = new EmbedLiteWindowThreadChild(width, height, id, reinterpret_cast<EmbedLiteWindowListener*>(aListener));
  mWeakWindowMap[id] = window;
  window->AddRef();
  return window;
}

mozilla::layers::PCompositorBridgeChild*
EmbedLiteAppThreadChild::AllocPCompositorBridgeChild(Transport* aTransport, ProcessId aOtherProcess)
{
  LOGNI();
  return 0;
}

} // namespace embedlite
} // namespace mozilla

