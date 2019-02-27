/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_BASE_CHILD_H
#define MOZ_WINDOW_EMBED_BASE_CHILD_H

#include "mozilla/embedlite/PEmbedLiteWindowChild.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteWindowBaseChild : public PEmbedLiteWindowChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteWindowBaseChild)

public:
  EmbedLiteWindowBaseChild(const uint16_t& width, const uint16_t& height, const uint32_t& id);

  uint32_t GetUniqueID() const { return mId; }
  EmbedLitePuppetWidget* GetWidget() const;
  LayoutDeviceIntRect GetSize() const { return mBounds; }

protected:
  virtual ~EmbedLiteWindowBaseChild() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool RecvDestroy() override;
  virtual bool RecvSetSize(const gfxSize& size) override;
  virtual bool RecvSetContentOrientation(const uint32_t &) override;

private:
  void CreateWidget();

  uint32_t mId;
  nsCOMPtr<nsIWidget> mWidget;
  LayoutDeviceIntRect mBounds;
  mozilla::ScreenRotation mRotation;
  CancelableTask* mCreateWidgetTask;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowBaseChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_BASE_CHILD_H
