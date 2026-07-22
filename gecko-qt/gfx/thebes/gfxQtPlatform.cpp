/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QPixmap>
#include <QWindow>

#include <QGuiApplication>
#include <QObject>
#include <QScreen>

#include <cmath>

#include "gfxQtPlatform.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"

#include "gfxImageSurface.h"
#include "nsUnicodeProperties.h"
#include "gfxFcPlatformFontList.h"

#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include <gfxTextRun.h>

#include "nsUnicharUtils.h"

#include "nsMathUtils.h"
#include "nsTArray.h"

#include "qcms.h"

#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"
#include "SoftwareVsyncSource.h"

using namespace mozilla;
using namespace mozilla::unicode;
using namespace mozilla::gfx;

static gfxImageFormat sOffscreenFormat = SurfaceFormat::X8R8G8B8_UINT32;
static FT_Library gPlatformFTLibrary = nullptr;

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS \
  "gfx.font_rendering.fontconfig.max_generic_substitutions"

static bool IsValidRefreshRate(qreal aRefreshRate)
{
    if (aRefreshRate <= 0.0 || !std::isfinite(aRefreshRate)) {
        return false;
    }

    const qreal interval = 1000.0 / aRefreshRate;
    return interval > 0.0 && std::isfinite(interval);
}

static TimeDuration VsyncRateForRefreshRate(qreal aRefreshRate)
{
    if (!IsValidRefreshRate(aRefreshRate)) {
        aRefreshRate = gfxPlatform::GetDefaultFrameRate();
    }

    return TimeDuration::FromMilliseconds(1000.0 / aRefreshRate);
}

static void SetScreenVsyncRate(SoftwareVsyncSource* aVsyncSource,
                               qreal aRefreshRate)
{
    aVsyncSource->SetVsyncRate(VsyncRateForRefreshRate(aRefreshRate));
}

class QtSoftwareVsyncSource final : public SoftwareVsyncSource
{
public:
    explicit QtSoftwareVsyncSource(const TimeDuration& aInitialVsyncRate)
        : SoftwareVsyncSource(aInitialVsyncRate)
        , mShutdown(false)
    {
    }

    void ObserveApplication(QGuiApplication* aApplication)
    {
        RefPtr<QtSoftwareVsyncSource> self = this;
        mPrimaryScreenConnection = QObject::connect(
            aApplication, &QGuiApplication::primaryScreenChanged, aApplication,
            [self](QScreen* screen) {
                self->ObserveScreen(screen);
            });

        ObserveScreen(aApplication->primaryScreen());
    }

    void ObserveScreen(QScreen* aScreen)
    {
        if (mShutdown) {
            return;
        }

        QObject::disconnect(mRefreshRateConnection);
        mRefreshRateConnection = QMetaObject::Connection();
        SetScreenVsyncRate(this, aScreen ? aScreen->refreshRate() : 0.0);

        if (!aScreen) {
            return;
        }

        RefPtr<QtSoftwareVsyncSource> self = this;
        mRefreshRateConnection =
            QObject::connect(aScreen, &QScreen::refreshRateChanged, qApp,
                             [self](qreal refreshRate) {
                if (!self->mShutdown) {
                    SetScreenVsyncRate(self, refreshRate);
                }
            });
    }

    void Shutdown() override
    {
        mShutdown = true;
        QObject::disconnect(mRefreshRateConnection);
        mRefreshRateConnection = QMetaObject::Connection();
        QObject::disconnect(mPrimaryScreenConnection);
        mPrimaryScreenConnection = QMetaObject::Connection();
        SoftwareVsyncSource::Shutdown();
    }

private:
    Atomic<bool, Relaxed> mShutdown;
    QMetaObject::Connection mPrimaryScreenConnection;
    QMetaObject::Connection mRefreshRateConnection;
};

gfxQtPlatform::gfxQtPlatform()
{
    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;

    int32_t depth = GetScreenDepth();
    if (depth == 16) {
        sOffscreenFormat = SurfaceFormat::R5G6B5_UINT16;
    }
    InitBackendPrefs(GetBackendPrefs());

    gPlatformFTLibrary = Factory::NewFTLibrary();
    MOZ_ASSERT(gPlatformFTLibrary);
    Factory::SetFTLibrary(gPlatformFTLibrary);
}

gfxQtPlatform::~gfxQtPlatform()
{
    Factory::ReleaseFTLibrary(gPlatformFTLibrary);
    gPlatformFTLibrary = nullptr;
}

bool
gfxQtPlatform::CheckVariationFontSupport()
{
  // Although there was some variation/multiple-master support in FreeType
  // in older versions, it seems too incomplete/unstable for us to use
  // until at least 2.7.1.
  FT_Int major, minor, patch;
  FT_Library_Version(Factory::GetFTLibrary(), &major, &minor, &patch);
  return major * 1000000 + minor * 1000 + patch >= 2007001;
}

already_AddRefed<gfxASurface>
gfxQtPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                      gfxImageFormat aFormat)
{
    RefPtr<gfxASurface> newSurface =
        new gfxImageSurface(aSize, aFormat);

    return newSurface.forget();
}

nsresult
gfxQtPlatform::GetFontList(nsAtom *aLangGroup,
                           const nsACString& aGenericFamily,
                           nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(
        aLangGroup, aGenericFamily, aListOfFonts);
    return NS_OK;
}

nsresult
gfxQtPlatform::UpdateFontList(bool aFullRebuild)
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList(aFullRebuild);
    return NS_OK;
}

bool
gfxQtPlatform::CreatePlatformFontList()
{
    return gfxPlatformFontList::Initialize(new gfxFcPlatformFontList);
}

int32_t
gfxQtPlatform::GetDPI()
{
    return qApp->primaryScreen()->logicalDotsPerInch();
}

gfxImageFormat
gfxQtPlatform::GetOffscreenFormat()
{
    return sOffscreenFormat;
}

void gfxQtPlatform::FontsPrefsChanged(const char *aPref)
{
    // only checking for generic substitions, pass other changes up
    if (strcmp(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, aPref)) {
      gfxPlatform::FontsPrefsChanged(aPref);
      return;
    }

    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;
    gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
    pfl->ClearGenericMappings();
    FlushFontAndWordCaches();
}

uint32_t gfxQtPlatform::MaxGenericSubstitions()
{
    if (mMaxGenericSubstitutions == UNINITIALIZED_VALUE) {
      mMaxGenericSubstitutions =
          Preferences::GetInt(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, 3);
      if (mMaxGenericSubstitutions < 0) {
        mMaxGenericSubstitutions = 3;
      }
    }

    return uint32_t(mMaxGenericSubstitutions);
}

already_AddRefed<gfx::VsyncSource>
gfxQtPlatform::CreateGlobalHardwareVsyncSource()
{
    RefPtr<QtSoftwareVsyncSource> softwareVsyncSource =
        new QtSoftwareVsyncSource(VsyncRateForRefreshRate(0.0));

    if (qApp) {
        softwareVsyncSource->ObserveApplication(qApp);
    }

    RefPtr<VsyncSource> vsyncSource = softwareVsyncSource;
    return vsyncSource.forget();
}
