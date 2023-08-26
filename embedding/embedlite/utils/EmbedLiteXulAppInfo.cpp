/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteXulAppInfo.h"
#include "nsServiceManagerUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsString.h"
#include "EmbedLiteAppThreadChild.h"

#include "application.ini.h"
#include "mozilla/Unused.h"

#if defined(ACCESSIBILITY)
#include "nsAccessibilityService.h"
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

using namespace mozilla::embedlite;

EmbedLiteXulAppInfo* EmbedLiteXulAppInfo::sXulAppInfo = nullptr;

EmbedLiteXulAppInfo::EmbedLiteXulAppInfo()
{
}

already_AddRefed<EmbedLiteXulAppInfo> EmbedLiteXulAppInfo::GetSingleton()
{
  if (!sXulAppInfo) {
    auto xulAppInfo = MakeRefPtr<EmbedLiteXulAppInfo>();
    sXulAppInfo = xulAppInfo.get();
    return xulAppInfo.forget();
  }

  return do_AddRef(sXulAppInfo);
}

EmbedLiteXulAppInfo::~EmbedLiteXulAppInfo()
{
}

NS_IMPL_ISUPPORTS(EmbedLiteXulAppInfo, nsIXULRuntime, nsIXULAppInfo, nsIPlatformInfo)

NS_IMETHODIMP EmbedLiteXulAppInfo::GetID(nsACString& aID)
{
  aID.Assign("embedliteBrowser@embed.mozilla.org");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetVersion(nsACString& aVersion)
{
  aVersion.Assign(sAppData.version);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetAppBuildID(nsACString& aAppBuildID)
{
  aAppBuildID.Assign(sAppData.buildID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetName(nsACString& aName)
{
  aName.Assign("EmbedLiteApp");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetUAName(nsACString& aUAName)
{
  aUAName.Assign("Firefox");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetVendor(nsACString& aVendor)
{
  aVendor.Assign(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetPlatformVersion(nsACString& aPlatformVersion)
{
  aPlatformVersion.Assign(sAppData.version);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetPlatformBuildID(nsACString& aPlatformBuildID)
{
  aPlatformBuildID.Assign(sAppData.buildID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetProcessType(uint32_t* aProcessType)
{
  *aProcessType = XRE_GetProcessType();
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetOS(nsACString& aOS)
{
  aOS.AssignLiteral(OS_TARGET);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetXPCOMABI(nsACString& aXPCOMABI)
{
#ifdef TARGET_XPCOM_ABI
  aXPCOMABI.AssignLiteral(TARGET_XPCOM_ABI);
  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetWidgetToolkit(nsACString& aWidgetToolkit)
{
  aWidgetToolkit.Assign(MOZ_WIDGET_TOOLKIT);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetInSafeMode(bool* aInSafeMode)
{
  static const char* embedSafeModeEnv = PR_GetEnv("EMBED_SAFEMODE");
  static const bool embedSafeMode = embedSafeModeEnv && *embedSafeModeEnv == '1';

  *aInSafeMode = embedSafeMode;
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetLogConsoleErrors(bool* aLogConsoleErrors)
{
  *aLogConsoleErrors = true;
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::SetLogConsoleErrors(bool aLogConsoleErrors)
{
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::InvalidateCachesOnRestart()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::EnsureContentProcess()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetReplacedLockTime(PRTime* aReplacedLockTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetWindowsDLLBlocklistStatus(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetIsReleaseOrBeta(bool* aResult)
{
#ifdef RELEASE_OR_BETA
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetIsOfficialBranding(bool* aResult)
{
#ifdef MOZ_OFFICIAL_BRANDING
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetDefaultUpdateChannel(nsACString& aResult)
{
  aResult.AssignLiteral(MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL));
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetDistributionID(nsACString& aResult)
{
  aResult.AssignLiteral(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetProcessID(uint32_t* aResult)
{
#ifdef XP_WIN
  *aResult = GetCurrentProcessId();
#else
  *aResult = getpid();
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetUniqueProcessID(uint64_t* aResult)
{
  *aResult = 0;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetRemoteType(nsAString& aRemoteType) {
  SetDOMStringToNull(aRemoteType);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetBrowserTabsRemoteAutostart(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetMaxWebProcessCount(uint32_t* aResult) {
  *aResult = mozilla::GetMaxWebProcessCount();
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetAccessibilityEnabled(bool* aResult)
{
#ifdef ACCESSIBILITY
  *aResult = GetAccService() != nullptr;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetAccessibleHandlerUsed(bool* aResult) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = Preferences::GetBool("accessibility.handler.enabled", false) &&
             a11y::IsHandlerRegistered();
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetAccessibilityInstantiator(nsAString& aInstantiator) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  if (!GetAccService()) {
    aInstantiator = u""_ns;
    return NS_OK;
  }
  nsAutoString ipClientInfo;
  a11y::Compatibility::GetHumanReadableConsumersStr(ipClientInfo);
  aInstantiator.Append(ipClientInfo);
  aInstantiator.AppendLiteral("|");

  nsCOMPtr<nsIFile> oopClientExe;
  if (a11y::GetInstantiator(getter_AddRefs(oopClientExe))) {
    nsAutoString oopClientInfo;
    if (NS_SUCCEEDED(oopClientExe->GetPath(oopClientInfo))) {
      aInstantiator.Append(oopClientInfo);
    }
  }
#else
  aInstantiator = u""_ns;
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetShouldBlockIncompatJaws(bool* aResult) {
  *aResult = false;
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  *aResult = mozilla::a11y::Compatibility::IsOldJAWS();
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetIs64Bit(bool* aResult)
{
#ifdef HAVE_64BIT_BUILD
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetSourceURL(nsACString &aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetUpdateURL(nsACString &aResult) {
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetRestartedByOS(bool *aResult)
{
  // TODO: implement gRestartedByOS flag
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetLauncherProcessState(uint32_t *aResult) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetLastAppVersion(nsACString &aResult) {
  aResult.Assign(sAppData.version);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetLastAppBuildID(nsACString &aResult) {
  aResult.Assign(sAppData.buildID);
  return NS_OK;
}
