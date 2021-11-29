/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedFrame.h"
#include "mozilla/dom/nsIEmbedFrameBinding.h"

namespace mozilla {
namespace dom {

EmbedFrame::EmbedFrame()
{
}

EmbedFrame::~EmbedFrame()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(EmbedFrame)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(EmbedFrame,
                                                mozilla::DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(EmbedFrame,
                                                  mozilla::DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EmbedFrame)
NS_INTERFACE_MAP_END_INHERITING(mozilla::DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(EmbedFrame, mozilla::DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(EmbedFrame, mozilla::DOMEventTargetHelper)

Nullable<WindowProxyHolder> EmbedFrame::GetContentWindow()
{
  if (mWindow) {
    return dom::WindowProxyHolder(mWindow);
  } else {
    return nullptr;
  }
}

already_AddRefed<mozilla::dom::ContentFrameMessageManager> EmbedFrame::MessageManager()
{
    RefPtr<mozilla::dom::ContentFrameMessageManager> mm = mMessageManager;
    return mm.forget();
};

JSObject* EmbedFrame::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return EmbedFrame_Binding::Wrap(aCx, this, aGivenProto);
}

}
}
