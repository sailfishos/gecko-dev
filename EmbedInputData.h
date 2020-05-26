/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EmbedLite_EmbedInputData_h__
#define mozilla_EmbedLite_EmbedInputData_h__

#include <vector>

namespace mozilla {

namespace embedlite {

struct TouchPointF
{
  TouchPointF(float x, float y)
    : x(x)
    , y(y)
  {}

  float x;
  float y;
};

// Simplified wrapper for SingleTouchData of the InputData.h
struct TouchData
{
  TouchData(int32_t identifier,
            TouchPointF touchPoint,
            float pressure)
    : identifier(identifier)
    , touchPoint(touchPoint)
    , pressure(pressure)
  {}

  int32_t identifier;
  TouchPointF touchPoint;
  float pressure;
};

class EmbedTouchInput
{
public:
  // Keep this sync with InputData.h
  enum EmbedTouchType
  {
    MULTITOUCH_START,
    MULTITOUCH_MOVE,
    MULTITOUCH_END,
    MULTITOUCH_CANCEL,

    // Used as an upper bound for ContiguousEnumSerializer
    MULTITOUCH_SENTINEL,
  };

  EmbedTouchInput(EmbedTouchType type, uint32_t timeStamp)
    : type(type)
    , timeStamp(timeStamp)
  {}

  EmbedTouchType type;
  uint32_t timeStamp;

  std::vector<TouchData> touches;
};

}  // namespace embedlite
}  // namespace mozilla

#endif // mozilla_EmbedLite_EmbedInputData_h__
