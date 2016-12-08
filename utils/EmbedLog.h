/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBED_LOG_H
#define MOZ_EMBED_LOG_H

#include <stdio.h>
#include "Logging.h"

#ifdef PR_LOGGING

#ifdef EMBED_LITE_INTERNAL

extern mozilla::LogModule* GetEmbedCommonLog(const char* aModule);

#define LOGF(FMT, ...) MOZ_LOG(GetEmbedCommonLog("EmbedLite"), mozilla::LogLevel::Error, ("FUNC::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))
#define LOGT(FMT, ...) MOZ_LOG(GetEmbedCommonLog("EmbedLite"), mozilla::LogLevel::Debug, ("TRACE::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))
#define LOGW(FMT, ...) MOZ_LOG(GetEmbedCommonLog("EmbedLite"), mozilla::LogLevel::Info, ("WARN: EmbedLite::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))
#define LOGE(FMT, ...) MOZ_LOG(GetEmbedCommonLog("EmbedLite"), mozilla::LogLevel::Warning, ("ERROR: EmbedLite::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))
#define LOGNI(FMT, ...) MOZ_LOG(GetEmbedCommonLog("EmbedLite"), mozilla::LogLevel::Error, ("NON_IMPL: EmbedLite::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))

#define LOGC(CUSTOMNAME, FMT, ...) MOZ_LOG(GetEmbedCommonLog(CUSTOMNAME), mozilla::LogLevel::Info, (CUSTOMNAME "::%s:%d " FMT , __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__))

#else // EMBED_LITE_INTERNAL

#define LOGT(FMT, ...) fprintf(stderr, \
                               "EmbedLiteExt %s:%d: " FMT "\n", __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif // EMBED_LITE_INTERNAL

#else // PR_LOGGING

#define LOGF(...) do {} while (0)
#define LOGT(...) do {} while (0)
#define LOGW(...) do {} while (0)
#define LOGE(...) do {} while (0)
#define LOGNI(...) do {} while (0)
#define LOGC(...) do {} while (0)

#endif // PR_LOGGING

#endif // MOZ_EMBED_LOG_H
