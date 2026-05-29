/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_QT_H
#define GFX_PLATFORM_QT_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
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

    virtual nsresult UpdateFontList(bool aFullRebuild = true);

    virtual bool CreatePlatformFontList() override;

    static int32_t GetDPI();

    virtual gfxImageFormat GetOffscreenFormat() override;

    bool AccelerateLayersByDefault() override {
      return true;
    }

    void FontsPrefsChanged(const char* aPref) override;

    uint32_t MaxGenericSubstitions();

    already_AddRefed<mozilla::gfx::VsyncSource>
    CreateGlobalHardwareVsyncSource() override;

    static bool CheckVariationFontSupport();

protected:
    int8_t mMaxGenericSubstitutions;

private:
    int mScreenDepth;
};

#endif /* GFX_PLATFORM_QT_H */
