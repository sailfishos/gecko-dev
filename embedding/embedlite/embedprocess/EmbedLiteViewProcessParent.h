/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_PROCESS_PARENT_H
#define MOZ_VIEW_EMBED_PROCESS_PARENT_H


#include "mozilla/embedlite/PEmbedLiteViewParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteViewProcessParent : public PEmbedLiteViewParent,
                                   public EmbedLiteViewIface
{
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteViewProcessParent)
public:
    MOZ_IMPLICIT EmbedLiteViewProcessParent(const uint32_t& id, const uint32_t& parentId, const bool&);
    virtual ~EmbedLiteViewProcessParent();

    NS_DECL_EMBEDLITEVIEWIFACE

protected:
    virtual bool
    RecvInitialized();

    virtual bool
    RecvOnLocationChanged(
            const nsCString& aLocation,
            const bool& aCanGoBack,
            const bool& aCanGoForward);

    virtual bool
    RecvOnLoadStarted(const nsCString& aLocation);

    virtual bool
    RecvOnLoadFinished();

    virtual bool
    RecvOnLoadRedirect();

    virtual bool
    RecvOnLoadProgress(
            const int32_t& aProgress,
            const int32_t& aCurTotal,
            const int32_t& aMaxTotal);

    virtual bool
    RecvOnSecurityChanged(
            const nsCString& aStatus,
            const uint32_t& aState);

    virtual bool
    RecvOnFirstPaint(
            const int32_t& aX,
            const int32_t& aY);

    virtual bool
    RecvOnScrolledAreaChanged(
            const uint32_t& aWidth,
            const uint32_t& aHeight);

    virtual bool
    RecvOnScrollChanged(
            const int32_t& offSetX,
            const int32_t& offSetY);

    virtual bool
    RecvOnTitleChanged(const nsString& aTitle);

    virtual bool
    RecvOnWindowCloseRequested();

    virtual bool
    RecvUpdateZoomConstraints(
            const uint32_t& aPresShellId,
            const ViewID& aViewId,
            const bool& aIsRoot,
            const ZoomConstraints& aConstraints);

    virtual bool
    RecvZoomToRect(
            const uint32_t& aPresShellId,
            const ViewID& aViewId,
            const CSSRect& aRect);

    virtual bool
    RecvSetBackgroundColor(const nscolor& color);

    virtual bool
    RecvContentReceivedTouch(
            const ScrollableLayerGuid& aGuid,
            const uint64_t& aInputBlockId,
            const bool& aPreventDefault);

    virtual bool
    RecvGetGLViewSize(gfxSize* aSize);

    virtual bool
    RecvSyncMessage(
            const nsString& aMessage,
            const nsString& aJSON,
            nsTArray<nsString>* retval);

    virtual bool
    RecvRpcMessage(
            const nsString& aMessage,
            const nsString& aJSON,
            nsTArray<nsString>* retval);

    virtual bool
    RecvGetInputContext(
            int32_t* IMEEnabled,
            int32_t* IMEOpen,
            intptr_t* NativeIMEContext);

    virtual bool
    RecvSetInputContext(
            const int32_t& IMEEnabled,
            const int32_t& IMEOpen,
            const nsString& type,
            const nsString& inputmode,
            const nsString& actionHint,
            const int32_t& cause,
            const int32_t& focusChange);

    virtual bool
    RecvAsyncMessage(
            const nsString& aMessage,
            const nsString& aData);

    virtual void
    ActorDestroy(ActorDestroyReason aWhy);

private:
  EmbedLiteView* mView;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewProcessParent);
};
} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_PROCESS_PARENT_H

