/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindow.h"

#include "EmbedLiteChromeInputSession.h"
#include "EmbedLiteChromeSession.h"
#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteChromeTabSession.h"
#include "mozilla/embedlite/PEmbedLiteWindowParent.h"
#include "EmbedLiteWindowParent.h"
#include "mozilla/Unused.h"

#include <set>

namespace mozilla {
namespace embedlite {

namespace {

std::set<const EmbedLiteWindow*> sChromeHostedWindows;

} // namespace

EmbedLiteWindow::EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent* parent, uint32_t id)
  : EmbedLiteWindow(app, parent, id, false)
{
}

EmbedLiteWindow::EmbedLiteWindow(EmbedLiteApp* app,
                                 PEmbedLiteWindowParent* parent,
                                 uint32_t id,
                                 bool chromeHosted)
  : mApp(app)
  , mWindowParent(static_cast<EmbedLiteWindowParent*>(parent))
  , mUniqueID(id)
{
  MOZ_COUNT_CTOR(EmbedLiteWindow);
  if (chromeHosted) {
    sChromeHostedWindows.insert(this);
  }
  mWindowParent->SetEmbedAPIWindow(this);
}

EmbedLiteWindow::~EmbedLiteWindow()
{
  sChromeHostedWindows.erase(this);
  MOZ_COUNT_DTOR(EmbedLiteWindow);
  mWindowParent->SetEmbedAPIWindow(nullptr);
}

void EmbedLiteWindow::Destroy()
{
  Unused << mWindowParent->SendDestroy();
}

void EmbedLiteWindow::Destroyed()
{
  if (mWindowParent) {
    mWindowParent->GetListener()->WindowDestroyed();
  }
  EmbedLiteApp::GetInstance()->WindowDestroyed(mUniqueID);
}

void EmbedLiteWindow::SetSize(int width, int height)
{
  mWindowParent->SetSize(width, height);
}

uint32_t EmbedLiteWindow::GetUniqueID() const
{
  return mUniqueID;
}

bool EmbedLiteWindow::IsChromeHosted() const
{
  return sChromeHostedWindows.find(this) != sChromeHostedWindows.end();
}

EmbedLiteChromeSession* EmbedLiteWindow::GetChromeSession()
{
  return IsChromeHosted() ? mWindowParent : nullptr;
}

EmbedLiteChromeInputSession* EmbedLiteWindow::GetChromeInputSession()
{
  return IsChromeHosted() ? mWindowParent : nullptr;
}

EmbedLiteChromeContentSession* EmbedLiteWindow::GetChromeContentSession()
{
  return IsChromeHosted() ? mWindowParent : nullptr;
}

EmbedLiteChromeTabSession* EmbedLiteWindow::GetChromeTabSession()
{
  return IsChromeHosted() ? mWindowParent : nullptr;
}

void EmbedLiteWindow::SetContentOrientation(mozilla::embedlite::ScreenRotation rotation)
{
  mWindowParent->SetContentOrientation(rotation);
}

void EmbedLiteWindow::ScheduleUpdate()
{
  mWindowParent->ScheduleUpdate();
}

void EmbedLiteWindow::SuspendRendering()
{
  mWindowParent->SuspendRendering();
}

void EmbedLiteWindow::ResumeRendering()
{
  mWindowParent->ResumeRendering();
}

bool EmbedLiteWindow::WithPlatformImage(const PlatformImageCallback& callback)
{
  return mWindowParent->WithPlatformImage(callback);
}

void EmbedLiteWindow::ClearPlatformImage()
{
  mWindowParent->ClearPlatformImage();
}

bool EmbedLiteWindow::AcquirePlatformFrame(
    const PlatformFrameToken& token,
    const PlatformFrameCallback& callback)
{
  return mWindowParent->AcquirePlatformFrame(token, callback);
}

bool EmbedLiteWindow::ReleasePlatformFrame(
    const PlatformFrameRelease& release)
{
  return mWindowParent->ReleasePlatformFrame(release);
}

bool EmbedLiteWindow::SetPlatformFrameDeliveryEnabled(bool enabled)
{
  return mWindowParent->SetPlatformFrameDeliveryEnabled(enabled);
}

bool EmbedLiteWindow::SetPlatformFrameListener(
    EmbedLitePlatformFrameListener* listener)
{
  return mWindowParent->SetPlatformFrameListener(listener);
}

} // nemsapace embedlite
} // namespace mozilla
