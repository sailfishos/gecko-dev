/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "EmbedLiteBrowserInit.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/PolicyContainer.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/NullPrincipal.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"
#include "nsOpenWindowInfo.h"

using mozilla::OriginAttributes;
using mozilla::RefPtr;
using mozilla::dom::BrowsingContext;
using mozilla::dom::WindowGlobalChild;
using mozilla::embedlite::BuildEmbedLiteBrowserInit;
using mozilla::embedlite::CreateEmbedLiteStandaloneInit;
using mozilla::embedlite::EmbedLiteBrowserInitData;
using mozilla::embedlite::IsUsableBrowserCreation;
using mozilla::embedlite::RestoreEmbedLiteBrowserInit;

namespace {

already_AddRefed<BrowsingContext> CreateParentContext(
    const OriginAttributes& aOriginAttributes) {
  RefPtr<BrowsingContext> context = BrowsingContext::CreateDetached(
      nullptr, nullptr, nullptr, EmptyString(), BrowsingContext::Type::Content,
      {.windowless = true});
  if (!context ||
      NS_FAILED(context->SetOriginAttributes(aOriginAttributes))) {
    return nullptr;
  }
  context->EnsureAttached();
  return context.forget();
}

void DestroyInitialWindowGlobal(WindowGlobalChild* aWindowGlobal) {
  if (aWindowGlobal && !aWindowGlobal->GetWindowGlobal()) {
    aWindowGlobal->Destroy();
  }
}

}  // namespace

TEST(EmbedLiteBrowserInitTest, StandaloneUsesBrowsingContextOriginAttributes)
{
  RefPtr<BrowsingContext> normalContext;
  nsCOMPtr<nsIOpenWindowInfo> normalOpenWindowInfo;
  ASSERT_EQ(NS_OK, CreateEmbedLiteStandaloneInit(false, normalContext,
                                                   normalOpenWindowInfo));
  ASSERT_NE(normalContext, nullptr);
  ASSERT_NE(normalOpenWindowInfo, nullptr);
  EXPECT_EQ(0u, normalContext->OriginAttributesRef().mPrivateBrowsingId);
  EXPECT_TRUE(normalOpenWindowInfo->PrincipalToInheritForAboutBlank()
                  ->OriginAttributesRef()
                  .Equals(normalContext->OriginAttributesRef()));
  EXPECT_TRUE(normalOpenWindowInfo->PartitionedPrincipalToInheritForAboutBlank()
                  ->OriginAttributesRef()
                  .Equals(normalContext->OriginAttributesRef()));
  EXPECT_FALSE(normalOpenWindowInfo->GetIsRemote());
  normalContext->Detach();

  RefPtr<BrowsingContext> privateContext;
  nsCOMPtr<nsIOpenWindowInfo> privateOpenWindowInfo;
  ASSERT_EQ(NS_OK, CreateEmbedLiteStandaloneInit(true, privateContext,
                                                   privateOpenWindowInfo));
  ASSERT_NE(privateContext, nullptr);
  EXPECT_NE(0u, privateContext->OriginAttributesRef().mPrivateBrowsingId);
  EXPECT_TRUE(privateOpenWindowInfo->PrincipalToInheritForAboutBlank()
                  ->OriginAttributesRef()
                  .Equals(privateContext->OriginAttributesRef()));
  privateContext->Detach();
}

TEST(EmbedLiteBrowserInitTest,
     SameProcessPopupPreservesInheritedAboutBlankState)
{
  OriginAttributes originAttributes;
  originAttributes.mPrivateBrowsingId = 1;
  originAttributes.mUserContextId = 7;
  RefPtr<BrowsingContext> parent = CreateParentContext(originAttributes);
  ASSERT_NE(parent, nullptr);

  nsCOMPtr<nsIPrincipal> primary =
      mozilla::NullPrincipal::Create(originAttributes);
  nsCOMPtr<nsIPrincipal> partitioned =
      mozilla::NullPrincipal::Create(originAttributes);
  ASSERT_NE(primary, nullptr);
  ASSERT_NE(partitioned, nullptr);

  nsCOMPtr<nsIURI> baseURI;
  ASSERT_EQ(NS_OK, NS_NewURI(getter_AddRefs(baseURI),
                              "https://example.invalid/base/"_ns));
  RefPtr<PolicyContainer> policyContainer = new PolicyContainer();

  RefPtr<nsOpenWindowInfo> source = new nsOpenWindowInfo();
  source->mParent = parent;
  source->mPrincipalToInheritForAboutBlank = primary;
  source->mPartitionedPrincipalToInheritForAboutBlank = partitioned;
  source->mBaseUriToInheritForAboutBlank = baseURI;
  source->mPolicyContainerToInheritForAboutBlank = policyContainer;
  source->mCoepToInheritForAboutBlank =
      mozilla::Some(nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP);
  source->mIsTopLevelCreatedByWebContent = true;
  source->mHasValidUserGestureActivation = true;
  source->mTextDirectiveUserActivation = true;

  EmbedLiteBrowserInitData initData;
  ASSERT_EQ(NS_OK, BuildEmbedLiteBrowserInit(source, 0, &initData));
  EXPECT_EQ(parent->Id(), initData.openWindowInfo().parentBrowsingContextId());
  EXPECT_EQ(originAttributes, initData.openWindowInfo().originAttributes());
  EXPECT_EQ(primary, initData.openWindowInfo().principal());
  EXPECT_EQ(partitioned, initData.openWindowInfo().partitionedPrincipal());
  EXPECT_TRUE(initData.openWindowInfo().hasValidUserGestureActivation());
  EXPECT_TRUE(initData.openWindowInfo().textDirectiveUserActivation());
  EXPECT_TRUE(initData.openWindowInfo().isTopLevelCreatedByWebContent());

  RefPtr<BrowsingContext> popup;
  RefPtr<WindowGlobalChild> initialWindowGlobal;
  nsCOMPtr<nsIOpenWindowInfo> initialOpenWindowInfo;
  nsCOMPtr<nsIOpenWindowInfo> originalOpenWindowInfo;
  ASSERT_EQ(NS_OK, RestoreEmbedLiteBrowserInit(
                       initData, popup, initialWindowGlobal,
                       initialOpenWindowInfo, originalOpenWindowInfo));
  ASSERT_NE(popup, nullptr);
  ASSERT_NE(initialWindowGlobal, nullptr);
  ASSERT_NE(initialOpenWindowInfo, nullptr);
  ASSERT_NE(originalOpenWindowInfo, nullptr);

  EXPECT_EQ(parent->Id(), popup->GetOpenerId());
  EXPECT_TRUE(popup->HadOriginalOpener());
  EXPECT_TRUE(popup->GetTopLevelCreatedByWebContent());
  EXPECT_TRUE(primary->Equals(
      originalOpenWindowInfo->PrincipalToInheritForAboutBlank()));
  EXPECT_TRUE(partitioned->Equals(
      originalOpenWindowInfo->PartitionedPrincipalToInheritForAboutBlank()));
  EXPECT_EQ(originAttributes, originalOpenWindowInfo->GetOriginAttributes());
  EXPECT_TRUE(PolicyContainer::Equals(
      policyContainer,
      PolicyContainer::Cast(
          originalOpenWindowInfo->PolicyContainerToInheritForAboutBlank())));
  ASSERT_TRUE(originalOpenWindowInfo->CoepToInheritForAboutBlank().isSome());
  EXPECT_EQ(nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP,
            *originalOpenWindowInfo->CoepToInheritForAboutBlank());
  EXPECT_TRUE(initialWindowGlobal->DocumentPrincipal()->Equals(
      initialOpenWindowInfo->PrincipalToInheritForAboutBlank()));

  nsCOMPtr<nsIURI> resolved;
  ASSERT_EQ(NS_OK, NS_NewURI(getter_AddRefs(resolved), "relative.html"_ns,
                              nullptr,
                              originalOpenWindowInfo
                                  ->BaseUriToInheritForAboutBlank()));
  EXPECT_EQ("https://example.invalid/base/relative.html"_ns,
            resolved->GetSpecOrDefault());

  DestroyInitialWindowGlobal(initialWindowGlobal);
  popup->Detach();
  parent->Detach();
}

TEST(EmbedLiteBrowserInitTest, NoopenerDoesNotAttachTheOpener)
{
  OriginAttributes originAttributes;
  RefPtr<BrowsingContext> parent = CreateParentContext(originAttributes);
  ASSERT_NE(parent, nullptr);

  nsCOMPtr<nsIPrincipal> principal =
      mozilla::NullPrincipal::Create(originAttributes);
  ASSERT_NE(principal, nullptr);
  RefPtr<nsOpenWindowInfo> source = new nsOpenWindowInfo();
  source->mParent = parent;
  source->mForceNoOpener = true;
  source->mPrincipalToInheritForAboutBlank = principal;
  source->mPartitionedPrincipalToInheritForAboutBlank = principal;
  source->mIsTopLevelCreatedByWebContent = true;

  EmbedLiteBrowserInitData initData;
  ASSERT_EQ(NS_OK, BuildEmbedLiteBrowserInit(source, 0, &initData));
  RefPtr<BrowsingContext> popup;
  RefPtr<WindowGlobalChild> initialWindowGlobal;
  nsCOMPtr<nsIOpenWindowInfo> initialOpenWindowInfo;
  nsCOMPtr<nsIOpenWindowInfo> originalOpenWindowInfo;
  ASSERT_EQ(NS_OK, RestoreEmbedLiteBrowserInit(
                       initData, popup, initialWindowGlobal,
                       initialOpenWindowInfo, originalOpenWindowInfo));
  EXPECT_EQ(0u, popup->GetOpenerId());
  EXPECT_FALSE(popup->HadOriginalOpener());

  DestroyInitialWindowGlobal(initialWindowGlobal);
  popup->Detach();
  parent->Detach();
}

TEST(EmbedLiteBrowserInitTest, RemoteLegacyCreationIsRejected)
{
  OriginAttributes originAttributes;
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::NullPrincipal::Create(originAttributes);
  ASSERT_NE(principal, nullptr);
  RefPtr<nsOpenWindowInfo> source = new nsOpenWindowInfo();
  source->mPrincipalToInheritForAboutBlank = principal;
  source->mPartitionedPrincipalToInheritForAboutBlank = principal;
  source->mIsRemote = true;

  EmbedLiteBrowserInitData initData;
  EXPECT_EQ(NS_ERROR_NOT_AVAILABLE,
            BuildEmbedLiteBrowserInit(source, 0, &initData));
}

TEST(EmbedLiteBrowserInitTest, BrowserCreationRequiresSuccessAndBrowser)
{
  int browser = 0;
  EXPECT_FALSE(IsUsableBrowserCreation(NS_ERROR_FAILURE, &browser));
  EXPECT_FALSE(IsUsableBrowserCreation(NS_OK, nullptr));
  EXPECT_TRUE(IsUsableBrowserCreation(NS_OK, &browser));
}
