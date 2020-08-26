/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_INIT_GLUE_H
#define EMBED_INIT_GLUE_H

#include "mozilla/embedlite/EmbedLiteAPI.h"

#ifndef EMBEDGLUE_EXPORT
#define EMBEDGLUE_EXPORT __attribute__((visibility("default")))
#endif

EMBEDGLUE_EXPORT bool LoadEmbedLite(int argc = 0, char** argv = 0);

#endif // EMBED_INIT_GLUE_H
