/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_H
#define MOZ_VIEW_EMBED_H

#include "mozilla/RefPtr.h"
#include "nsStringGlue.h"
#include "gfxMatrix.h"
#include "nsRect.h"

class EmbedLiteViewIface;

namespace mozilla {
class InputData;
namespace embedlite {

class EmbedLiteViewThreadParent;
class PEmbedLiteViewParent;
class EmbedLiteView;
class EmbedLiteWindow;

class EmbedLiteViewListener
{
public:
  // View intialized and ready for API calls
  virtual void ViewInitialized() {}
  // View finally destroyed and deleted
  virtual void ViewDestroyed() {}

  // Messaging interface, allow to receive json messages from content child scripts
  virtual void RecvAsyncMessage(const char16_t* aMessage, const char16_t* aData) {}
  virtual char* RecvSyncMessage(const char16_t* aMessage, const char16_t* aData) { return NULL; }

  virtual void OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward) {}
  virtual void OnLoadStarted(const char* aLocation) {}
  virtual void OnLoadFinished(void) {}
  virtual void OnLoadRedirect(void) {}
  virtual void OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal) {}
  virtual void OnSecurityChanged(const char* aStatus, unsigned int aState) {}
  virtual void OnFirstPaint(int32_t aX, int32_t aY) {}
  virtual void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight) {}
  virtual void OnScrollChanged(int32_t offSetX, int32_t offSetY) {}
  virtual void OnTitleChanged(const char16_t* aTitle) {}
  virtual void SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {}
  virtual void OnWindowCloseRequested(void) {}

  virtual void IMENotification(int aEnabled, bool aOpen, int aCause, int aFocusChange, const char16_t* inputType, const char16_t* inputMode) {}
  virtual void GetIMEStatus(int32_t* aIMEEnabled, int32_t* aIMEOpen, intptr_t* aNativeIMEContext) {}

  // AZPC Interface, return true in order to prevent default behavior
  virtual bool RequestContentRepaint() { return false; }
  virtual bool HandleDoubleTap(const nsIntPoint& aPoint) { return false; }
  virtual bool HandleSingleTap(const nsIntPoint& aPoint) { return false; }
  virtual bool HandleLongTap(const nsIntPoint& aPoint) { return false; }
  virtual bool AcknowledgeScrollUpdate(const uint32_t& aViewID, const uint32_t& aScrollGeneration) { return false; }
  virtual bool SendAsyncScrollDOMEvent(const gfxRect& aContentRect,
                                       const gfxSize& aScrollableSize) { return false; }
};

class EmbedLiteApp;
class EmbedLiteView
{
public:
  EmbedLiteView(EmbedLiteApp* aApp, EmbedLiteWindow* aWindow, PEmbedLiteViewParent* aViewImpl, uint32_t aViewId);

  // Listener setup, call this with null pointer if listener destroyed before EmbedLiteView
  virtual void SetListener(EmbedLiteViewListener* aListener);
  virtual EmbedLiteViewListener* const GetListener() const;

  // Embed Interface
  virtual void LoadURL(const char* aUrl);
  virtual void SetIsActive(bool);
  virtual void SetIsFocused(bool);
  virtual void SetThrottlePainting(bool);
  virtual void SuspendTimeouts();
  virtual void ResumeTimeouts();
  virtual void GoBack();
  virtual void GoForward();
  virtual void StopLoad();
  virtual void Reload(bool hard);

  // Scrolling methods see nsIDomWindow.idl
  // Scrolls this view to an absolute pixel offset.
  virtual void ScrollTo(int x, int y);
  // Scrolls this view to a pixel offset relative to
  // the current scroll position.
  virtual void ScrollBy(int x, int y);

  // Input Interface
  virtual void SendTextEvent(const char* composite, const char* preEdit);
  virtual void SendKeyPress(int domKeyCode, int gmodifiers, int charCode);
  virtual void SendKeyRelease(int domKeyCode, int gmodifiers, int charCode);

  virtual void ReceiveInputEvent(const mozilla::InputData& aEvent);
  virtual void MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);
  virtual void MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);
  virtual void MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);

  virtual void PinchStart(int x, int y);
  virtual void PinchUpdate(int x, int y, float scale);
  virtual void PinchEnd(int x, int y, float scale);

  virtual void SetMargins(int top, int right, int bottom, int left);

  // Set DPI for the view (views placed on different screens may get different DPI).
  virtual void SetDPI(const float& dpi);

  // Scripting Interface, allow to extend embedding API by creating
  // child js scripts and messaging interface.
  // and do communication between UI and Content child via json messages.
  // See RecvAsyncMessage, RecvSyncMessage, SendAsyncMessage
  // For more detailed info see https://developer.mozilla.org/en-US/docs/The_message_manager
  //   https://wiki.mozilla.org/Content_Process_Event_Handlers
  virtual void LoadFrameScript(const char* aURI);

  virtual void AddMessageListener(const char* aMessageName);
  virtual void RemoveMessageListener(const char* aMessageName);
  virtual void AddMessageListeners(const nsTArray<nsString>& aMessageNames);
  virtual void RemoveMessageListeners(const nsTArray<nsString>& aMessageNames);
  virtual void SendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage);

  virtual uint32_t GetUniqueID();

protected:
  friend class EmbedLiteApp; // Needs to destroy the view.

  virtual ~EmbedLiteView();
  // Request the view to be destroyed. Once this async process is done
  // EmbedLiteViewListener::ViewDestroyed will be called. This interface
  // should only be used by EmbedLiteApp. EmbedLite users should destroy
  // EmbedLiteViews by calling EmbedLiteApp::DestroyView.
  void Destroy();

private:
  friend class EmbedLiteViewBaseParent;
  friend class EmbedLiteViewThreadParent;

  void Destroyed();

  EmbedLiteViewIface* GetImpl();

  EmbedLiteApp* mApp;
  EmbedLiteWindow* mWindow;
  EmbedLiteViewListener* mListener;
  EmbedLiteViewIface* mViewImpl;
  PEmbedLiteViewParent* mViewParent;
  const uint32_t mUniqueID;
};

} // namespace embedlite
} // namespace mozilla

#endif
