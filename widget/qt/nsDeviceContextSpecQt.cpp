/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <QTemporaryFile>

#define SET_PRINTER_FEATURES_VIA_PREFS 1
#define PRINTERFEATURES_PREF "print.tmp.printerfeatures"

#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/Logging.h"

#include "plstr.h"

#include "nsDeviceContextSpecQt.h"

#include "prenv.h" /* for PR_GetEnv */

#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsIServiceManager.h"
#include "nsPrintSettingsQt.h"
#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsTArray.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace mozilla;
using namespace mozilla::gfx;

static LazyLogModule DeviceContextSpecQtLM("DeviceContextSpecQt");
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) MOZ_LOG(DeviceContextSpecQtLM, mozilla::LogLevel::Debug, x)

nsDeviceContextSpecQt::nsDeviceContextSpecQt()
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::nsDeviceContextSpecQt()\n"));
}

nsDeviceContextSpecQt::~nsDeviceContextSpecQt()
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::~nsDeviceContextSpecQt()\n"));
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecQt,
        nsIDeviceContextSpec)

already_AddRefed<PrintTarget> nsDeviceContextSpecQt::MakePrintTarget()
{
    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);

    // If we're in landscape mode, we'll be rotating the output --
    // need to swap width & height.
    int32_t orientation;
    mPrintSettings->GetOrientation(&orientation);
    if (nsIPrintSettings::kLandscapeOrientation == orientation) {
        double tmp = width;
        width = height;
        height = tmp;
    }

    // convert twips to points
    width  /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    DO_PR_DEBUG_LOG(("\"%s\", %f, %f\n", mPath, width, height));

    QTemporaryFile file;
    if(!file.open()) {
        return nullptr;
    }
    file.setAutoRemove(false);

    nsresult rv = NS_NewNativeLocalFile(
            nsDependentCString(file.fileName().toUtf8().constData()),
            false,
            getter_AddRefs(mSpoolFile));
    if (NS_FAILED(rv)) {
        file.remove();
        return nullptr;
    }

    mSpoolName = file.fileName().toUtf8().constData();

    mSpoolFile->SetPermissions(0600);

    nsCOMPtr<nsIFileOutputStream> stream =
        do_CreateInstance("@mozilla.org/network/file-output-stream;1");

    rv = stream->Init(mSpoolFile, -1, -1, 0);
    if (NS_FAILED(rv))
        return nullptr;

    int16_t format;
    mPrintSettings->GetOutputFormat(&format);

    if (format == nsIPrintSettings::kOutputFormatNative) {
        if (mIsPPreview) {
            // There is nothing to detect on Print Preview, use PS.
            // TODO: implement for Qt?
            //format = nsIPrintSettings::kOutputFormatPS;
            return nullptr;
        }
        format = nsIPrintSettings::kOutputFormatPDF;
    }

    IntSize size = IntSize::Truncate(width, height);

    if (format == nsIPrintSettings::kOutputFormatPDF) {
        return PrintTargetPDF::CreateOrNull(stream, size);
    }

    return nullptr;
}

NS_IMETHODIMP nsDeviceContextSpecQt::Init(nsIWidget* aWidget,
        nsIPrintSettings* aPS,
        bool aIsPrintPreview)
{
    DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::Init(aPS=%p)\n", aPS));

    mPrintSettings = aPS;
    mIsPPreview = aIsPrintPreview;

    // This is only set by embedders
    bool toFile;
    aPS->GetPrintToFile(&toFile);

    mToPrinter = !toFile && !aIsPrintPreview;

    nsCOMPtr<nsPrintSettingsQt> printSettingsQt(do_QueryInterface(aPS));
    if (!printSettingsQt)
        return NS_ERROR_NO_INTERFACE;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::BeginDocument(
        const nsAString& aTitle,
        const nsAString& aPrintToFileName,
        int32_t aStartPage,
        int32_t aEndPage)
{
    if (mToPrinter) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::EndDocument()
{
    if (mToPrinter) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    // Handle print-to-file ourselves for the benefit of embedders
    nsString targetPath;
    nsCOMPtr<nsIFile> destFile;
    mPrintSettings->GetToFileName(targetPath);

    nsresult rv = NS_NewLocalFile(targetPath, false, getter_AddRefs(destFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString destLeafName;
    rv = destFile->GetLeafName(destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> destDir;
    rv = destFile->GetParent(getter_AddRefs(destDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSpoolFile->MoveTo(destDir, destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is the standard way to get the UNIX umask. Ugh.
    mode_t mask = umask(0);
    umask(mask);
    // If you're not familiar with umasks, they contain the bits of what NOT
    // to set in the permissions
    // (thats because files and directories have different numbers of bits
    // for their permissions)
    destFile->SetPermissions(0666 & ~(mask));

    return NS_OK;
}

//  Printer Enumerator
nsPrinterEnumeratorQt::nsPrinterEnumeratorQt()
{
}

nsPrinterEnumeratorQt::~nsPrinterEnumeratorQt()
{
}

NS_IMPL_ISUPPORTS(nsPrinterEnumeratorQt, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorQt::GetPrinterNameList(
        nsIStringEnumerator** aPrinterNameList)
{
    NS_ENSURE_ARG_POINTER(aPrinterNameList);
    *aPrinterNameList = nullptr;

    nsTArray<nsString>* printers =
        new nsTArray<nsString>(0);

    return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

NS_IMETHODIMP nsPrinterEnumeratorQt::GetDefaultPrinterName(nsAString &aDefaultPrinterName)
{
    DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::GetDefaultPrinterName()\n"));
    return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter(
        const nsAString &aPrinterName,
        nsIPrintSettings* aPrintSettings)
{
    DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter()"));
    // XXX Leave NS_OK for now
    // Probably should use NS_ERROR_NOT_IMPLEMENTED
    return NS_OK;
}
