/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindow.h"

#include "EmbedLiteChromeInputSession.h"
#include "EmbedLiteChromeSession.h"
#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteChromeTabSession.h"
#include "EmbedLiteWindowParent.h"

namespace mozilla {
namespace embedlite {

EmbedLiteWindow::EmbedLiteWindow(EmbedLiteApp* app,
                                 EmbedLiteWindowParent* parent,
                                 uint32_t id)
  : mApp(app)
  , mWindowParent(parent)
  , mUniqueID(id)
{
  MOZ_COUNT_CTOR(EmbedLiteWindow);
  mWindowParent->SetEmbedAPIWindow(this);
}

EmbedLiteWindow::~EmbedLiteWindow()
{
  MOZ_COUNT_DTOR(EmbedLiteWindow);
  mWindowParent->SetEmbedAPIWindow(nullptr);
}

void EmbedLiteWindow::Destroy()
{
  mWindowParent->Destroy();
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
  return true;
}

EmbedLiteChromeSession* EmbedLiteWindow::GetChromeSession()
{
  return mWindowParent;
}

EmbedLiteChromeInputSession* EmbedLiteWindow::GetChromeInputSession()
{
  return mWindowParent;
}

EmbedLiteChromeContentSession* EmbedLiteWindow::GetChromeContentSession()
{
  return mWindowParent;
}

EmbedLiteChromeTabSession* EmbedLiteWindow::GetChromeTabSession()
{
  return mWindowParent;
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
