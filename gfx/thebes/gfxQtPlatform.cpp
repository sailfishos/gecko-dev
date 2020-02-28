/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QPixmap>
#include <QWindow>

#include <QGuiApplication>
#include <QScreen>

#include "gfxQtPlatform.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"

#include "gfxImageSurface.h"
#include "nsUnicodeProperties.h"

#include "gfxContext.h"
#include "gfxUserFontSet.h"

#include "nsUnicharUtils.h"

#include "nsMathUtils.h"
#include "nsTArray.h"

#include "qcms.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::unicode;
using namespace mozilla::gfx;

static gfxImageFormat sOffscreenFormat = SurfaceFormat::X8R8G8B8_UINT32;

gfxQtPlatform::gfxQtPlatform()
{
    int32_t depth = GetScreenDepth();
    if (depth == 16) {
        sOffscreenFormat = SurfaceFormat::R5G6B5_UINT16;
    }
    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO) | BackendTypeBit(BackendType::SKIA);
    uint32_t contentMask = BackendTypeBit(BackendType::CAIRO) | BackendTypeBit(BackendType::SKIA);
    InitBackendPrefs(canvasMask, BackendType::CAIRO,
                     contentMask, BackendType::CAIRO);
}

gfxQtPlatform::~gfxQtPlatform()
{
    gfxPangoFontGroup::Shutdown();
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
gfxQtPlatform::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName,
                                                                   aFamilyName);
    return NS_OK;
}

nsresult
gfxQtPlatform::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName,
                                                                   aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxQtPlatform::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                               const gfxFontStyle *aStyle,
                               gfxTextPerfMetrics* aTextPerf,
                               gfxUserFontSet* aUserFontSet,
                               gfxFloat aDevToCssSize)
{
    return new gfxPangoFontGroup(aFontFamilyList, aStyle,
                                 aUserFontSet, aDevToCssSize);
}

gfxFontEntry*
gfxQtPlatform::LookupLocalFont(const nsAString& aFontName,
                               uint16_t aWeight,
                               int16_t aStretch,
                               uint8_t aStyle)
{
    return gfxPangoFontGroup::NewFontEntry(aFontName, aWeight,
                                           aStretch, aStyle);
}

gfxFontEntry*
gfxQtPlatform::MakePlatformFont(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                uint8_t aStyle,
                                const uint8_t* aFontData,
                                uint32_t aLength)
{
    // passing ownership of the font data to the new font entry
    return gfxPangoFontGroup::NewFontEntry(aFontName, aWeight,
                                           aStretch, aStyle,
                                           aFontData, aLength);
}

void
gfxQtPlatform::GetPlatformCMSOutputProfile(void *&mem, size_t &size)
{
    mem = nullptr;
    size = 0;
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
