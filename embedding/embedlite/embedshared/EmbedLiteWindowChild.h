/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_CHILD_H
#define MOZ_WINDOW_EMBED_CHILD_H

#include "mozilla/embedlite/PEmbedLiteWindowChild.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "base/task.h" // for CancelableRunnable

namespace mozilla {
namespace embedlite {

class nsWindow;

class EmbedLiteWindowChild : public PEmbedLiteWindowChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteWindowChild)

public:
  EmbedLiteWindowChild(const uint16_t& width, const uint16_t& height, const uint32_t& id);

  static EmbedLiteWindowChild *From(const uint32_t id);

  uint32_t GetUniqueID() const { return mId; }
  nsWindow *GetWidget() const;
  LayoutDeviceIntRect GetSize() const { return mBounds; }

protected:
  virtual ~EmbedLiteWindowChild() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  friend class PEmbedLiteWindowChild;
  void CreateWidget();

  mozilla::ipc::IPCResult RecvDestroy();
  mozilla::ipc::IPCResult RecvSetSize(const gfxSize &size);
  mozilla::ipc::IPCResult RecvSetContentOrientation(const uint32_t &);

  uint32_t mId;
  nsCOMPtr<nsIWidget> mWidget;
  LayoutDeviceIntRect mBounds;
  mozilla::ScreenRotation mRotation;
  RefPtr<CancelableRunnable> mCreateWidgetTask;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_CHILD_H
