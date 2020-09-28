/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_QT_H
#define GFX_PLATFORM_QT_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"
#ifdef MOZ_X11
#include "X11/Xlib.h"
#endif

class QWindow;

class gfxQtPlatform : public gfxPlatform {
public:
    gfxQtPlatform();
    virtual ~gfxQtPlatform();

    static gfxQtPlatform *GetPlatform() {
        return static_cast<gfxQtPlatform*>(gfxPlatform::GetPlatform());
    }

    virtual already_AddRefed<gfxASurface>
      CreateOffscreenSurface(const IntSize& aSize,
                             gfxImageFormat aFormat) override;

    virtual nsresult GetFontList(nsAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts) override;

    virtual nsresult UpdateFontList() override;

    virtual gfxPlatformFontList* CreatePlatformFontList() override;

    virtual nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName) override;

    gfxFontGroup*
    CreateFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                    const gfxFontStyle *aStyle,
                    gfxTextPerfMetrics* aTextPerf,
                    gfxUserFontSet *aUserFontSet,
                    gfxFloat aDevToCssSize) override;

    /**
     * Look up a local platform font using the full font face name (needed to
     * support @font-face src local() )
     */
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          uint8_t aStyle) override;

    /**
     * Activate a platform font (needed to support @font-face src url() )
     *
     */
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           uint8_t aStyle,
                                           const uint8_t* aFontData,
                                           uint32_t aLength) override;

    FT_Library GetFTLibrary() override;

    static int32_t GetDPI();

    virtual gfxImageFormat GetOffscreenFormat() override;

    bool AccelerateLayersByDefault() override {
      return true;
    }

    void FontsPrefsChanged(const char* aPref) override;

    uint32_t MaxGenericSubstitions();

protected:
    int8_t mMaxGenericSubstitutions;

private:
    virtual void GetPlatformCMSOutputProfile(void *&mem, size_t &size) override;

    int mScreenDepth;
};

#endif /* GFX_PLATFORM_QT_H */

