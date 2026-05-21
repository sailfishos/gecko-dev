/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QPageLayout>
#include <QRectF>
#include <QDebug>
#include "nsPrintSettingsQt.h"
#include "nsIFile.h"
#include "nsCRTGlue.h"

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsQt,
                            nsPrintSettings,
                            nsPrintSettingsQt)

nsPrintSettingsQt::nsPrintSettingsQt():
    mPageLayout(new QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF()))
{
}

nsPrintSettingsQt::~nsPrintSettingsQt()
{
    //smart pointer should take care of cleanup
}

nsPrintSettingsQt::nsPrintSettingsQt(const nsPrintSettingsQt& aPS):
    mPageLayout(aPS.mPageLayout),
    mFilename(aPS.mFilename),
    mPrinterName(aPS.mPrinterName),
    mNumCopies(aPS.mNumCopies),
    mPrintInColor(aPS.mPrintInColor),
    mPrintReversed(aPS.mPrintReversed),
    mPageRanges(aPS.mPageRanges.Clone()),
    mResolution(aPS.mResolution),
    mDuplex(aPS.mDuplex),
    mOutputFormat(aPS.mOutputFormat)
{
}

nsPrintSettingsQt&
nsPrintSettingsQt::operator=(const nsPrintSettingsQt& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    nsPrintSettings::operator=(rhs);
    mPageLayout = rhs.mPageLayout;
    mFilename = rhs.mFilename;
    mPrinterName = rhs.mPrinterName;
    mNumCopies = rhs.mNumCopies;
    mPrintInColor = rhs.mPrintInColor;
    mPrintReversed = rhs.mPrintReversed;
    mPageRanges = rhs.mPageRanges.Clone();
    mResolution = rhs.mResolution;
    mDuplex = rhs.mDuplex;
    mOutputFormat = rhs.mOutputFormat;

    return *this;
}

nsresult
nsPrintSettingsQt::_Clone(nsIPrintSettings** _retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsPrintSettingsQt* newSettings = new nsPrintSettingsQt(*this);
    *_retval = newSettings;
    NS_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::_Assign(nsIPrintSettings* aPS)
{
    nsPrintSettingsQt* printSettingsQt = static_cast<nsPrintSettingsQt*>(aPS);
    if (!printSettingsQt)
        return NS_ERROR_UNEXPECTED;
    *this = *printSettingsQt;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPageRanges(const nsTArray<int32_t>& aRanges)
{
    if (aRanges.Length() % 2 != 0) {
        return NS_ERROR_FAILURE;
    }
    mPageRanges = aRanges.Clone();
    return NS_OK;
}


NS_IMETHODIMP
nsPrintSettingsQt::GetPageRanges(nsTArray<int32_t>& aRanges)
{
    aRanges = mPageRanges.Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintReversed(bool* aPrintReversed)
{
    NS_ENSURE_ARG_POINTER(aPrintReversed);
    *aPrintReversed = mPrintReversed;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrintReversed(bool aPrintReversed)
{
    mPrintReversed = aPrintReversed;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintInColor(bool* aPrintInColor)
{
    NS_ENSURE_ARG_POINTER(aPrintInColor);
    *aPrintInColor = mPrintInColor;
    return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsQt::SetPrintInColor(bool aPrintInColor)
{
    mPrintInColor = aPrintInColor;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetOrientation(int32_t* aOrientation)
{
    NS_ENSURE_ARG_POINTER(aOrientation);
    *aOrientation = (mPageLayout->orientation() == QPageLayout::Landscape) ?
                kLandscapeOrientation :
                kPortraitOrientation;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetOrientation(int32_t aOrientation)
{
    if (aOrientation == kLandscapeOrientation) {
        mPageLayout->setOrientation(QPageLayout::Landscape);
    } else {
        mPageLayout->setOrientation(QPageLayout::Portrait);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetToFileName(nsAString &aToFileName)
{
    aToFileName = nsDependentString((char16_t*)mFilename.data());
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetToFileName(const nsAString &aToFileName)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(aToFileName, true,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertUTF16toUTF8 fileNameCStr(aToFileName);
    mFilename = QString(fileNameCStr.get());

    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrinterName(nsAString &aPrinter)
{
    aPrinter = nsDependentString((const char16_t*)mPrinterName.constData());
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrinterName(const nsAString &aPrinter)
{
    NS_ConvertUTF16toUTF8 printer(aPrinter);
    mPrinterName = QString(printer.get());
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetNumCopies(int32_t* aNumCopies)
{
    NS_ENSURE_ARG_POINTER(aNumCopies);
    *aNumCopies = mNumCopies;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetNumCopies(int32_t aNumCopies)
{
    mNumCopies = aNumCopies;
    if (mNumCopies < 1) {
        qWarning() << "nsPrintSettingsQt::SetNumCopies: 'NumCopies' must be greater than 0";
        mNumCopies = 1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetScaling(double* aScaling)
{
    return nsPrintSettings::GetScaling(aScaling);
}

NS_IMETHODIMP
nsPrintSettingsQt::SetScaling(double aScaling)
{
    return nsPrintSettings::SetScaling(aScaling);
}

static const char* const indexToPaperName[] =
{ "a4", "b5", "letter", "legal", "executive",
  "a0", "a1", "a2", "a3", "a5", "a6", "a7", "a8", "a9",
  "b0", "b1", "b10", "b2", "b3", "b4", "b6", "b7", "b8", "b9",
  "c5e", "comm10e", "dle", "folio", "ledger", "tabloid"
};

static const QPageSize::PageSizeId indexToQtPaperEnum[] =
{
    QPageSize::A4, QPageSize::B5, QPageSize::Letter, QPageSize::Legal,
    QPageSize::Executive, QPageSize::A0, QPageSize::A1, QPageSize::A2, QPageSize::A3,
    QPageSize::A5, QPageSize::A6, QPageSize::A7, QPageSize::A8, QPageSize::A9,
    QPageSize::B0, QPageSize::B1, QPageSize::B10, QPageSize::B2, QPageSize::B3,
    QPageSize::B4, QPageSize::B6, QPageSize::B7, QPageSize::B8, QPageSize::B9,
    QPageSize::C5E, QPageSize::Comm10E, QPageSize::DLE, QPageSize::Folio,
    QPageSize::Ledger, QPageSize::Tabloid
};

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperId(nsAString &aPaperName)
{
    QPageSize::PageSizeId size = mPageLayout->pageSize().id();
    QString name(indexToPaperName[size]);
    aPaperName = nsDependentString((const char16_t*)name.constData());
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperId(const nsAString &aPaperName)
{
    NS_ConvertUTF16toUTF8 paperName(aPaperName);
    QString ref(paperName.get());
    ref = ref.toLower();
    for (uint32_t i = 0; i < sizeof(indexToPaperName)/sizeof(char*); i++)
    {
        if (ref == QString(indexToPaperName[i])) {
            mPageLayout->setPageSize(QPageSize(indexToQtPaperEnum[i]));
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

QPageLayout::Unit GetQtUnit(int16_t aGeckoUnit)
{
    if (aGeckoUnit == nsIPrintSettings::kPaperSizeMillimeters) {
        return QPageLayout::Millimeter;
    } else {
        return QPageLayout::Inch;
    }
}

#define SETUNWRITEABLEMARGIN\
    mPageLayout->setUnits(QPageLayout::Inch);\
    mPageLayout->setMargins(QMarginsF(\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.left),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.top),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.right),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom)));

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin)
{
    nsPrintSettings::SetUnwriteableMarginInTwips(aUnwriteableMargin);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginTop(double aUnwriteableMarginTop)
{
    nsPrintSettings::SetUnwriteableMarginTop(aUnwriteableMarginTop);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft)
{
    nsPrintSettings::SetUnwriteableMarginLeft(aUnwriteableMarginLeft);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom)
{
    nsPrintSettings::SetUnwriteableMarginBottom(aUnwriteableMarginBottom);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginRight(double aUnwriteableMarginRight)
{
    nsPrintSettings::SetUnwriteableMarginRight(aUnwriteableMarginRight);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperWidth(double* aPaperWidth)
{
    NS_ENSURE_ARG_POINTER(aPaperWidth);
    *aPaperWidth = mPageLayout->fullRect(GetQtUnit(mPaperSizeUnit)).width();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperWidth(double aPaperWidth)
{
    QSizeF papersize = mPageLayout->fullRect(GetQtUnit(mPaperSizeUnit)).size();
    papersize.setWidth(aPaperWidth);
    mPageLayout->setPageSize(QPageSize(papersize, (QPageSize::Unit)GetQtUnit(mPaperSizeUnit)));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperHeight(double* aPaperHeight)
{
    NS_ENSURE_ARG_POINTER(aPaperHeight);
    *aPaperHeight = mPageLayout->fullRect(GetQtUnit(mPaperSizeUnit)).height();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperHeight(double aPaperHeight)
{
    QSizeF papersize = mPageLayout->fullRect(GetQtUnit(mPaperSizeUnit)).size();
    papersize.setHeight(aPaperHeight);
    mPageLayout->setPageSize(QPageSize(papersize, (QPageSize::Unit)GetQtUnit(mPaperSizeUnit)));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperSizeUnit(int16_t aPaperSizeUnit)
{
    mPaperSizeUnit = aPaperSizeUnit;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetEffectivePageSize(double* aWidth, double* aHeight)
{
    QSizeF papersize = mPageLayout->fullRect(QPageLayout::Inch).size();
    if (mPageLayout->orientation() == QPageLayout::Landscape) {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.height());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.width());
    } else {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.width());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.height());
    }
    return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsQt::GetResolution(int32_t* aResolution) {
    NS_ENSURE_ARG_POINTER(aResolution);
    *aResolution = mResolution;
    return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsQt::SetResolution(const int32_t aResolution) {
    mResolution = aResolution;
    return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsQt::GetDuplex(int32_t* aDuplex) {
    NS_ENSURE_ARG_POINTER(aDuplex);
    *aDuplex = mDuplex;
    return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsQt::SetDuplex(const int32_t aDuplex) {
    mDuplex = aDuplex;
    return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsQt::GetOutputFormat(int16_t* aOutputFormat) {
    NS_ENSURE_ARG_POINTER(aOutputFormat);
    *aOutputFormat = mOutputFormat;
    return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsQt::SetOutputFormat(int16_t aOutputFormat) {
    mOutputFormat = aOutputFormat;
    return NS_OK;
}
