/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/psm/TransportSecurityInfo.h"
#include "EmbedLog.h"

#include "EmbedLiteSecurity.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteSecurityPrivate
{
public:
    EmbedLiteSecurityPrivate(EmbedLiteSecurity *q_ptr);

public:
    EmbedLiteSecurity *q_ptr;

    bool mPopulated;
    unsigned int mState;
    bool mDomainMismatch;
    std::string mCipherName;
    bool mNotValidAtThisTime;
    bool mUntrusted;
    bool mExtendedValidation;
    EmbedLiteSecurity::TLS_VERSION mProtocolVersion;
    std::string mRawDER;
};

EmbedLiteSecurityPrivate::EmbedLiteSecurityPrivate(EmbedLiteSecurity *q_ptr)
    : q_ptr(q_ptr)
    , mPopulated(false)
    , mState(0)
    , mDomainMismatch(true)
    , mCipherName()
    , mNotValidAtThisTime(true)
    , mUntrusted(true)
    , mExtendedValidation(false)
    , mProtocolVersion(EmbedLiteSecurity::TLS_VERSION_1)
    , mRawDER()
{
}

EmbedLiteSecurity::EmbedLiteSecurity()
    : d_ptr(new EmbedLiteSecurityPrivate(this))
{

}

EmbedLiteSecurity::EmbedLiteSecurity(const char *aStatus, unsigned int aState)
    : EmbedLiteSecurity()
{
    importState(aStatus, aState);
}

EmbedLiteSecurity::~EmbedLiteSecurity()
{
    delete d_ptr;
}

void EmbedLiteSecurity::importState(const char *aStatus, unsigned int aState)
{
    bool booleanResult;
    nsresult rv = NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsITransportSecurityInfo> securityInfo;

    d_ptr->mPopulated = false;
    d_ptr->mState = aState;

    // If the status is empty, leave it as it was
    if (aStatus && *aStatus) {
        nsCString serSSLStatus(aStatus);
        rv = mozilla::psm::TransportSecurityInfo::Read(serSSLStatus, getter_AddRefs(securityInfo));

        if (!NS_SUCCEEDED(rv)) {
            LOGW("Security state change: deserialisation failed");
        }
    }

    if (NS_SUCCEEDED(rv) && securityInfo) {
        nsITransportSecurityInfo::OverridableErrorCategory errorCategory =
            nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
        rv = securityInfo->GetOverridableErrorCategory(&errorCategory);
        if (NS_SUCCEEDED(rv)) {
            d_ptr->mDomainMismatch =
                errorCategory == nsITransportSecurityInfo::OverridableErrorCategory::ERROR_DOMAIN;
            d_ptr->mNotValidAtThisTime =
                errorCategory == nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME;
            d_ptr->mUntrusted =
                errorCategory == nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TRUST;
        }

        nsCString resultCString;
        rv = securityInfo->GetCipherName(resultCString);
        if (NS_SUCCEEDED(rv)) {
            std::string cipherName(resultCString.get());
            d_ptr->mCipherName = cipherName;
        }

        rv = securityInfo->GetIsExtendedValidation(&booleanResult);
        if (NS_SUCCEEDED(rv))
            d_ptr->mExtendedValidation = booleanResult;

        nsCOMPtr<nsIX509Cert> serverCert;
        rv = securityInfo->GetServerCert(getter_AddRefs(serverCert));

        unsigned short protocolVersion;
        rv = securityInfo->GetProtocolVersion(&protocolVersion);
        if (NS_SUCCEEDED(rv))
            d_ptr->mProtocolVersion = static_cast<TLS_VERSION>(protocolVersion);

        nsTArray<uint8_t> certArray;
        if (serverCert) {
            rv = serverCert->GetRawDER(certArray);
            unsigned int length = certArray.Length();
            void *data = certArray.Elements();

            if (NS_SUCCEEDED(rv)) {
                if (data) {
                    d_ptr->mRawDER.assign((char*)data, length);
                } else {
                    d_ptr->mRawDER.clear();
                }
            } else {
                LOGW("Certificate: deserialisation failed");
            }
        } else if (NS_SUCCEEDED(rv)) {
            d_ptr->mRawDER.clear();
        }
    }

    if (NS_SUCCEEDED(rv)) {
        d_ptr->mPopulated = true;
    }
}

bool EmbedLiteSecurity::populated() const
{
    return d_ptr->mPopulated;
}

unsigned int EmbedLiteSecurity::state() const
{
    return d_ptr->mState;
}

bool EmbedLiteSecurity::domainMismatch() const
{
    return d_ptr->mDomainMismatch;
}

std::string EmbedLiteSecurity::cipherName() const
{
    return d_ptr->mCipherName;
}

bool EmbedLiteSecurity::notValidAtThisTime() const
{
    return d_ptr->mNotValidAtThisTime;
}

bool EmbedLiteSecurity::untrusted() const
{
    return d_ptr->mUntrusted;
}

bool EmbedLiteSecurity::extendedValidation() const
{
    return d_ptr->mExtendedValidation;
}

EmbedLiteSecurity::TLS_VERSION EmbedLiteSecurity::protocolVersion() const
{
    return d_ptr->mProtocolVersion;
}

std::string EmbedLiteSecurity::rawDER() const
{
    return d_ptr->mRawDER;
}

} // namespace embedlite
} // namespace mozilla
