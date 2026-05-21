/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QGuiApplication>
#include <QScreen>
#include <utility>

#include "nsScreenManagerQt.h"

#include "mozilla/RefPtr.h"
#include "mozilla/widget/Screen.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

static LayoutDeviceIntRect ToLayoutDeviceIntRect(const QRect& aRect) {
    return LayoutDeviceIntRect(aRect.x(), aRect.y(), aRect.width(), aRect.height());
}

ScreenHelperQt::ScreenHelperQt() { RefreshScreens(); }

void ScreenHelperQt::RefreshScreens() {
    AutoTArray<RefPtr<Screen>, 4> screenList;

    for (QScreen* screen : QGuiApplication::screens()) {
        const LayoutDeviceIntRect rect = ToLayoutDeviceIntRect(screen->geometry());
        const LayoutDeviceIntRect availRect =
            ToLayoutDeviceIntRect(screen->availableGeometry());
        const uint32_t depth = screen->depth();
        const uint32_t refreshRate = screen->refreshRate() > 0.0
                                         ? uint32_t(screen->refreshRate())
                                         : 0;
        const double deviceScale = screen->devicePixelRatio() > 0.0
                                       ? screen->devicePixelRatio()
                                       : 1.0;

        screenList.AppendElement(MakeRefPtr<Screen>(
            rect, availRect, depth, depth, refreshRate,
            DesktopToLayoutDeviceScale(deviceScale),
            CSSToLayoutDeviceScale(deviceScale), screen->logicalDotsPerInch(),
            Screen::IsPseudoDisplay::No));
    }

    if (screenList.IsEmpty()) {
        LayoutDeviceIntRect rect(0, 0, 1, 1);
        screenList.AppendElement(MakeRefPtr<Screen>(
            rect, rect, 24, 24, 0, DesktopToLayoutDeviceScale(),
            CSSToLayoutDeviceScale(), 96.0f, Screen::IsPseudoDisplay::No));
    }

    ScreenManager::Refresh(std::move(screenList));
}

}  // namespace widget
}  // namespace mozilla
