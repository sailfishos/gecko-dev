/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLitePuppetWidget.h"
#include "EmbedLiteWindowBaseChild.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace embedlite {

EmbedLiteWindowBaseChild::EmbedLiteWindowBaseChild(const uint32_t& aId)
  : mId(aId)
  , mWidget(nullptr)
  , mSize(0, 0)
  , mRotation(ROTATION_0)
{
  MOZ_COUNT_CTOR(EmbedLiteWindowBaseChild);

  mCreateWidgetTask = NewRunnableMethod(this, &EmbedLiteWindowBaseChild::CreateWidget);
  MessageLoop::current()->PostTask(FROM_HERE, mCreateWidgetTask);
}

EmbedLiteWindowBaseChild::~EmbedLiteWindowBaseChild()
{
  MOZ_COUNT_DTOR(EmbedLiteWindowBaseChild);

  if (mCreateWidgetTask) {
    mCreateWidgetTask->Cancel();
    mCreateWidgetTask = nullptr;
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
  PEmbedLiteWindowChild::Send__delete__(this);
  return true;
}

bool EmbedLiteWindowBaseChild::RecvSetSize(const gfxSize& aSize)
{
  LOGT("this:%p", this);
  mSize = aSize;
  if (mWidget) {
    mWidget->Resize(aSize.width, aSize.height, true);
  }
  return true;
}

bool EmbedLiteWindowBaseChild::RecvSetContentOrientation(const mozilla::ScreenRotation& aRotation)
{
  LOGT("this:%p", this);
  mRotation = aRotation;
  if (mWidget) {
    EmbedLitePuppetWidget* widget = static_cast<EmbedLitePuppetWidget*>(mWidget.get());
    widget->SetRotation(aRotation);
    widget->UpdateSize();
  }
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
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(mSize.width, mSize.height)),
    &widgetInit              // HandleWidgetEvent
  );
  static_cast<EmbedLitePuppetWidget*>(mWidget.get())->UpdateSize();

  unused << SendInitialized();
}

} // namespace embedlite
} // namespace mozilla
