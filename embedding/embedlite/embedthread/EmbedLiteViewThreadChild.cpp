/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewThreadChild.h"
#include "EmbedLiteAppThreadChild.h"

#include "mozilla/unused.h"

#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"

#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
#include "gfxPrefs.h"

#include "APZCCallbackHelper.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

EmbedLiteViewThreadChild::EmbedLiteViewThreadChild(const uint32_t& aId, const uint32_t& parentId)
  : EmbedLiteViewBaseChild(aId, parentId)
  , mDispatchSynthMouseEvents(true)
  , mIMEComposing(false)
  , mPendingTouchPreventedBlockId(0)
{
  LOGT("id:%u, parentID:%u", aId, parentId);
}

EmbedLiteViewThreadChild::~EmbedLiteViewThreadChild()
{
  LOGT();
  NS_ASSERTION(mControllerListeners.IsEmpty(), "Controller listeners list is not empty...");
}

EmbedLiteAppThreadChild*
EmbedLiteViewThreadChild::AppChild()
{
  return EmbedLiteAppThreadChild::GetInstance();
}

void
EmbedLiteViewThreadChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
  mControllerListeners.Clear();
}

bool EmbedLiteViewThreadChild::RecvDestroy()
{
  LOGT("destroy");
  mControllerListeners.Clear();
  return true;
}

/*----------------------------WidgetIface-----------------------------------------------------*/

gfxSize
EmbedLiteViewThreadChild::GetGLViewSize()
{
  if (mGLViewSize.IsEmpty()) {
    SendGetGLViewSize(&mGLViewSize);
  }
  return mGLViewSize;
}

void EmbedLiteViewThreadChild::ResetInputState()
{
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

void
EmbedLiteViewThreadChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->RequestContentRepaint(aFrameMetrics);
  }
}

bool
EmbedLiteViewThreadChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }

  return false;
}

bool
EmbedLiteViewThreadChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewThreadChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

bool
EmbedLiteViewThreadChild::DoCallRpcMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    SendRpcMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewThreadChild::RecvAsyncMessage(const nsString& aMessage,
                                           const nsString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  EmbedLiteAppService::AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return true;
}


void
EmbedLiteViewThreadChild::RecvAsyncMessage(const nsAString& aMessage,
                                           const nsAString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
}

bool
EmbedLiteViewThreadChild::RecvAddMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvRemoveMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return true;
}

bool
EmbedLiteViewThreadChild::RecvAddMessageListeners(const InfallibleTArray<nsString>& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvRemoveMessageListeners(const InfallibleTArray<nsString>& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return true;
}

void
EmbedLiteViewThreadChild::AddGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.AppendElement(listener);
}

void
EmbedLiteViewThreadChild::RemoveGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.RemoveElement(listener);
}

bool
EmbedLiteViewThreadChild::RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                                  const gfxSize& scrollSize)
{
  mozilla::CSSRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  mozilla::CSSSize size(scrollSize.width, scrollSize.height);
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->SendAsyncScrollDOMEvent(0, rect, size);
  }

  if (sPostAZPCAsJson.scroll) {
    nsString data;
    data.AppendPrintf("{ \"contentRect\" : { \"x\" : ");
    data.AppendFloat(contentRect.x);
    data.AppendPrintf(", \"y\" : ");
    data.AppendFloat(contentRect.y);
    data.AppendPrintf(", \"width\" : ");
    data.AppendFloat(contentRect.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(contentRect.height);
    data.AppendPrintf("}, \"scrollSize\" : { \"width\" : ");
    data.AppendFloat(scrollSize.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(scrollSize.height);
    data.AppendPrintf(" }}");
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("AZPC:ScrollDOMEvent"), data);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleDoubleTap(const nsIntPoint& aPoint)
{
  if (!mWebBrowser) {
    return true;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleDoubleTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0));
  }

  if (sPostAZPCAsJson.doubleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);
  }

  return true;
}

void
EmbedLiteViewThreadChild::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (aPoint) {
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  } else {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }

  event.time = PR_Now() / 1000;
}

bool
EmbedLiteViewThreadChild::RecvHandleSingleTap(const nsIntPoint& aPoint)
{
  if (mIMEComposing) {
    // If we are in the middle of compositing we must finish it, before it is too late.
    // this way we can get focus and actual compositing node working properly in future composition
    nsPoint offset;
    nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
    mIMEComposing = false;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleSingleTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0));
  }

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:SingleTap"), data);
  }

  if (sHandleDefaultAZPC.singleTap) {
    RecvMouseEvent(NS_LITERAL_STRING("mousemove"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mousedown"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mouseup"), aPoint.x, aPoint.y, 0, 1, 0, false);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleLongTap(const nsIntPoint& aPoint, const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleLongTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0), aInputBlockId);
  }

  if (sPostAZPCAsJson.longTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:LongTap"), data);
  }

  bool eventHandled = false;
  if (sHandleDefaultAZPC.longTap) {
    eventHandled = RecvMouseEvent(NS_LITERAL_STRING("contextmenu"), aPoint.x, aPoint.y,
                   2 /* Right button */,
                   1 /* Click count */,
                   0 /* Modifiers */,
                   false /* Ignore root scroll frame */);
  }

  SendContentReceivedTouch(aGuid, aInputBlockId, eventHandled);

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleTextEvent(const nsString& commit, const nsString& preEdit)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  const InputContext& ctx = mWidget->GetInputContext();

  LOGF("ctx.mIMEState.mEnabled:%i, com:%s, pre:%s\n", ctx.mIMEState.mEnabled, NS_ConvertUTF16toUTF8(commit).get(), NS_ConvertUTF16toUTF8(preEdit).get());
  if (!widget || !ctx.mIMEState.mEnabled) {
    return false;
  }

  // probably logic here is over engineered, but clean enough
  bool prevIsComposition = mIMEComposing;
  bool StartComposite = !prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool UpdateComposite = prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool EndComposite = prevIsComposition && preEdit.IsEmpty();
  mIMEComposing = UpdateComposite || StartComposite;
  nsString pushStr = preEdit.IsEmpty() ? commit : preEdit;
  if (!commit.IsEmpty() && !EndComposite) {
    StartComposite = UpdateComposite = EndComposite = true;
  }

  if (StartComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_START, widget);
    InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
  }

  if (StartComposite || UpdateComposite || EndComposite) {
    WidgetCompositionEvent updateEvent(true, NS_COMPOSITION_UPDATE, widget);
    InitEvent(updateEvent, nullptr);
    updateEvent.mData = pushStr;
    mHelper->DispatchWidgetEvent(updateEvent);

    WidgetCompositionEvent event(true, NS_COMPOSITION_CHANGE, widget);
    InitEvent(event, nullptr);
    event.mData = pushStr;
    mHelper->DispatchWidgetEvent(event);

    nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
    if (!ps) {
      return false;
    }
    nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
    nsIContent* mTarget = DOMFocusManager->GetFocusedContent();

    InternalEditorInputEvent inputEvent(true, NS_EDITOR_INPUT, widget);
    inputEvent.time = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIMEComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  // If the key isn't autorepeat, we need to send the initial down event
  utils->SendKeyEvent(NS_LITERAL_STRING("keydown"), domKeyCode, 0, gmodifiers, 0, &handled);
  // Don't pass modifiers as NS_KEY_PRESS events.
  // Instead of selectively excluding some keys from NS_KEY_PRESS events,
  // we instead selectively include (as per MSDN spec
  // ( http://msdn.microsoft.com/en-us/library/system.windows.forms.control.keypress%28VS.71%29.aspx );
  // no official spec covers KeyPress events).
  if (domKeyCode != nsIDOMKeyEvent::DOM_VK_SHIFT &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_META &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_CONTROL &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_ALT)
  {
    utils->SendKeyEvent(NS_LITERAL_STRING("keypress"), domKeyCode, charCode, gmodifiers, 0, &handled);
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  utils->SendKeyEvent(NS_LITERAL_STRING("keyup"), domKeyCode, 0, gmodifiers, 0, &handled);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvMouseEvent(const nsString& aType,
                                         const float&    aX,
                                         const float&    aY,
                                         const int32_t&  aButton,
                                         const int32_t&  aClickCount,
                                         const int32_t&  aModifiers,
                                         const bool&     aIgnoreRootScrollFrame)
{
  if (!mWebBrowser) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);

  NS_ENSURE_TRUE(utils, true);
  bool ignored = false;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0, false, 4, &ignored);

  return !ignored;
}

bool
EmbedLiteViewThreadChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  WidgetTouchEvent localEvent;
  if (mHelper->ConvertMutiTouchInputToEvent(aData, localEvent)) {
    nsEventStatus status =
      mHelper->DispatchWidgetEvent(localEvent);
    nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNavigation);
    nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();
    if (innerWindow && innerWindow->HasTouchEventListeners()) {
      SendContentReceivedTouch(aGuid, mPendingTouchPreventedBlockId, nsIPresShell::gPreventMouseEvents);
    }
    mPendingTouchPreventedBlockId = aInputBlockId;
    static bool sDispatchMouseEvents;
    static bool sDispatchMouseEventsCached = false;
    if (!sDispatchMouseEventsCached) {
      sDispatchMouseEventsCached = true;
      Preferences::AddBoolVarCache(&sDispatchMouseEvents,
                                   "embedlite.dispatch_mouse_events", false);
    }
    if (status != nsEventStatus_eConsumeNoDefault && mDispatchSynthMouseEvents && sDispatchMouseEvents) {
      // Touch event not handled
      status = mHelper->DispatchSynthesizedMouseEvent(localEvent.message, localEvent.time, localEvent.refPoint, localEvent.widget);
      if (status != nsEventStatus_eConsumeNoDefault && status != nsEventStatus_eConsumeDoDefault) {
        mDispatchSynthMouseEvents = false;
      }
    }
  }
  if (aData.mType == MultiTouchInput::MULTITOUCH_END ||
      aData.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    mDispatchSynthMouseEvents = true;
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  return RecvInputDataTouchEvent(aGuid, aData, aInputBlockId);
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward)
{
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnWindowCloseRequested()
{
  return SendOnWindowCloseRequested() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnFirstPaint(int32_t aX, int32_t aY)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIDOMWindow> window;
  rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(window));

  nsCOMPtr<nsIDOMDocument> ddoc;
  window->GetDocument(getter_AddRefs(ddoc));
  NS_ENSURE_TRUE(ddoc, NS_OK);

  nsCOMPtr<nsIDocument> doc;
  doc = do_QueryInterface(ddoc, &rv);
  NS_ENSURE_TRUE(doc, NS_OK);

  nsIPresShell* presShell = doc->GetShell();
  if (presShell) {
    nscolor bgcolor = presShell->GetCanvasBackground();
    unused << SendSetBackgroundColor(bgcolor);
  }

  unused << RecvSetViewSize(mViewSize);

  return SendOnFirstPaint(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

bool
EmbedLiteViewThreadChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::FrameMetrics::ViewID *aViewIdOut)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mWebNavigation->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

} // namespace embedlite
} // namespace mozilla

