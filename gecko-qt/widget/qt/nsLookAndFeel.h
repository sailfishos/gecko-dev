/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"

class nsLookAndFeel : public nsXPLookAndFeel
{
    class Observer;
public:
    nsLookAndFeel();
    virtual ~nsLookAndFeel();

    void NativeInit() final;

    virtual nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
    virtual nsresult NativeGetFloat(FloatID aID, float& aResult) override;
    virtual nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) override;
    virtual bool NativeGetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle) override;

    virtual bool GetEchoPasswordImpl() override;
    virtual uint32_t GetPasswordMaskDelayImpl() override;
    virtual char16_t GetPasswordCharacterImpl() override;

private:
    RefPtr<Observer> mObserver;
};

#endif
