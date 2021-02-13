/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "PuppetWidgetBase.h"

#include "mozilla/Unused.h"

#include "Layers.h"                // for LayerManager
#include "ClientLayerManager.h"    // for ClientLayerManager

using namespace mozilla::layers;

namespace mozilla {
namespace embedlite {

// Arbitrary, fungible.
const size_t PuppetWidgetBase::kMaxDimension = 4000;

static nsTArray<PuppetWidgetBase*> gTopLevelWindows;

NS_IMPL_ISUPPORTS_INHERITED(PuppetWidgetBase,
                            nsBaseWidget,
                            nsISupportsWeakReference)

PuppetWidgetBase::PuppetWidgetBase()
  : nsBaseWidget()
  , mVisible(false)
  , mEnabled(false)
  , mActive(false)
  , mParent(nullptr)
  , mRotation(mozilla::ROTATION_0)
  , mMargins(0, 0, 0, 0)

{

}

nsresult
PuppetWidgetBase::Create(nsIWidget *aParent, nsNativeWidget aNativeParent, const LayoutDeviceIntRect &aRect, nsWidgetInitData *aInitData)
{
  LOGT("Puppet: %p, parent: %p", this, aParent);

  NS_ASSERTION(!aNativeParent, "got a non-Puppet native parent");

  mParent = static_cast<PuppetWidgetBase*>(aParent);

  mEnabled = true;
  mVisible = mParent ? mParent->mVisible : true;

  if (mParent) {
    mParent->mChildren.AppendElement(this);
  }
  mRotation = mParent ? mParent->mRotation : mRotation;
  mBounds = mParent ? mParent->mBounds : aRect;
  mMargins = mParent ? mParent->mMargins : mMargins;
  mNaturalBounds = mParent ? mParent->mNaturalBounds : aRect;

  BaseCreate(aParent, aInitData);

  if (IsTopLevel()) {
    LOGT("Append this to toplevel windows:%p", this);
    gTopLevelWindows.AppendElement(this);
  }

  return NS_OK;
}

void
PuppetWidgetBase::Destroy()
{
  LOGT();
  if (mOnDestroyCalled) {
    return;
  }

  mOnDestroyCalled = true;
  mLayerManager = nullptr;

  Base::OnDestroy();
  Base::Destroy();

  while (mChildren.Length()) {
    mChildren[0]->SetParent(nullptr);
  }
  mChildren.Clear();

  if (mParent) {
    mParent->mChildren.RemoveElement(this);
  }

  mParent = nullptr;

#if DEBUG
  DumpWidgetTree();
#endif
}

void
PuppetWidgetBase::Show(bool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  if (Destroyed() || !WillShow(aState)) {
    return;
  }

  LOGT("this:%p, state: %i, LM:%p", this, aState, mLayerManager.get());

  bool wasVisible = mVisible;
  mVisible = aState;

  nsIWidget* topWidget = GetTopLevelWidget();
  if (!mVisible && mLayerManager && topWidget == this) {
    mLayerManager->ClearCachedResources();
  }

  if (Destroyed()) {
    return;
  }

  if (!wasVisible && mVisible) {
    Resize(mNaturalBounds.width, mNaturalBounds.height, false);
    Invalidate(mBounds);
  }

#if DEBUG
    // No point for dumping the tree for both show and hide calls.
    if (aState) {
      DumpWidgetTree();
    }
#endif
}

bool
PuppetWidgetBase::IsVisible() const
{
  return mVisible;
}

void
PuppetWidgetBase::ConstrainPosition(bool, int32_t *aX, int32_t *aY)
{
  *aX = kMaxDimension;
  *aY = kMaxDimension;
  LOGNI();
}

// We're always at <0, 0>, and so ignore move requests.
void
PuppetWidgetBase::Move(double aX, double aY)
{
  (void)aX;
  (void)aY;

  LOGNI();
}

void
PuppetWidgetBase::Resize(double aWidth, double aHeight, bool aRepaint)
{
  if (Destroyed()) {
    return;
  }

  LayoutDeviceIntRect oldBounds = mBounds;
  LOGT("sz[%i,%i]->[%g,%g]", oldBounds.width, oldBounds.height, aWidth, aHeight);

  mNaturalBounds.SizeTo(NSToIntRound(aWidth), NSToIntRound(aHeight));
  if (mRotation == mozilla::ROTATION_0 || mRotation == mozilla::ROTATION_180) {
    mBounds.SizeTo(NSToIntRound(aWidth), NSToIntRound(aHeight));
  } else {
    mBounds.SizeTo(NSToIntRound(aHeight), NSToIntRound(aWidth));
  }

  // Do not move bounds here. Just alter size based on margins.
  mBounds.y = 0;
  mBounds.x = 0;
  mBounds.width = std::max(0, (mBounds.width - std::max(0, mMargins.left) - std::max(0, mMargins.right)));
  mBounds.height = std::max(0, (mBounds.height - std::max(0, mMargins.top) - std::max(0, mMargins.bottom)));

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->WidgetBoundsChanged(mBounds);
  }

  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->Resize(aWidth, aHeight, aRepaint);
  }

  if (aRepaint) {
    Invalidate(mBounds);
  }

  nsIWidgetListener* listener =
    mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
  if (!oldBounds.IsEqualEdges(mBounds) && listener) {
    listener->WindowResized(this, mBounds.width, mBounds.height);
  }
}

void
PuppetWidgetBase::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint)
{
  (void)aX;
  (void)aY;
  Resize(aWidth, aHeight, aRepaint);
}

void
PuppetWidgetBase::Enable(bool aState)
{
  LOGNI();
  mEnabled = aState;
}

bool
PuppetWidgetBase::IsEnabled() const
{
  LOGNI();
  return mEnabled;
}

void
PuppetWidgetBase::SetFocus(Raise aRaise, mozilla::dom::CallerType aCallerType)
{
  Unused << aRaise;
  Unused << aCallerType;
  LOGT();
}

nsresult
PuppetWidgetBase::SetTitle(const nsAString &aTitle)
{
  LOGNI();
  return NS_ERROR_UNEXPECTED;
}

// PuppetWidgets don't care about children.
nsresult
PuppetWidgetBase::ConfigureChildren(const nsTArray<Configuration> &aConfigurations)
{
  Unused << aConfigurations;
  LOGNI();
  return NS_OK;
}

// PuppetWidgets are always at <0, 0>.
mozilla::LayoutDeviceIntPoint
PuppetWidgetBase::WidgetToScreenOffset()
{
  LOGT();
  return LayoutDeviceIntPoint(0, 0);
}

void
PuppetWidgetBase::Invalidate(const LayoutDeviceIntRect &aRect)
{
  Unused << aRect;

  if (Destroyed()) {
    return;
  }

  nsIWidgetListener* listener = GetWidgetListener();
  if (listener) {
    listener->WillPaintWindow(this);
  }

  LayerManager* lm = nsIWidget::GetLayerManager();
  if (mozilla::layers::LayersBackend::LAYERS_CLIENT == lm->GetBackendType()) {
    // No need to do anything, the compositor will handle drawing
  } else {
    MOZ_CRASH("Unexpected layer manager type");
  }

  listener = GetWidgetListener();
  if (listener) {
    listener->DidPaintWindow();
  }
}

void
PuppetWidgetBase::SetParent(nsIWidget *aNewParent)
{
  LOGT();
  if (mParent == static_cast<PuppetWidgetBase*>(aNewParent)) {
    return;
  }

  if (mParent) {
    mParent->mChildren.RemoveElement(this);
  }

  mParent = static_cast<PuppetWidgetBase*>(aNewParent);

  if (mParent) {
    mParent->mChildren.AppendElement(this);
  }
}

nsIWidget*
PuppetWidgetBase::GetParent(void)
{
  return mParent;
}

void
PuppetWidgetBase::CaptureRollupEvents(nsIRollupListener *aListener, bool aDoCapture)
{
  (void)aListener;
  (void)aDoCapture;
  LOGNI();
}

void
PuppetWidgetBase::ReparentNativeWidget(nsIWidget *aNewParent)
{
  Unused << aNewParent;
  LOGNI();
}

void
PuppetWidgetBase::SetRotation(mozilla::ScreenRotation rotation)
{
  mRotation = rotation;

  for (ObserverArray::size_type i = 0; i < mObservers.Length(); ++i) {
    mObservers[i]->WidgetRotationChanged(mRotation);
  }

  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->SetRotation(rotation);
  }

#ifdef DEBUG
  if (IsTopLevel()) {
    DumpWidgetTree();
  }
#endif
}

void
PuppetWidgetBase::SetMargins(const LayoutDeviceIntMargin &margins)
{
  mMargins = margins;
  for (ChildrenArray::size_type i = 0; i < mChildren.Length(); i++) {
    mChildren[i]->SetMargins(margins);
  }
}

void
PuppetWidgetBase::UpdateSize()
{
  Resize(mNaturalBounds.width, mNaturalBounds.height, true);
#ifdef DEBUG
  DumpWidgetTree();
#endif
}

void
PuppetWidgetBase::SetActive(bool active)
{
  mActive = active;
}

LayerManager *
PuppetWidgetBase::GetLayerManager(PLayerTransactionChild *aShadowManager,
                                                LayersBackend aBackendHint,
                                                LayerManagerPersistence aPersistence)
{
  if (Destroyed()) {
    return nullptr;
  }

  if (mLayerManager) {
    // This layer manager might be used for painting outside of DoDraw(), so we need
    // to set the correct rotation on it.
    if (mLayerManager->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
      ClientLayerManager* manager =
          static_cast<ClientLayerManager*>(mLayerManager.get());
      manager->SetDefaultTargetConfiguration(mozilla::layers::BufferMode::BUFFER_NONE,
                                             mRotation);
    }
    return mLayerManager;
  }

  // Layer manager can be null here. Sub-class shall handle this.
  return mLayerManager;
}

void PuppetWidgetBase::DumpWidgetTree()
{
  printf_stderr("PuppetWidgetBase Tree:\n");
  DumpWidgetTree(gTopLevelWindows);
}

void PuppetWidgetBase::DumpWidgetTree(const nsTArray<PuppetWidgetBase *> &widgets, int indent)
{
  for (uint32_t i = 0; i < widgets.Length(); ++i) {
    PuppetWidgetBase *w = widgets[i];
    LogWidget(w, i, indent);
    DumpWidgetTree(w->mChildren, indent + 2);
  }
}

void PuppetWidgetBase::LogWidget(PuppetWidgetBase *widget, int index, int indent)
{
  char spaces[] = "                    ";
  spaces[indent < 20 ? indent : 20] = 0;
  printf_stderr("%s [% 2d] [%p = %s]  size: [(%d, %d), (%3d, %3d)], margins: [%d, %d, %d, %d], "
                "visible: %d, type: %d, rotation: %d, observers: %zu\n",
                spaces, index, widget, widget->Type(),
                widget->mBounds.x, widget->mBounds.y,
                widget->mBounds.width, widget->mBounds.height,
                widget->mMargins.top, widget->mMargins.right,
                widget->mMargins.bottom, widget->mMargins.left,
                widget->mVisible, widget->mWindowType,
                widget->mRotation * 90, widget->mObservers.Length());
}

PuppetWidgetBase::~PuppetWidgetBase()
{
  LOGT("this: %p", this);

  if (IsTopLevel()) {
    gTopLevelWindows.RemoveElement(this);
  }
}

bool
PuppetWidgetBase::WillShow(bool aState)
{
  return mVisible != aState;
}

bool
PuppetWidgetBase::IsTopLevel()
{
  return mWindowType == eWindowType_toplevel ||
         mWindowType == eWindowType_dialog ||
         mWindowType == eWindowType_invisible;
}

}  // namespace embedlite
}  // namespace mozilla
