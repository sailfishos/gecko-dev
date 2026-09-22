/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBEDLITE_CHROME_CONTENT_REGISTRATIONS_H
#define MOZ_EMBEDLITE_CHROME_CONTENT_REGISTRATIONS_H

#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::embedlite {

class EmbedLiteChromeContentRegistrations final
{
public:
  bool AddFrameScript(const nsACString& aURI)
  {
    if (mFrameScripts.Contains(aURI)) {
      return false;
    }
    mFrameScripts.AppendElement(aURI);
    return true;
  }

  bool AddMessageListener(const nsACString& aName)
  {
    if (mMessageListeners.Contains(aName)) {
      return false;
    }
    mMessageListeners.AppendElement(aName);
    return true;
  }

  bool RemoveFrameScript(const nsACString& aURI)
  {
    return mFrameScripts.RemoveElement(aURI);
  }

  bool RemoveMessageListener(const nsACString& aName)
  {
    return mMessageListeners.RemoveElement(aName);
  }

  bool HasFrameScript(const nsACString& aURI) const
  {
    return mFrameScripts.Contains(aURI);
  }

  bool HasMessageListener(const nsACString& aName) const
  {
    return mMessageListeners.Contains(aName);
  }

  const nsTArray<nsCString>& FrameScripts() const { return mFrameScripts; }
  const nsTArray<nsCString>& MessageListeners() const
  {
    return mMessageListeners;
  }

private:
  nsTArray<nsCString> mFrameScripts;
  nsTArray<nsCString> mMessageListeners;
};

} // namespace mozilla::embedlite

#endif
