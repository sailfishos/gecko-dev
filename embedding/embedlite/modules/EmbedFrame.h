/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBEDFRAME_H
#define EMBEDFRAME_H

#include "nsCOMPtr.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/BrowsingContext.h"

namespace mozilla {
namespace dom {

class EmbedFrame : public mozilla::DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(EmbedFrame,
                                           mozilla::DOMEventTargetHelper)

  EmbedFrame();

  Nullable<WindowProxyHolder> GetContentWindow();
  already_AddRefed<mozilla::dom::ContentFrameMessageManager> MessageManager();

  RefPtr<mozilla::dom::BrowsingContext> mWindow;
  RefPtr<mozilla::dom::ContentFrameMessageManager> mMessageManager;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;


private:
  virtual ~EmbedFrame();
};

}
}

#endif
