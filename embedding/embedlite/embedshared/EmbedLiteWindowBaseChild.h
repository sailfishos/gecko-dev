/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_BASE_CHILD_H
#define MOZ_WINDOW_EMBED_BASE_CHILD_H

#include "mozilla/embedlite/PEmbedLiteWindowChild.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "base/task.h" // for CancelableRunnable

namespace mozilla {
namespace embedlite {

class nsWindow;
class EmbedLiteWindowListener;

class EmbedLiteWindowBaseChild : public PEmbedLiteWindowChild
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteWindowBaseChild)

public:
  EmbedLiteWindowBaseChild(const uint16_t &width, const uint16_t &height, const uint32_t &id, EmbedLiteWindowListener *aListener);

  static EmbedLiteWindowBaseChild *From(const uint32_t id);

  uint32_t GetUniqueID() const { return mId; }
  nsWindow *GetWidget() const;
  LayoutDeviceIntRect GetSize() const { return mBounds; }
  EmbedLiteWindowListener* GetListener() const { return mListener; }

protected:
  virtual ~EmbedLiteWindowBaseChild() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult RecvDestroy() override;
  virtual mozilla::ipc::IPCResult RecvSetSize(const gfxSize &size) override;
  virtual mozilla::ipc::IPCResult RecvSetContentOrientation(const uint32_t &) override;

private:
  void CreateWidget();

  uint32_t mId;
  EmbedLiteWindowListener *const mListener;
  nsCOMPtr<nsIWidget> mWidget;
  LayoutDeviceIntRect mBounds;
  mozilla::ScreenRotation mRotation;
  RefPtr<CancelableRunnable> mCreateWidgetTask;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowBaseChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_BASE_CHILD_H
