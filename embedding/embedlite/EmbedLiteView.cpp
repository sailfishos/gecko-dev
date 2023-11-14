/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteView.h"
#include "EmbedInputData.h"
#include "EmbedLiteApp.h"

#include "mozilla/Unused.h"
#include "nsServiceManagerUtils.h"

#include "EmbedLiteViewThreadParent.h"

// Image as URL includes
#include "gfxImageSurface.h"
#include "mozilla/Base64.h"
#include "imgIEncoder.h"
#include "InputData.h"

#include <sys/syscall.h>

namespace mozilla {
namespace embedlite {

class FakeListener : public EmbedLiteViewListener {};
static FakeListener sFakeListener;

EmbedLiteView::EmbedLiteView(EmbedLiteApp* aApp, EmbedLiteWindow* aWindow,  PEmbedLiteViewParent* aViewImpl, uint32_t aViewId)
  : mApp(aApp)
  , mWindow(aWindow)
  , mListener(NULL)
  , mViewImpl(dynamic_cast<EmbedLiteViewIface*>(aViewImpl))
  , mViewParent(aViewImpl)
  , mUniqueID(aViewId)
  , mMarginsChanging(false)
  , mDynamicToolbarHeightChanging(false)
  , mMargins(0, 0, 0, 0)
  , mDynamicToolbarHeight(0)
{
  LOGT();
  dynamic_cast<EmbedLiteViewIface*>(aViewImpl)->SetEmbedAPIView(this);
}

EmbedLiteView::~EmbedLiteView()
{
  LOGT("impl:%p", mViewImpl);
  if (mViewImpl) {
    mViewImpl->ViewAPIDestroyed();
  }
}

void
EmbedLiteView::Destroy()
{
  MOZ_ASSERT(mViewParent);
  Unused << mViewParent->SendDestroy();
}

void
EmbedLiteView::Destroyed()
{
  if (mListener) {
    mListener->ViewDestroyed();
  }
  EmbedLiteApp::GetInstance()->ViewDestroyed(mUniqueID);
}

void
EmbedLiteView::SetListener(EmbedLiteViewListener* aListener)
{
   mListener = aListener;
}

EmbedLiteViewListener *EmbedLiteView::GetListener() const
{
  return mListener ? mListener : &sFakeListener;
}

EmbedLiteViewIface*
EmbedLiteView::GetImpl()
{
  return mViewImpl;
}

void
EmbedLiteView::LoadURL(const char* aUrl)
{
  LOGT("url:%s", aUrl);
  Unused << mViewParent->SendLoadURL(NS_ConvertUTF8toUTF16(nsDependentCString(aUrl)));
}

void
EmbedLiteView::SetIsActive(bool aIsActive)
{
  LOGT("active: %d thread %ld", aIsActive, syscall(SYS_gettid));

  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendSetIsActive(aIsActive);
  // Make sure active view content controller is always registered with
  // APZCTreeManager for the window.

  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetIsActive(aIsActive);

  if (aIsActive) {
    static_cast<EmbedLiteViewParent*>(mViewParent)->UpdateScrollController();
  }
}

void
EmbedLiteView::SetIsFocused(bool aIsFocused)
{
  LOGT("focus: %d thread %ld", aIsFocused, syscall(SYS_gettid));

  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendSetIsFocused(aIsFocused);
}

void
EmbedLiteView::SetDesktopMode(bool aDesktopMode)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendSetDesktopMode(aDesktopMode);
}

void
EmbedLiteView::SetThrottlePainting(bool aThrottle)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendSetThrottlePainting(aThrottle);
}

void
EmbedLiteView::SuspendTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendSuspendTimeouts();
}

void
EmbedLiteView::ResumeTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendResumeTimeouts();
}

void EmbedLiteView::GoBack(bool aRequireUserInteraction, bool aUserActivation)
{
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendGoBack(aRequireUserInteraction, aUserActivation);
}

void EmbedLiteView::GoForward(bool aRequireUserInteraction, bool aUserActivation)
{
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendGoForward(aRequireUserInteraction, aUserActivation);
}

void EmbedLiteView::StopLoad()
{
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendStopLoad();

}

void EmbedLiteView::Reload(bool hard)
{
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendReload(hard);
}

void
EmbedLiteView::SetHttpUserAgent(const char16_t* aHttpUserAgent)
{
    LOGT();
    NS_ENSURE_TRUE(mViewParent, );
    const nsDependentString httpUserAgent(aHttpUserAgent);
    Unused << mViewParent->SendSetHttpUserAgent(httpUserAgent);
}

void EmbedLiteView::ScrollTo(int x, int y)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendScrollTo(x, y);
}

void EmbedLiteView::ScrollBy(int x, int y)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendScrollBy(x, y);
}

void
EmbedLiteView::LoadFrameScript(const char* aURI)
{
  LOGT("uri:%s, mViewImpl:%p", aURI, mViewImpl);
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendLoadFrameScript(NS_ConvertUTF8toUTF16(nsDependentCString(aURI)));
}

void
EmbedLiteView::AddMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendAddMessageListener(nsDependentCString(aName));
}

void
EmbedLiteView::RemoveMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendRemoveMessageListener(nsDependentCString(aName));
}

void EmbedLiteView::AddMessageListeners(const std::vector<std::string> &aMessageNames)
{
  NS_ENSURE_TRUE(mViewParent, );

  nsTArray<nsString> messages;
  for (const auto &message : aMessageNames) {
      messages.AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(message.c_str())));
  }

  Unused << mViewParent->SendAddMessageListeners(messages);
}

void EmbedLiteView::RemoveMessageListeners(const std::vector<std::string> &aMessageNames)
{
  NS_ENSURE_TRUE(mViewParent, );
  nsTArray<nsString> messages;
  for (const auto &message : aMessageNames) {
      messages.AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(message.c_str())));
  }

  Unused << mViewParent->SendRemoveMessageListeners(messages);
}

void
EmbedLiteView::SendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  NS_ENSURE_TRUE(mViewParent, );

  const nsDependentString msgname(aMessageName);
  const nsDependentString msg(aMessage);
  Unused << mViewParent->SendAsyncMessage(msgname, msg);
}

// Render interface

void EmbedLiteView::SetDynamicToolbarHeight(int height)
{
    mDynamicToolbarHeight = height;

    if (!mDynamicToolbarHeightChanging) {
        mDynamicToolbarHeightChanging = true;
        Unused << mViewParent->SendSetDynamicToolbarHeight(height);
    }
}

void EmbedLiteView::SetScreenProperties(const int &depth, const float &density, const float &dpi)
{
    SetDPI(dpi);
    Unused << mViewParent->SendSetScreenProperties(depth, density, dpi);
}

void EmbedLiteView::DynamicToolbarHeightChanged(int height)
{
    if (mDynamicToolbarHeight != height) {
        Unused << mViewParent->SendSetDynamicToolbarHeight(mDynamicToolbarHeight);
    } else {
        mDynamicToolbarHeightChanging = false;
        if (mListener) {
            mListener->OnDynamicToolbarHeightChanged();
        }
    }
}

void EmbedLiteView::SetMargins(int top, int right, int bottom, int left)
{
    mMargins.SizeTo(top, right, bottom, left);

    if (!mMarginsChanging) {
        mMarginsChanging = true;
        Unused << mViewParent->SendSetMargins(top, right, bottom, left);
    }
}

void EmbedLiteView::MarginsChanged(int top, int right, int bottom, int left)
{
    if (mMargins != mozilla::gfx::IntMargin(top, right, bottom, left)) {
         Unused << mViewParent->SendSetMargins(mMargins.top, mMargins.right, mMargins.bottom, mMargins.left);
    } else {
        mMarginsChanging = false;
    }
}

void
EmbedLiteView::ScheduleUpdate()
{
  NS_ENSURE_TRUE(mViewParent, );
  Unused << mViewParent->SendScheduleUpdate();
}

void
EmbedLiteView::SetDPI(const float& dpi)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetDPI(dpi);
}

void
EmbedLiteView::ReceiveInputEvent(const EmbedTouchInput &aEvent)
{
  LOGT();
  NS_ENSURE_TRUE(mViewImpl,);

  mozilla::MultiTouchInput multiTouchInput(static_cast<mozilla::MultiTouchInput::MultiTouchType>(aEvent.type),
                                           aEvent.timeStamp, TimeStamp::Now(), 0);

  for (const mozilla::embedlite::TouchData &touchData : aEvent.touches) {
    nsIntPoint point = nsIntPoint(int32_t(floorf(touchData.touchPoint.x)),
                                  int32_t(floorf(touchData.touchPoint.y)));

    mozilla::ScreenIntPoint screenPoint = mozilla::ScreenIntPoint::FromUnknownPoint(point);

    multiTouchInput.mTouches.AppendElement(mozilla::SingleTouchData(touchData.identifier,
                                                                    screenPoint,
                                                                    mozilla::ScreenSize(1, 1),
                                                                    180.0f,
                                                                    touchData.pressure));
  }

  mViewImpl->ReceiveInputEvent(multiTouchInput);
}

void
EmbedLiteView::SendTextEvent(const char *composite, const char *preEdit, int replacementStart, int replacementLength)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->TextEvent(composite, preEdit, replacementStart, replacementLength);
}

void EmbedLiteView::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->SendKeyPress(domKeyCode, gmodifiers, charCode);
}

void EmbedLiteView::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->SendKeyRelease(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteView::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MousePress(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MouseRelease(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MouseMove(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::PinchStart(int x, int y)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

void
EmbedLiteView::PinchUpdate(int x, int y, float scale)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

void
EmbedLiteView::PinchEnd(int x, int y, float scale)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

uint32_t
EmbedLiteView::GetUniqueID()
{
  NS_ENSURE_TRUE(mViewImpl, 0);
  uint32_t id;
  mViewImpl->GetUniqueID(&id);
  MOZ_ASSERT(id == mUniqueID);
  return mUniqueID;
}

} // namespace embedlite
} // namespace mozilla
