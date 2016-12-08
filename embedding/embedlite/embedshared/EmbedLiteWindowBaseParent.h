/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_WINDOW_EMBED_BASE_PARENT_H
#define MOZ_WINDOW_EMBED_BASE_PARENT_H

#include "mozilla/embedlite/PEmbedLiteWindowParent.h"
#include "mozilla/WidgetUtils.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteWindow;
class EmbedLiteCompositorParent;

class EmbedLiteWindowParentObserver
{
public:
  virtual void CompositorCreated() = 0;
};

class EmbedLiteWindowBaseParent : public PEmbedLiteWindowParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteWindowBaseParent)
public:
  EmbedLiteWindowBaseParent(const uint16_t& width, const uint16_t& height, const uint32_t& id);

  static EmbedLiteWindowBaseParent* From(const uint32_t id);

  void AddObserver(EmbedLiteWindowParentObserver*);
  void RemoveObserver(EmbedLiteWindowParentObserver*);

  EmbedLiteCompositorParent* GetCompositor() const { return mCompositor.get(); }

  void SetSize(int width, int height);
  void SetContentOrientation(const uint32_t &);
  bool ScheduleUpdate();
  void SuspendRendering();
  void ResumeRendering();
  void* GetPlatformImage(int* width, int* height);

protected:
  friend class EmbedLiteCompositorParent;
  friend class EmbedLiteWindow;

  virtual ~EmbedLiteWindowBaseParent() override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvInitialized() override;
  virtual bool RecvDestroyed() override;

  void SetEmbedAPIWindow(EmbedLiteWindow* window);
  void SetCompositor(EmbedLiteCompositorParent* aCompositor);

private:
  typedef nsTArray<EmbedLiteWindowParentObserver*> ObserverArray;

  uint32_t mId;
  EmbedLiteWindow* mWindow;
  ObserverArray mObservers;
  RefPtr<EmbedLiteCompositorParent> mCompositor;

  gfxSize mSize;
  mozilla::ScreenRotation mRotation;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteWindowBaseParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_WINDOW_EMBED_BASE_PARENT_H

