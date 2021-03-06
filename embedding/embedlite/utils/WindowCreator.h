/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WindowCreator_h_
#define __WindowCreator_h_

#include "nsIWindowCreator.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteAppChildIface;
}}
class WindowCreator : public nsIWindowCreator
{
public:
  WindowCreator(mozilla::embedlite::EmbedLiteAppChildIface* aChild);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWCREATOR

protected:
  virtual ~WindowCreator();

private:
  mozilla::embedlite::EmbedLiteAppChildIface* mChild;
};

#endif

