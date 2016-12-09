/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLitePuppetWidget.h"
#include "EmbedLiteWindowBaseChild.h"
#include "mozilla/unused.h"
#include "Hal.h"
#include "ScreenOrientation.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "gfxPlatform.h"

using namespace mozilla::dom;

namespace mozilla {

namespace layers {
void ShutdownTileCache();
}
namespace embedlite {

static int sWindowCount = 0;

EmbedLiteWindowBaseChild::EmbedLiteWindowBaseChild(const uint16_t& width, const uint16_t& height, const uint32_t& aId)
  : mId(aId)
  , mWidget(nullptr)
  , mBounds(0, 0, width, height)
  , mRotation(ROTATION_0)
{
  MOZ_COUNT_CTOR(EmbedLiteWindowBaseChild);

  mCreateWidgetTask = NewRunnableMethod(this, &EmbedLiteWindowBaseChild::CreateWidget);
  MessageLoop::current()->PostTask(FROM_HERE, mCreateWidgetTask);
  sWindowCount++;

  // Make sure gfx platform is initialized and ready to go.
  gfxPlatform::GetPlatform();
}

EmbedLiteWindowBaseChild::~EmbedLiteWindowBaseChild()
{
  MOZ_COUNT_DTOR(EmbedLiteWindowBaseChild);

  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }

  sWindowCount--;
  if (sWindowCount == 0) {
    mozilla::layers::ShutdownTileCache();
  }
}

EmbedLitePuppetWidget* EmbedLiteWindowBaseChild::GetWidget() const
{
  return static_cast<EmbedLitePuppetWidget*>(mWidget.get());
}

void EmbedLiteWindowBaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
}

bool EmbedLiteWindowBaseChild::RecvDestroy()
{
  LOGT("destroy");
  mWidget = nullptr;
  Unused << SendDestroyed();
  PEmbedLiteWindowChild::Send__delete__(this);
  return true;
}

bool EmbedLiteWindowBaseChild::RecvSetSize(const gfxSize& aSize)
{
  mBounds = LayoutDeviceIntRect(0, 0, aSize.width, aSize.height);
  LOGT("this:%p width: %f, height: %f", this, aSize.width, aSize.height);
  if (mWidget) {
    mWidget->Resize(aSize.width, aSize.height, true);
  }
  return true;
}

bool EmbedLiteWindowBaseChild::RecvSetContentOrientation(const uint32_t &aRotation)
{
  LOGT("this:%p", this);
  mRotation = static_cast<mozilla::ScreenRotation>(aRotation);
  if (mWidget) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetRotation(mRotation);
    widget->UpdateSize();
  }

  nsresult rv;
  nsCOMPtr<nsIScreenManager> screenMgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Can't find nsIScreenManager!");
    return true;
  }

  nsIntRect rect;
  int32_t colorDepth, pixelDepth;
  nsCOMPtr<nsIScreen> screen;

  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  screen->GetRect(&rect.x, &rect.y, &rect.width, &rect.height);
  screen->GetColorDepth(&colorDepth);
  screen->GetPixelDepth(&pixelDepth);

  mozilla::dom::ScreenOrientationInternal orientation = eScreenOrientation_Default;
  uint16_t angle = 0;
  switch (mRotation) {
    case ROTATION_0:
      angle = 0;
      orientation = mozilla::dom::eScreenOrientation_PortraitPrimary;
      break;
    case ROTATION_90:
      angle = 90;
      orientation = mozilla::dom::eScreenOrientation_LandscapePrimary;
      break;
    case ROTATION_180:
      angle = 180;
      orientation = mozilla::dom::eScreenOrientation_PortraitSecondary;
      break;
    case ROTATION_270:
      angle = 270;
      orientation = mozilla::dom::eScreenOrientation_LandscapeSecondary;
      break;
    default:
      break;
  }

  hal::NotifyScreenConfigurationChange(hal::ScreenConfiguration(
      rect, orientation, angle, colorDepth, pixelDepth));

  return true;
}

void EmbedLiteWindowBaseChild::CreateWidget()
{
  LOGT("this:%p", this);
  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
  }

  mWidget = new EmbedLitePuppetWidget(this);
  static_cast<EmbedLitePuppetWidget*>(mWidget.get())->SetRotation(mRotation);

  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.mWindowType = eWindowType_toplevel;
  widgetInit.mRequireOffMainThreadCompositing = true;

  // EmbedLitePuppetWidget::CreateCompositor() reads back Size
  // when it creates the compositor.
  mWidget->Create(
    nullptr, 0,              // no parents
    mBounds,
    &widgetInit              // HandleWidgetEvent
  );
  static_cast<EmbedLitePuppetWidget*>(mWidget.get())->UpdateSize();

  Unused << SendInitialized();
}

} // namespace embedlite
} // namespace mozilla
