/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerQt_h___
#define nsScreenManagerQt_h___

#include "mozilla/widget/ScreenManager.h"

//------------------------------------------------------------------------
namespace mozilla {
namespace widget {

class ScreenHelperQt final : public ScreenManager::Helper {
public:
  ScreenHelperQt();
  ~ScreenHelperQt() override = default;

  static void RefreshScreens();
};

}  // namespace widget
}  // namespace mozilla

#endif  // nsScreenManagerQt_h___
