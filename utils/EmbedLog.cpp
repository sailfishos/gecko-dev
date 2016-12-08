/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include <string>
#include <map>

#ifdef PR_LOGGING

LogModule*
GetEmbedCommonLog(const char* aModule)
{
  static std::map<std::string, LogModule*> sLogMap;
  std::map<std::string, LogModule*>::iterator it = sLogMap.find(aModule);
  LogModule* retVal;
  if (sLogMap.end() == it) {
    printf("Created LOG for %s\n", aModule);
    sLogMap[aModule] = retVal = LazyLogModule(aModule);
  } else {
    retVal = it->second;
  }
  return retVal;
}

#endif
