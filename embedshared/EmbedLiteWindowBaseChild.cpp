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

bool EmbedLiteWindowBaseChild::RecvSetSize(const gfxSize& size)
{
  LOGT("this:%p", this);
  mSize = size;
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

  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.mWindowType = eWindowType_toplevel;
  widgetInit.mRequireOffMainThreadCompositing = true;
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(mSize.width, mSize.height)),
    &widgetInit              // HandleWidgetEvent
  );

  unused << SendInitialized();
}

} // namespace embedlite
} // namespace mozilla
