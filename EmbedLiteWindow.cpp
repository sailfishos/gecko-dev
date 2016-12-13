/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteWindow.h"

#include "mozilla/embedlite/PEmbedLiteWindowParent.h"
#include "EmbedLiteWindowBaseParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace embedlite {

namespace {

class FakeWindowListener : public EmbedLiteWindowListener {};
static FakeWindowListener sFakeWindowListener;

} // namespace

EmbedLiteWindow::EmbedLiteWindow(EmbedLiteApp* app, PEmbedLiteWindowParent* parent, uint32_t id)
  : mApp(app)
  , mListener(nullptr)
  , mWindowParent(static_cast<EmbedLiteWindowBaseParent*>(parent))
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
  Unused << mWindowParent->SendDestroy();
}

void EmbedLiteWindow::Destroyed()
{
  if (mListener) {
    mListener->WindowDestroyed();
  }
  EmbedLiteApp::GetInstance()->WindowDestroyed(mUniqueID);
}

void EmbedLiteWindow::SetListener(EmbedLiteWindowListener* aListener)
{
  mListener = aListener;
}

EmbedLiteWindowListener* const EmbedLiteWindow::GetListener() const
{
  return mListener ? mListener : &sFakeWindowListener;
}

void EmbedLiteWindow::SetSize(int width, int height)
{
  Unused << mWindowParent->SendSetSize(gfxSize(width, height));
}

uint32_t EmbedLiteWindow::GetUniqueID() const
{
  return mUniqueID;
}

void EmbedLiteWindow::SetContentOrientation(mozilla::embedlite::ScreenRotation rotation)
{
  Unused << mWindowParent->SendSetContentOrientation(rotation);
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

void* EmbedLiteWindow::GetPlatformImage(int* width, int* height)
{
  return mWindowParent->GetPlatformImage(width, height);
}

} // nemsapace embedlite
} // namespace mozilla

