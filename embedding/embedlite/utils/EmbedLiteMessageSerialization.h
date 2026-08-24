/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteMessageSerialization_h
#define EmbedLiteMessageSerialization_h

#include "js/RootingAPI.h"
#include "js/Value.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::embedlite {

bool ConvertMessageToJSON(JSContext* aCx, JS::Handle<JS::Value> aValue,
                          bool aHasTransferables, nsAString& aJSON);

void AppendJSONReplies(
    JSContext* aCx, const nsTArray<nsString>& aJSONReplies,
    nsTArray<NotNull<RefPtr<dom::ipc::StructuredCloneData>>>& aReplies);

}  // namespace mozilla::embedlite

#endif
