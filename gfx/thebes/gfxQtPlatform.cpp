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
#include "gfxFcPlatformFontList.h"

#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include <gfxTextRun.h>

#include "nsUnicharUtils.h"

#include "nsMathUtils.h"
#include "nsTArray.h"

#include "qcms.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::unicode;
using namespace mozilla::gfx;

static gfxImageFormat sOffscreenFormat = SurfaceFormat::X8R8G8B8_UINT32;

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS \
  "gfx.font_rendering.fontconfig.max_generic_substitutions"

gfxQtPlatform::gfxQtPlatform()
{
    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;

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
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

gfxPlatformFontList *gfxQtPlatform::CreatePlatformFontList()
{
  gfxPlatformFontList* list = new gfxFcPlatformFontList();
  if (NS_SUCCEEDED(list->InitFontList())) {
    return list;
  }
  gfxPlatformFontList::Shutdown();
  return nullptr;
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
    return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf, aUserFontSet,
                            aDevToCssSize);
}

gfxFontEntry*
gfxQtPlatform::LookupLocalFont(const nsAString& aFontName,
                               uint16_t aWeight,
                               int16_t aStretch,
                               uint8_t aStyle)
{
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    return pfl->LookupLocalFont(aFontName, aWeight, aStretch, aStyle);
}

gfxFontEntry*
gfxQtPlatform::MakePlatformFont(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                uint8_t aStyle,
                                const uint8_t* aFontData,
                                uint32_t aLength)
{
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    return pfl->MakePlatformFont(aFontName, aWeight, aStretch, aStyle, aFontData,
                                 aLength);
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
