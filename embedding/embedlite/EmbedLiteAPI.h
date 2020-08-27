/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_API_H
#define EMBED_LITE_API_H

#include "nscore.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
}
}

#ifdef EXPORT_XPCOM_API
#undef EXPORT_XPCOM_API
#endif

#ifdef IMPORT_XPCOM_API
#undef IMPORT_XPCOM_API
#endif

#define EXPORT_XPCOM_API(type) extern "C" NS_EXPORT type NS_FROZENCALL
#define IMPORT_XPCOM_API(type) extern "C" NS_IMPORT type NS_FROZENCALL

/**
 * Import/export macros for EmbedLite APIs.
 * See sha1 d616a7ec4475c8 & c7352a66573b6 from gecko git  mirror.
 */

#if defined(IMPL_LIBXUL)
#define EMBEDLITE_API(type, name, params) EXPORT_XPCOM_API(type) name params;
#else
#define EMBEDLITE_API(type, name, params) IMPORT_XPCOM_API(type) name params;
#endif

EMBEDLITE_API(mozilla::embedlite::EmbedLiteApp*, XRE_GetEmbedLite, ())

#endif // EMBED_LITE_API_H
