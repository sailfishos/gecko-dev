/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsServiceManagerUtils.h"
#include "nsISerializationHelper.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsString.h"
#include "nsCOMPtr.h"
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
    nsCOMPtr<nsISupports> infoObj;

    d_ptr->mPopulated = false;
    d_ptr->mState = aState;

    // If the status is empty, leave it as it was
    if (aStatus && *aStatus) {
        nsCOMPtr<nsISerializationHelper> serialHelper = do_GetService("@mozilla.org/network/serialization-helper;1");

        nsCString serSSLStatus(aStatus);
        rv = serialHelper->DeserializeObject(serSSLStatus, getter_AddRefs(infoObj));

        if (!NS_SUCCEEDED(rv)) {
            LOGW("Security state change: deserialisation failed");
        }
    }

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransportSecurityInfo> securityInfo = do_QueryInterface(infoObj);

        securityInfo->GetIsDomainMismatch(&booleanResult);
        d_ptr->mDomainMismatch = booleanResult;

        nsCString resultCString;
        securityInfo->GetCipherName(resultCString);
        std::string cipherName(resultCString.get());
        d_ptr->mCipherName = cipherName;

        securityInfo->GetIsNotValidAtThisTime(&booleanResult);
        d_ptr->mNotValidAtThisTime = booleanResult;

        securityInfo->GetIsUntrusted(&booleanResult);
        d_ptr->mUntrusted = booleanResult;

        securityInfo->GetIsExtendedValidation(&booleanResult);
        d_ptr->mExtendedValidation = booleanResult;

        nsIX509Cert * aServerCert;
        securityInfo->GetServerCert(&aServerCert);

        unsigned short protocolVersion;
        securityInfo->GetProtocolVersion(&protocolVersion);
        d_ptr->mProtocolVersion = static_cast<TLS_VERSION>(protocolVersion);

        Maybe<nsTArray<uint8_t>> certArray;
        rv = aServerCert->GetRawDER(*certArray);
        unsigned int length = certArray->Length();
        void *data = certArray->Elements();

        if (NS_SUCCEEDED(rv)) {
            if (data) {
                d_ptr->mRawDER.assign((char*)data, length);
            }
            else {
                d_ptr->mRawDER.clear();
            }
        }
        else {
            LOGW("Certificate: deserialisation failed");
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
