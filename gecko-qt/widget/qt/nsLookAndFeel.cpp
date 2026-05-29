/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <QGuiApplication>
#include <QFont>
#include <QScreen>
#include <QPalette>

#include "nsLookAndFeel.h"
#include "nsStyleConsts.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/Services.h"
#include "mozilla/Logging.h"
#include "nsIObserver.h"

using namespace mozilla;

LazyLogModule sLookAndFeel("LookAndFeel");

class nsLookAndFeel::Observer final : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS

    explicit Observer() : mDarkAmbience(false) {}

    NS_IMETHOD Observe(nsISupports*, const char* aTopic,
                       const char16_t* aData) override;

    bool GetDarkAmbience();
private:
    virtual ~Observer() = default;

public:
    bool mDarkAmbience;
};

NS_IMPL_ISUPPORTS(nsLookAndFeel::Observer, nsIObserver)

static const char16_t UNICODE_BULLET = 0x2022;

#define QCOLOR_TO_NS_RGB(c)                     \
  ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel()
{
    mObserver = new nsLookAndFeel::Observer();
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
        os->AddObserver(mObserver, "ambience-theme-changed", false);
    }
}

nsLookAndFeel::~nsLookAndFeel()
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
        os->RemoveObserver(mObserver, "ambience-theme-changed");
    }
    mObserver = nullptr;
}

NS_IMETHODIMP nsLookAndFeel::Observer::Observe(nsISupports*, const char* aTopic, const char16_t* aData) {
    MOZ_ASSERT(!strcmp(aTopic, "ambience-theme-changed"));

    bool darkAmbience = false;
    nsDependentString data(aData);
    if (data.EqualsLiteral("dark")) {
        darkAmbience = true;
    }

    if (mDarkAmbience != darkAmbience) {
        mDarkAmbience = darkAmbience;
        MOZ_LOG(sLookAndFeel, LogLevel::Info, ("Ambience set to %s", mDarkAmbience ? "dark" : "light"));
        Refresh();
        if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
            NotifyChangedAllWindows(widget::ThemeChangeKind::StyleAndLayout);
        }
    }

    return NS_OK;
}

bool nsLookAndFeel::Observer::GetDarkAmbience()
{
    return mDarkAmbience;
}


void nsLookAndFeel::NativeInit() { }

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme, nscolor &aResult)
{
    nsresult rv = NS_OK;

#define BG_PRELIGHT_COLOR     NS_RGB(0xee,0xee,0xee)
#define FG_PRELIGHT_COLOR     NS_RGB(0x77,0x77,0x77)
#define RED_COLOR             NS_RGB(0xff,0x00,0x00)

    QPalette palette = QGuiApplication::palette();

    switch (aID) {
    case ColorID::TextHighlightBackground:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
        // still used
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case ColorID::TextHighlightForeground:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
        // still used
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
        break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
        aResult = NS_TRANSPARENT;
        break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
        aResult = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
        aResult = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
        aResult = NS_TRANSPARENT;
        break;
    case ColorID::SpellCheckerUnderline:
        aResult = RED_COLOR;
        break;

        // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:
        // active window border
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::Activecaption:
        // active window caption background
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::Appworkspace:
        // MDI background color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::Background:
        // desktop background
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::Captiontext:
        // text in active window caption, size box, and scrollbar arrow box (!)
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::Graytext:
        // disabled text in windows, menus, etc.
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
        break;
    case ColorID::Highlight:
        // background of selected item
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case ColorID::Highlighttext:
        // text of selected item
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
        break;
    case ColorID::Inactiveborder:
        // inactive window border
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
        break;
    case ColorID::Inactivecaption:
        // inactive window caption
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
        break;
    case ColorID::Inactivecaptiontext:
        // text in inactive window caption
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
        break;
    case ColorID::Infobackground:
        // tooltip background color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipBase));
        break;
    case ColorID::Infotext:
        // tooltip text color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipText));
        break;
    case ColorID::Menu:
        // menu background
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::Menutext:
        // menu text
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::Scrollbar:
        // scrollbar gray area
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Mid));
        break;

    case ColorID::Threedface:
    case ColorID::Buttonface:
        // 3-D face color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
        break;

    case ColorID::Buttontext:
        // text on push buttons
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ButtonText));
        break;

    case ColorID::Buttonhighlight:
        // 3-D highlighted edge color
    case ColorID::Threedhighlight:
        // 3-D highlighted outer edge color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
        break;

    case ColorID::Threedlightshadow:
        // 3-D highlighted inner edge color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Light));
        break;

    case ColorID::Buttonshadow:
        // 3-D shadow edge color
    case ColorID::Threedshadow:
        // 3-D shadow inner edge color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
        break;

    case ColorID::Threeddarkshadow:
        // 3-D shadow outer edge color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Shadow));
        break;

    case ColorID::Window:
    case ColorID::Windowframe:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;

    case ColorID::Windowtext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;

    case ColorID::MozEventreerow:
    case ColorID::Field:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
        break;
    case ColorID::Fieldtext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::MozDialog:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::MozDialogtext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
        break;
    case ColorID::MozDragtargetzone:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
        break;
    case ColorID::MozButtondefault:
        // default button border color
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
        break;
    case ColorID::MozButtonhoverface:
        aResult = BG_PRELIGHT_COLOR;
        break;
    case ColorID::MozButtonhovertext:
        aResult = FG_PRELIGHT_COLOR;
        break;
    case ColorID::MozCellhighlight:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
        break;
    case ColorID::MozCellhighlighttext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
        break;
    case ColorID::MozMenuhover:
        aResult = BG_PRELIGHT_COLOR;
        break;
    case ColorID::MozMenuhovertext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::MozOddtreerow:
        aResult = NS_TRANSPARENT;
        break;
    case ColorID::MozNativehyperlinktext:
        aResult = NS_SAME_AS_FOREGROUND_COLOR;
        break;
    case ColorID::MozComboboxtext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::MozCombobox:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
        break;
    case ColorID::MozMenubartext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    case ColorID::MozMenubarhovertext:
        aResult = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
        break;
    default:
        /* default color is BLACK */
        aResult = 0;
        rv = NS_ERROR_FAILURE;
        break;
    }

    return rv;
}

nsresult
nsLookAndFeel::NativeGetInt(IntID aID, int32_t &aResult)
{
    nsresult rv = NS_OK;

    switch (aID) {
        case IntID::ScrollButtonLeftMouseButtonAction:
          aResult = 0;
          break;

        case IntID::ScrollButtonMiddleMouseButtonAction:
        case IntID::ScrollButtonRightMouseButtonAction:
          aResult = 3;
          break;

        case IntID::CaretBlinkTime:
            aResult = 500;
            break;

        case IntID::CaretWidth:
            aResult = 1;
            break;

        case IntID::ShowCaretDuringSelection:
            aResult = 0;
            break;

        case IntID::SelectTextfieldsOnKeyFocus:
            // Select textfield content when focused by kbd
            // used by EventStateManager::sTextfieldSelectModel
            aResult = 1;
            break;

        case IntID::SubmenuDelay:
            aResult = 200;
            break;

        case IntID::TooltipDelay:
            aResult = 500;
            break;

        case IntID::MenusCanOverlapOSBar:
            // we want XUL popups to be able to overlap the task bar.
            aResult = 1;
            break;

        case IntID::ScrollArrowStyle:
            aResult = eScrollArrowStyle_Single;
            break;

        case IntID::WindowsDefaultTheme:
            aResult = 0;
            rv = NS_ERROR_NOT_IMPLEMENTED;
            break;

        case IntID::IMERawInputUnderlineStyle:
        case IntID::IMEConvertedTextUnderlineStyle:
            aResult = static_cast<int32_t>(StyleTextDecorationStyle::Solid);
            break;

        case IntID::IMESelectedRawTextUnderlineStyle:
        case IntID::IMESelectedConvertedTextUnderline:
            aResult = static_cast<int32_t>(StyleTextDecorationStyle::None);
            break;

        case IntID::SpellCheckerUnderlineStyle:
            aResult = static_cast<int32_t>(StyleTextDecorationStyle::Wavy);
            break;

        case IntID::ScrollbarButtonAutoRepeatBehavior:
            aResult = 0;
            break;

        case IntID::ContextMenuOffsetVertical:
        case IntID::ContextMenuOffsetHorizontal:
            aResult = 2;
            break;

        case IntID::SystemUsesDarkTheme:
            // Choose theme based on ambience
            aResult = mObserver->GetDarkAmbience() ? 1 : 0;
            break;

        case IntID::DragThresholdX:
        case IntID::DragThresholdY:
          // Threshold where a tap becomes a drag, in 1/240" reference pixels.
          aResult = 25;
          break;

        default:
            aResult = 0;
            rv = NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult
nsLookAndFeel::NativeGetFloat(FloatID aID, float &aResult)
{
  nsresult res = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
        aResult = 1.0f;
        break;

    case FloatID::SpellCheckerUnderlineRelativeSize:
        aResult = 1.0f;
        break;

    default:
        aResult = -1.0;
        res = NS_ERROR_FAILURE;
  }
  return res;
}

/*virtual*/
bool
nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle)
{
  QFont qFont = QGuiApplication::font();

  constexpr auto quote = u"\""_ns;
  nsString family((char16_t*)qFont.family().data());
  aFontName = quote + family + quote;

  aFontStyle.systemFont = true;
  switch (qFont.style()) {
      case QFont::StyleNormal:
          aFontStyle.style = mozilla::FontSlantStyle::NORMAL;
          break;
      case QFont::StyleItalic:
          aFontStyle.style = mozilla::FontSlantStyle::ITALIC;
          break;
      case QFont::StyleOblique:
          aFontStyle.style = mozilla::FontSlantStyle::OBLIQUE;
          break;
      default:
          aFontStyle.style = mozilla::FontSlantStyle::NORMAL;
          break;
  }

  aFontStyle.weight = mozilla::FontWeight::FromInt(qFont.weight());
  aFontStyle.stretch = mozilla::FontStretch::NORMAL;
  float scaleFactor = 1.0f;
  // use pixel size directly if it is set, otherwise compute from point size
  if (qFont.pixelSize() != -1) {
    aFontStyle.size = qFont.pixelSize() / scaleFactor;
  } else {
    aFontStyle.size = qFont.pointSizeF() * qApp->primaryScreen()->logicalDotsPerInch() / 72.0f / scaleFactor;
  }

  return true;
}

/*virtual*/
bool
nsLookAndFeel::GetEchoPasswordImpl() {
    return true;
}

/*virtual*/
uint32_t
nsLookAndFeel::GetPasswordMaskDelayImpl()
{
    // Same value on Android framework
    return 1500;
}

/* virtual */
char16_t
nsLookAndFeel::GetPasswordCharacterImpl()
{
    return UNICODE_BULLET;
}
