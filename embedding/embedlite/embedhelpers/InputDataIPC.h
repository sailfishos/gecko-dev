/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputDataIPC_h__
#define InputDataIPC_h__

#include "ipc/IPCMessageUtils.h"
#include "InputData.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::InputData>
{
  typedef mozilla::InputData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, (uint8_t)aParam.mInputType);
    WriteParam(aMsg, aParam.mTime);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.modifiers);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t inputType = 0;
    bool ret = ReadParam(aMsg, aIter, &inputType) &&
               ReadParam(aMsg, aIter, &aResult->mTime) &&
               ReadParam(aMsg, aIter, &aResult->mTimeStamp) &&
               ReadParam(aMsg, aIter, &aResult->modifiers);
    aResult->mInputType = static_cast<mozilla::InputType>(inputType);
    return ret;
  }
};

template<>
struct ParamTraits<mozilla::MultiTouchInput>
{
  typedef mozilla::MultiTouchInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, (uint8_t)aParam.mType);
    WriteParam(aMsg, aParam.mTouches.Length());

    for (uint32_t i = 0; i < aParam.mTouches.Length(); ++i) {
      const mozilla::SingleTouchData& data = aParam.mTouches[i];
      WriteParam(aMsg, data.mIdentifier);
      WriteParam(aMsg, data.mScreenPoint);
      WriteParam(aMsg, data.mLocalScreenPoint);
      WriteParam(aMsg, data.mRadius);
      WriteParam(aMsg, data.mRotationAngle);
      WriteParam(aMsg, data.mForce);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t inputType = 0;
    nsTArray<mozilla::SingleTouchData>::size_type numTouches = 0;
    if (!ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) ||
        !ReadParam(aMsg, aIter, &inputType) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    aResult->mType = static_cast<mozilla::MultiTouchInput::MultiTouchType>(inputType);
    for (uint32_t i = 0; i < numTouches; ++i) {
      mozilla::SingleTouchData touchData;
      if (!ReadParam(aMsg, aIter, &touchData.mIdentifier) ||
          !ReadParam(aMsg, aIter, &touchData.mScreenPoint) ||
          !ReadParam(aMsg, aIter, &touchData.mLocalScreenPoint) ||
          !ReadParam(aMsg, aIter, &touchData.mRadius) ||
          !ReadParam(aMsg, aIter, &touchData.mRotationAngle) ||
          !ReadParam(aMsg, aIter, &touchData.mForce)) {
        return false;
      }
      aResult->mTouches.AppendElement(touchData);
    }

    return true;
  }
};

} // namespace IPC

#endif // InputDataIPC_h__

