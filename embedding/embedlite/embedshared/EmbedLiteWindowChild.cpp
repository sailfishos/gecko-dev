/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include <math.h>

#include "nsWindow.h"
#include "EmbedLiteAppChild.h"
#include "EmbedLiteWindowChild.h"
#include "mozilla/Unused.h"
#include "Hal.h"
#include "gfxPlatform.h"
#include "mozilla/widget/ScreenManager.h"
#include "nsString.h"

using namespace mozilla::dom;

namespace mozilla {

namespace embedlite {

namespace {
static std::map<uint32_t, EmbedLiteWindowChild*> sWindowChildMap;

const char*
ScreenOrientationName(hal::ScreenOrientation aOrientation)
{
  switch (aOrientation) {
    case hal::ScreenOrientation::PortraitPrimary:
      return "portrait-primary";
    case hal::ScreenOrientation::LandscapePrimary:
      return "landscape-primary";
    case hal::ScreenOrientation::PortraitSecondary:
      return "portrait-secondary";
    case hal::ScreenOrientation::LandscapeSecondary:
      return "landscape-secondary";
    default:
      return nullptr;
  }
}

void
SendContentOrientationChanged(uint32_t aWindowID, hal::ScreenOrientation aOrientation)
{
  const char* orientationName = ScreenOrientationName(aOrientation);
  if (!orientationName) {
    return;
  }

  EmbedLiteAppChild* app = EmbedLiteAppChild::GetInstance();
  if (!app) {
    return;
  }

  nsAutoCString json;
  json.AssignLiteral("{\"orientation\":\"");
  json.Append(orientationName);
  json.AppendLiteral("\"}");
  const NS_ConvertUTF8toUTF16 message(json);

  app->SendAsyncMessageToViewsForWindowID(aWindowID,
                                          u"embed:contentOrientationChanged",
                                          message.get());
}
} // namespace

EmbedLiteWindowChild::EmbedLiteWindowChild(const uint16_t &width, const uint16_t &height, const uint32_t &aId, EmbedLiteWindowListener *aListener)
  : mId(aId)
  , mListener(aListener)
  , mWidget(nullptr)
  , mBounds(0, 0, width, height)
  , mRotation(ROTATION_0)
  , mInitialized(false)
  , mDestroyAfterInit(false)
  , mDepth(32)
  , mDensity(1.0)
  , mDpi(96)
{
  MOZ_ASSERT(sWindowChildMap.find(aId) == sWindowChildMap.end());
  MOZ_ASSERT(mListener);
  sWindowChildMap[aId] = this;

  MOZ_COUNT_CTOR(EmbedLiteWindowChild);

  mCreateWidgetTask = NewCancelableRunnableMethod("EmbedLiteWindowChild::CreateWidget",
                                                  this,
                                                  &EmbedLiteWindowChild::CreateWidget);
  MessageLoop::current()->PostTask(mCreateWidgetTask.forget());

  // Make sure gfx platform is initialized and ready to go.
  gfxPlatform::GetPlatform();
}

EmbedLiteWindowChild *EmbedLiteWindowChild::From(const uint32_t id)
{
  std::map<uint32_t, EmbedLiteWindowChild*>::const_iterator it = sWindowChildMap.find(id);
  if (it != sWindowChildMap.end()) {
    return it->second;
  }
  return nullptr;
}

EmbedLiteWindowChild::~EmbedLiteWindowChild()
{
  MOZ_ASSERT(sWindowChildMap.find(mId) != sWindowChildMap.end());
  sWindowChildMap.erase(sWindowChildMap.find(mId));

  MOZ_COUNT_DTOR(EmbedLiteWindowChild);

  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }
}

nsWindow *EmbedLiteWindowChild::GetWidget() const
{
  return static_cast<nsWindow*>(mWidget.get());
}

void EmbedLiteWindowChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvDestroy()
{
  if (!mInitialized) {
    mDestroyAfterInit = true;
    return IPC_OK();
  }

  LOGT("destroy");
  if (mWidget) {
    mWidget->Destroy();
    mWidget = nullptr;
  }
  Unused << SendDestroyed();
  PEmbedLiteWindowChild::Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetSize(const gfxSize &aSize)
{
  mBounds = LayoutDeviceIntRect(0, 0, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  LOGT("this:%p width: %f, height: %f as int w: %d h: %d", this, aSize.width, aSize.height, (int)nearbyint(aSize.width), (int)nearbyint(aSize.height));
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetSize(aSize.width, aSize.height);
    widget->UpdateBounds(true);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult EmbedLiteWindowChild::RecvSetContentOrientation(const uint32_t &aRotation)
{
  LOGT("this:%p", this);
  mRotation = static_cast<mozilla::ScreenRotation>(aRotation);
  if (mWidget) {
    nsWindow *widget = GetWidget();
    widget->SetRotation(mRotation);
    widget->UpdateBounds(true);
  }

  int32_t colorDepth, pixelDepth;

  RefPtr<widget::Screen> screen =
      widget::ScreenManager::GetSingleton().GetPrimaryScreen();

  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);

  hal::ScreenOrientation orientation = hal::ScreenOrientation::Default;
  uint16_t angle = 0;
  switch (mRotation) {
    case ROTATION_0:
      angle = 0;
      orientation = hal::ScreenOrientation::PortraitPrimary;
      break;
    case ROTATION_90:
      angle = 90;
      orientation = hal::ScreenOrientation::LandscapePrimary;
      break;
    case ROTATION_180:
      angle = 180;
      orientation = hal::ScreenOrientation::PortraitSecondary;
      break;
    case ROTATION_270:
      angle = 270;
      orientation = hal::ScreenOrientation::LandscapeSecondary;
      break;
    default:
      break;
  }

  nsIntRect rect(mBounds.X(), mBounds.Y(), mBounds.Width(), mBounds.Height());

  RefreshScreen();
  SendContentOrientationChanged(mId, orientation);

  return IPC_OK();
}

void EmbedLiteWindowChild::CreateWidget()
{
  LOGT("this:%p", this);
  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }

  if (mDestroyAfterInit) {
    RecvDestroy();
    return;
  }

  mWidget = new nsWindow(this);
  GetWidget()->SetRotation(mRotation);

  widget::InitData widgetInit;
  widgetInit.mClipChildren = true;
  widgetInit.mWindowType = widget::WindowType::TopLevel;

  // nsWindow::CreateCompositor() reads back Size
  // when it creates the compositor.
  Unused << mWidget->Create(
              nullptr, 0,              // no parents
              mBounds,
              &widgetInit              // HandleWidgetEvent
              );
  GetWidget()->UpdateBounds(true);

  // Initialize ScreenManager
  RefreshScreen();

  mInitialized = true;
  Unused << SendInitialized();
}

void EmbedLiteWindowChild::RefreshScreen()
{
  LayoutDeviceIntRect rect;
  if (mRotation == ROTATION_0 || mRotation == ROTATION_180)
    rect = mBounds;
  else
    rect = LayoutDeviceIntRect(0, 0, mBounds.Height(), mBounds.Width());

  const float density = GetDensity();
  AutoTArray<RefPtr<widget::Screen>, 1> screenList;
  auto screen = MakeRefPtr<widget::Screen>(
      rect, rect, mDepth, mDepth, 0, DesktopToLayoutDeviceScale(density),
      CSSToLayoutDeviceScale(density), mDpi,
      widget::Screen::IsPseudoDisplay::No);
  screenList.AppendElement(screen.forget());
  widget::ScreenManager::Refresh(std::move(screenList));
}

void EmbedLiteWindowChild::SetScreenProperties(const int &depth, const float &density, const float &dpi)
{
  bool refresh = false;
  float normalizedDensity = density > 0.0f ? density : 1.0f;

  if (depth != mDepth) {
    mDepth = depth;
    refresh = true;
  }

  if (normalizedDensity != mDensity) {
    mDensity = normalizedDensity;
    refresh = true;
  }

  if (dpi != mDpi) {
    mDpi = dpi;
    refresh = true;
  }

  if (refresh) {
    RefreshScreen();
    if (mWidget) {
      GetWidget()->BackingScaleFactorChanged();
    }
  }
}

} // namespace embedlite
} // namespace mozilla
