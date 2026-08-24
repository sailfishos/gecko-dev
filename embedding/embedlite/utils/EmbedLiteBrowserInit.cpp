/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteBrowserInit.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/PolicyContainer.h"
#include "mozilla/dom/WindowGlobalActor.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ScopeExit.h"
#include "nsIPrincipal.h"
#include "nsILoadInfo.h"
#include "nsIWebBrowserChrome.h"
#include "nsNetUtil.h"
#include "nsOpenWindowInfo.h"
#include "nsXULAppAPI.h"
#include "mozilla/NullPrincipal.h"

namespace mozilla::embedlite {

namespace {

nsresult SerializeOpenWindowInfo(nsIOpenWindowInfo* aOpenWindowInfo,
                                 EmbedLiteOpenWindowInfoData& aData) {
  NS_ENSURE_ARG(aOpenWindowInfo);

  RefPtr<dom::BrowsingContext> parent = aOpenWindowInfo->GetParent();
  aData.parentBrowsingContextId() = parent ? parent->Id() : 0;
  aData.forceNoOpener() = aOpenWindowInfo->GetForceNoOpener();
  aData.isRemote() = aOpenWindowInfo->GetIsRemote();
  aData.isForPrinting() = aOpenWindowInfo->GetIsForPrinting();
  aData.isForWindowDotPrint() = aOpenWindowInfo->GetIsForWindowDotPrint();
  aData.isTopLevelCreatedByWebContent() =
      aOpenWindowInfo->GetIsTopLevelCreatedByWebContent();
  aData.hasValidUserGestureActivation() =
      aOpenWindowInfo->GetHasValidUserGestureActivation();
  aData.textDirectiveUserActivation() =
      aOpenWindowInfo->GetTextDirectiveUserActivation();
  aData.originAttributes() = aOpenWindowInfo->GetOriginAttributes();

  nsCOMPtr<nsIPrincipal> principal =
      aOpenWindowInfo->PrincipalToInheritForAboutBlank();
  nsCOMPtr<nsIPrincipal> partitionedPrincipal =
      aOpenWindowInfo->PartitionedPrincipalToInheritForAboutBlank();
  NS_ENSURE_TRUE(principal && partitionedPrincipal, NS_ERROR_INVALID_ARG);
  aData.principal() = principal;
  aData.partitionedPrincipal() = partitionedPrincipal;
  aData.baseURI() = aOpenWindowInfo->BaseUriToInheritForAboutBlank();

  if (nsCOMPtr<nsIPolicyContainer> policyContainer =
          aOpenWindowInfo->PolicyContainerToInheritForAboutBlank()) {
    ipc::PolicyContainerArgs policyContainerArgs;
    PolicyContainer::ToArgs(PolicyContainer::Cast(policyContainer),
                            policyContainerArgs);
    aData.policyContainer() = Some(std::move(policyContainerArgs));
  }
  if (const auto& coep = aOpenWindowInfo->CoepToInheritForAboutBlank();
      coep.isSome()) {
    aData.coep() = Some(*coep);
  }
  return NS_OK;
}

nsresult DeserializeOpenWindowInfo(const EmbedLiteOpenWindowInfoData& aData,
                                   nsCOMPtr<nsIOpenWindowInfo>& aResult) {
  NS_ENSURE_TRUE(aData.principal() && aData.partitionedPrincipal(),
                 NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aData.originAttributes().EqualsIgnoringFPD(
                     aData.principal()->OriginAttributesRef()) &&
                     aData.originAttributes().EqualsIgnoringFPD(
                         aData.partitionedPrincipal()->OriginAttributesRef()),
                 NS_ERROR_INVALID_ARG);

  RefPtr<nsOpenWindowInfo> openWindowInfo = new nsOpenWindowInfo();
  if (aData.parentBrowsingContextId()) {
    openWindowInfo->mParent =
        dom::BrowsingContext::Get(aData.parentBrowsingContextId());
    NS_ENSURE_TRUE(openWindowInfo->mParent, NS_ERROR_NOT_AVAILABLE);
  }
  openWindowInfo->mForceNoOpener = aData.forceNoOpener();
  openWindowInfo->mIsRemote = aData.isRemote();
  openWindowInfo->mIsForPrinting = aData.isForPrinting();
  openWindowInfo->mIsForWindowDotPrint = aData.isForWindowDotPrint();
  openWindowInfo->mIsTopLevelCreatedByWebContent =
      aData.isTopLevelCreatedByWebContent();
  openWindowInfo->mHasValidUserGestureActivation =
      aData.hasValidUserGestureActivation();
  openWindowInfo->mTextDirectiveUserActivation =
      aData.textDirectiveUserActivation();
  openWindowInfo->mPrincipalToInheritForAboutBlank = aData.principal();
  openWindowInfo->mPartitionedPrincipalToInheritForAboutBlank =
      aData.partitionedPrincipal();
  openWindowInfo->mBaseUriToInheritForAboutBlank = aData.baseURI();
  if (aData.policyContainer().isSome()) {
    RefPtr<PolicyContainer> policyContainer;
    PolicyContainer::FromArgs(*aData.policyContainer(), nullptr,
                              getter_AddRefs(policyContainer));
    NS_ENSURE_TRUE(policyContainer, NS_ERROR_FAILURE);
    openWindowInfo->mPolicyContainerToInheritForAboutBlank = policyContainer;
  }
  if (aData.coep().isSome()) {
    openWindowInfo->mCoepToInheritForAboutBlank = aData.coep();
  }
  aResult = openWindowInfo;
  return NS_OK;
}

}  // namespace

nsresult BuildEmbedLiteBrowserInit(nsIOpenWindowInfo* aOpenWindowInfo,
                                   uint32_t aChromeFlags,
                                   EmbedLiteBrowserInitData* aInitData) {
  NS_ENSURE_ARG_POINTER(aInitData);
  NS_ENSURE_ARG(aOpenWindowInfo);
  NS_ENSURE_TRUE(XRE_IsParentProcess() && !aOpenWindowInfo->GetIsRemote(),
                 NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(!(aChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW),
                 NS_ERROR_NOT_AVAILABLE);

  EmbedLiteOpenWindowInfoData openWindowInfoData;
  MOZ_TRY(SerializeOpenWindowInfo(aOpenWindowInfo, openWindowInfoData));

  RefPtr<dom::BrowsingContext> parent = aOpenWindowInfo->GetParent();
  RefPtr<dom::BrowsingContext> opener =
      aOpenWindowInfo->GetForceNoOpener() ? nullptr : parent;
  RefPtr<dom::BrowsingContext> browsingContext =
      dom::BrowsingContext::CreateDetached(
          nullptr, opener, nullptr, EmptyString(),
          dom::BrowsingContext::Type::Content,
          {.topLevelCreatedByWebContent =
               aOpenWindowInfo->GetIsTopLevelCreatedByWebContent(),
           .isForPrinting = aOpenWindowInfo->GetIsForPrinting(),
           .windowless = true});
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_OUT_OF_MEMORY);

  MOZ_TRY(browsingContext->SetOriginAttributes(
      openWindowInfoData.originAttributes()));
  browsingContext->InitPendingInitialization(true);
  auto clearPendingInitialization = MakeScopeExit([&] {
    (void)browsingContext->SetPendingInitialization(false);
  });
  browsingContext->EnsureAttached();

  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::Create(browsingContext->OriginAttributesRef());
  NS_ENSURE_TRUE(initialPrincipal, NS_ERROR_OUT_OF_MEMORY);

  aInitData->initialWindowGlobal() =
      dom::WindowGlobalActor::AboutBlankInitializer(browsingContext,
                                                     initialPrincipal);
  aInitData->openWindowInfo() = std::move(openWindowInfoData);
  aInitData->chromeFlags() = aChromeFlags;
  return NS_OK;
}

nsresult RestoreEmbedLiteBrowserInit(
    const EmbedLiteBrowserInitData& aInitData,
    RefPtr<dom::BrowsingContext>& aBrowsingContext,
    RefPtr<dom::WindowGlobalChild>& aInitialWindowGlobal,
    nsCOMPtr<nsIOpenWindowInfo>& aInitialOpenWindowInfo,
    nsCOMPtr<nsIOpenWindowInfo>& aOriginalOpenWindowInfo) {
  const dom::WindowGlobalInit& windowInit = aInitData.initialWindowGlobal();
  NS_ENSURE_TRUE(XRE_IsParentProcess() &&
                     !aInitData.openWindowInfo().isRemote() &&
                     !(aInitData.chromeFlags() &
                       nsIWebBrowserChrome::CHROME_FISSION_WINDOW),
                 NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(windowInit.isInitialDocument() && windowInit.principal() &&
                     windowInit.documentURI() &&
                     NS_IsAboutBlank(windowInit.documentURI()),
                 NS_ERROR_INVALID_ARG);

  RefPtr<dom::BrowsingContext> browsingContext = dom::BrowsingContext::Get(
      windowInit.context().mBrowsingContextId);
  NS_ENSURE_TRUE(browsingContext && !browsingContext->IsDiscarded(),
                 NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIOpenWindowInfo> originalOpenWindowInfo;
  MOZ_TRY(DeserializeOpenWindowInfo(aInitData.openWindowInfo(),
                                    originalOpenWindowInfo));
  NS_ENSURE_TRUE(aInitData.openWindowInfo().originAttributes()
                         .EqualsIgnoringFPD(
                             browsingContext->OriginAttributesRef()) &&
                     originalOpenWindowInfo->GetOriginAttributes()
                         .EqualsIgnoringFPD(
                             browsingContext->OriginAttributesRef()),
                 NS_ERROR_INVALID_ARG);

  RefPtr<dom::WindowGlobalChild> initialWindowGlobal =
      dom::WindowGlobalChild::CreateDisconnected(windowInit);
  NS_ENSURE_TRUE(initialWindowGlobal, NS_ERROR_FAILURE);
  initialWindowGlobal->Init();

  nsCOMPtr<nsIOpenWindowInfo> initialOpenWindowInfo;
  MOZ_TRY(originalOpenWindowInfo->CloneWithPrincipals(
      windowInit.principal(), windowInit.principal(),
      getter_AddRefs(initialOpenWindowInfo)));

  aBrowsingContext = browsingContext;
  aInitialWindowGlobal = initialWindowGlobal;
  aInitialOpenWindowInfo = initialOpenWindowInfo;
  aOriginalOpenWindowInfo = originalOpenWindowInfo;
  return NS_OK;
}

void DiscardEmbedLiteBrowserInit(const EmbedLiteBrowserInitData& aInitData) {
  RefPtr<dom::BrowsingContext> browsingContext = dom::BrowsingContext::Get(
      aInitData.initialWindowGlobal().context().mBrowsingContextId);
  if (browsingContext && !browsingContext->IsDiscarded() &&
      !browsingContext->GetDocShell()) {
    browsingContext->Detach();
  }
}

nsresult CreateEmbedLiteStandaloneInit(
    bool aIsPrivateWindow, RefPtr<dom::BrowsingContext>& aBrowsingContext,
    nsCOMPtr<nsIOpenWindowInfo>& aOpenWindowInfo) {
  NS_ENSURE_TRUE(XRE_IsParentProcess(), NS_ERROR_NOT_AVAILABLE);
  RefPtr<dom::BrowsingContext> browsingContext =
      dom::BrowsingContext::CreateDetached(
          nullptr, nullptr, nullptr, EmptyString(),
          dom::BrowsingContext::Type::Content, {.windowless = true});
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_OUT_OF_MEMORY);
  MOZ_TRY(browsingContext->SetUsePrivateBrowsing(aIsPrivateWindow));
  browsingContext->EnsureAttached();
  browsingContext->InitSessionHistory();

  nsCOMPtr<nsIPrincipal> principal =
      NullPrincipal::Create(browsingContext->OriginAttributesRef());
  NS_ENSURE_TRUE(principal, NS_ERROR_OUT_OF_MEMORY);
  RefPtr<nsOpenWindowInfo> openWindowInfo = new nsOpenWindowInfo();
  openWindowInfo->mPrincipalToInheritForAboutBlank = principal;
  openWindowInfo->mPartitionedPrincipalToInheritForAboutBlank = principal;
  openWindowInfo->mIsRemote = false;

  aBrowsingContext = browsingContext;
  aOpenWindowInfo = openWindowInfo;
  return NS_OK;
}

bool IsUsableBrowserCreation(nsresult aResult, const void* aBrowser) {
  return NS_SUCCEEDED(aResult) && aBrowser;
}

}  // namespace mozilla::embedlite
