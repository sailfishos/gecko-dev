/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsQt_h_
#define nsPrintSettingsQt_h_

#include <QSharedPointer>
#include "nsPrintSettingsImpl.h"
#define NS_PRINTSETTINGSQT_IID \
{0x5bc4c746, 0x8970, 0x43a3, {0xbf, 0xb1, 0x5d, 0xe1, 0x74, 0xaf, 0x7c, 0xea}}

class QPageLayout;
class nsPrintSettingsQt : public nsPrintSettings
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRINTSETTINGSQT_IID)

    nsPrintSettingsQt();

    NS_IMETHOD GetPrintReversed(bool* aPrintReversed) override;
    NS_IMETHOD SetPrintReversed(bool aPrintReversed) override;

    NS_IMETHOD GetPrintInColor(bool* aPrintInColor) override;
    NS_IMETHOD SetPrintInColor(bool aPrintInColor) override;

    NS_IMETHOD GetOrientation(int32_t* aOrientation) override;
    NS_IMETHOD SetOrientation(int32_t aOrientation) override;

    NS_IMETHOD GetToFileName(nsAString &aToFileName) override;
    NS_IMETHOD SetToFileName(const nsAString &aToFileName) override;

    NS_IMETHOD GetPrinterName(nsAString &aPrinter) override;
    NS_IMETHOD SetPrinterName(const nsAString &aPrinter) override;

    NS_IMETHOD GetNumCopies(int32_t* aNumCopies) override;
    NS_IMETHOD SetNumCopies(int32_t aNumCopies) override;

    NS_IMETHOD GetScaling(double* aScaling) override;
    NS_IMETHOD SetScaling(double aScaling) override;

    NS_IMETHOD GetPaperId(nsAString& aPaperId) override;
    NS_IMETHOD SetPaperId(const nsAString& aPaperId) override;

    NS_IMETHOD SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin) override;
    NS_IMETHOD SetUnwriteableMarginTop(double aUnwriteableMarginTop) override;
    NS_IMETHOD SetUnwriteableMarginLeft(double aUnwriteableMarginLeft) override;
    NS_IMETHOD SetUnwriteableMarginBottom(double aUnwriteableMarginBottom) override;
    NS_IMETHOD SetUnwriteableMarginRight(double aUnwriteableMarginRight) override;

    NS_IMETHOD GetPaperWidth(double* aPaperWidth) override;
    NS_IMETHOD SetPaperWidth(double aPaperWidth) override;

    NS_IMETHOD GetPaperHeight(double* aPaperHeight) override;
    NS_IMETHOD SetPaperHeight(double aPaperHeight) override;

    NS_IMETHOD SetPaperSizeUnit(int16_t aPaperSizeUnit) override;

    NS_IMETHOD GetEffectivePageSize(double* aWidth, double* aHeight) override;

    NS_IMETHOD SetPageRanges(const nsTArray<int32_t>&) override;
    NS_IMETHOD GetPageRanges(nsTArray<int32_t>&) override;

    NS_IMETHOD GetResolution(int32_t* aResolution) override;
    NS_IMETHOD SetResolution(int32_t aResolution) override;

    NS_IMETHOD GetDuplex(int32_t* aDuplex) override;
    NS_IMETHOD SetDuplex(int32_t aDuplex) override;

    NS_IMETHOD GetOutputFormat(int16_t* aOutputFormat) override;
    NS_IMETHODIMP SetOutputFormat(int16_t aOutputFormat) override;


protected:
    virtual ~nsPrintSettingsQt();

    nsPrintSettingsQt(const nsPrintSettingsQt& src);
    nsPrintSettingsQt& operator=(const nsPrintSettingsQt& rhs);

    virtual nsresult _Clone(nsIPrintSettings** _retval) override;
    virtual nsresult _Assign(nsIPrintSettings* aPS) override;

    QSharedPointer<QPageLayout> mPageLayout;
    QString mFilename;
    QString mPrinterName;
    int32_t mNumCopies = 1;
    bool mPrintInColor = true;
    bool mPrintReversed = false;
    nsTArray<int32_t> mPageRanges;
    int32_t mResolution = 0;
    int32_t mDuplex = 0;
    int16_t mOutputFormat = kOutputFormatNative;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsQt, NS_PRINTSETTINGSQT_IID)
#endif // nsPrintSettingsQt_h_
