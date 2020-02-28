/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This will pull in Qt Widgets and we don't want those
#if 0
#include <QPrinter>
#endif
#include <QDebug>
#include "nsPrintSettingsQt.h"
#include "nsIFile.h"
#include "nsCRTGlue.h"

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsQt,
                            nsPrintSettings,
                            nsPrintSettingsQt)

nsPrintSettingsQt::nsPrintSettingsQt()
// This will pull in Qt Widgets and we don't want those
#if 0
    : mQPrinter(new QPrinter())
#endif
{
}

nsPrintSettingsQt::~nsPrintSettingsQt()
{
    //smart pointer should take care of cleanup
}

nsPrintSettingsQt::nsPrintSettingsQt(const nsPrintSettingsQt& aPS)
// This will pull in Qt Widgets and we don't want those
#if 0
    : mQPrinter(aPS.mQPrinter)
#endif
{
}

nsPrintSettingsQt& 
nsPrintSettingsQt::operator=(const nsPrintSettingsQt& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    nsPrintSettings::operator=(rhs);
// This will pull in Qt Widgets and we don't want those
#if 0
    mQPrinter = rhs.mQPrinter;
#endif
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
nsPrintSettingsQt::GetPrintRange(int16_t* aPrintRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPrintRange);

    QPrinter::PrintRange range = mQPrinter->printRange();
    if (range == QPrinter::PageRange) {
        *aPrintRange = kRangeSpecifiedPageRange;
    } else if (range == QPrinter::Selection) {
        *aPrintRange = kRangeSelection;
    } else {
        *aPrintRange = kRangeAllPages;
    }

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP 
nsPrintSettingsQt::SetPrintRange(int16_t aPrintRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    if (aPrintRange == kRangeSelection) {
        mQPrinter->setPrintRange(QPrinter::Selection);
    } else if (aPrintRange == kRangeSpecifiedPageRange) {
        mQPrinter->setPrintRange(QPrinter::PageRange);
    } else {
        mQPrinter->setPrintRange(QPrinter::AllPages);
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetStartPageRange(int32_t* aStartPageRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aStartPageRange);
    int32_t start = mQPrinter->fromPage();
    *aStartPageRange = start;
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetStartPageRange(int32_t aStartPageRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    int32_t endRange = mQPrinter->toPage();
    mQPrinter->setFromTo(aStartPageRange, endRange);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetEndPageRange(int32_t* aEndPageRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aEndPageRange);
    int32_t end = mQPrinter->toPage();
    *aEndPageRange = end;
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetEndPageRange(int32_t aEndPageRange)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    int32_t startRange = mQPrinter->fromPage();
    mQPrinter->setFromTo(startRange, aEndPageRange);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintReversed(bool* aPrintReversed)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPrintReversed);
    if (mQPrinter->pageOrder() == QPrinter::LastPageFirst) {
        *aPrintReversed = true;
    } else {
        *aPrintReversed = false;
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrintReversed(bool aPrintReversed)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    if (aPrintReversed) {
        mQPrinter->setPageOrder(QPrinter::LastPageFirst);
    } else {
        mQPrinter->setPageOrder(QPrinter::FirstPageFirst);
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintInColor(bool* aPrintInColor)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPrintInColor);
    if (mQPrinter->colorMode() == QPrinter::Color) {
        *aPrintInColor = true;
    } else {
        *aPrintInColor = false;
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}
NS_IMETHODIMP
nsPrintSettingsQt::SetPrintInColor(bool aPrintInColor)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    if (aPrintInColor) {
        mQPrinter->setColorMode(QPrinter::Color);
    } else {
        mQPrinter->setColorMode(QPrinter::GrayScale);
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetOrientation(int32_t* aOrientation)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aOrientation);
    QPrinter::Orientation orientation = mQPrinter->orientation();
    if (orientation == QPrinter::Landscape) {
        *aOrientation = kLandscapeOrientation;
    } else {
        *aOrientation = kPortraitOrientation;
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetOrientation(int32_t aOrientation)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    if (aOrientation == kLandscapeOrientation) {
        mQPrinter->setOrientation(QPrinter::Landscape);
    } else {
        mQPrinter->setOrientation(QPrinter::Portrait);
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetToFileName(nsAString &aToFileName)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QString filename;
    filename = mQPrinter->outputFileName();
    *aToFileName = ToNewUnicode(
            nsDependentString((char16_t*)filename.data()));
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetToFileName(const nsAString &aToFileName)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_NewLocalFile(aToFileName, true,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    QString filename((const QChar*)aToFileName, NS_strlen(aToFileName));
    mQPrinter->setOutputFileName(filename);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrinterName(nsAString &aPrinter)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPrinter);
    *aPrinter = ToNewUnicode(nsDependentString(
                (const char16_t*)mQPrinter->printerName().constData()));
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrinterName(const nsAString &aPrinter)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QString printername((const QChar*)aPrinter, NS_strlen(aPrinter));
    mQPrinter->setPrinterName(printername);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetNumCopies(int32_t* aNumCopies)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aNumCopies);
    *aNumCopies = mQPrinter->numCopies();
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetNumCopies(int32_t aNumCopies)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    mQPrinter->setNumCopies(aNumCopies);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetScaling(double* aScaling)
{
    NS_ENSURE_ARG_POINTER(aScaling);
    qDebug() << Q_FUNC_INFO;
    qDebug() << "Scaling not implemented in Qt port";
    *aScaling = 1.0; //FIXME
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetScaling(double aScaling)
{
    qDebug() << Q_FUNC_INFO;
    qDebug() << "Scaling not implemented in Qt port"; //FIXME
    return NS_ERROR_NOT_IMPLEMENTED;
}

static const char* const indexToPaperName[] =
{ "A4", "B5", "Letter", "Legal", "Executive",
  "A0", "A1", "A2", "A3", "A5", "A6", "A7", "A8", "A9",
  "B0", "B1", "B10", "B2", "B3", "B4", "B6", "B7", "B8", "B9",
  "C5E", "Comm10E", "DLE", "Folio", "Ledger", "Tabloid"
};

// This will pull in Qt Widgets and we don't want those
#if 0
static const QPrinter::PageSize indexToQtPaperEnum[] =
{
    QPrinter::A4, QPrinter::B5, QPrinter::Letter, QPrinter::Legal,
    QPrinter::Executive, QPrinter::A0, QPrinter::A1, QPrinter::A2, QPrinter::A3,
    QPrinter::A5, QPrinter::A6, QPrinter::A7, QPrinter::A8, QPrinter::A9,
    QPrinter::B0, QPrinter::B1, QPrinter::B10, QPrinter::B2, QPrinter::B3,
    QPrinter::B4, QPrinter::B6, QPrinter::B7, QPrinter::B8, QPrinter::B9,
    QPrinter::C5E, QPrinter::Comm10E, QPrinter::DLE, QPrinter::Folio,
    QPrinter::Ledger, QPrinter::Tabloid
};
#endif

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperName(nsAString &aPaperName)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QPrinter::PaperSize size = mQPrinter->paperSize();
    QString name(indexToPaperName[size]);
    *aPaperName = ToNewUnicode(nsDependentString((const char16_t*)name.constData()));
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperName(const nsAString &aPaperName)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QString ref((QChar*)aPaperName, NS_strlen(aPaperName));
    for (uint32_t i = 0; i < sizeof(indexToPaperName)/sizeof(char*); i++)
    {
        if (ref == QString(indexToPaperName[i])) {
            mQPrinter->setPageSize(indexToQtPaperEnum[i]);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

// This will pull in Qt Widgets and we don't want those
#if 0
QPrinter::Unit GetQtUnit(int16_t aGeckoUnit)
{
    if (aGeckoUnit == nsIPrintSettings::kPaperSizeMillimeters) {
        return QPrinter::Millimeter;
    } else {
        return QPrinter::Inch;
    }
}
#endif

// This will pull in Qt Widgets and we don't want those
#if 0
#define SETUNWRITEABLEMARGIN\
    mQPrinter->setPageMargins(\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.left),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.top),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.right),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom),\
            QPrinter::Inch);
#else
#define SETUNWRITEABLEMARGIN
#endif

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
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPaperWidth);
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    *aPaperWidth = papersize.width();
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperWidth(double aPaperWidth)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    papersize.setWidth(aPaperWidth);
    mQPrinter->setPaperSize(papersize, GetQtUnit(mPaperSizeUnit));
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperHeight(double* aPaperHeight)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    NS_ENSURE_ARG_POINTER(aPaperHeight);
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    *aPaperHeight = papersize.height();
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperHeight(double aPaperHeight)
{
// This will pull in Qt Widgets and we don't want those
#if 0
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    papersize.setHeight(aPaperHeight);
    mQPrinter->setPaperSize(papersize, GetQtUnit(mPaperSizeUnit));
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
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
// This will pull in Qt Widgets and we don't want those
#if 0
    QSizeF papersize = mQPrinter->paperSize(QPrinter::Inch);
    if (mQPrinter->orientation() == QPrinter::Landscape) {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.height());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.width());
    } else {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.width());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.height());
    }
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

