/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteSecurity_H_
#define EmbedLiteSecurity_H_

#include <string>

namespace mozilla {
namespace embedlite {

class EmbedLiteSecurityPrivate;

class EmbedLiteSecurity
{
public:
    enum TLS_VERSION {
        SSL_VERSION_3 = 0,
        TLS_VERSION_1 = 1,
        TLS_VERSION_1_1 = 2,
        TLS_VERSION_1_2 = 3,
        TLS_VERSION_1_3 = 4
    };

    EmbedLiteSecurity();
    EmbedLiteSecurity(const char *aStatus, unsigned int aState);
    virtual ~EmbedLiteSecurity();

    virtual void importState(const char *aStatus, unsigned int aState);

    virtual bool populated() const;
    virtual unsigned int state() const;
    virtual bool domainMismatch() const;
    virtual std::string cipherName() const;
    virtual bool notValidAtThisTime() const;
    virtual bool untrusted() const;
    virtual bool extendedValidation() const;
    virtual TLS_VERSION protocolVersion() const;
    virtual std::string rawDER() const;

private:
    EmbedLiteSecurityPrivate * d_ptr;
};

} // namespace embedlite
} // namespace mozilla

#endif /*EmbedLiteSecurity_H_*/
