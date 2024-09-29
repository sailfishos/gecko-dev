/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_APP_H
#define EMBED_LITE_APP_H

#include "mozilla/RefPtr.h"
#include <string>
#include <vector>
#include <stdint.h>
#include <map>

class MessageLoop;

namespace mozilla {
namespace embedlite {

typedef void (*EMBEDTaskCallback)(void* userData);

class EmbedLiteMessagePump;
class EmbedLiteMessagePumpListener;
class EmbedLiteUILoop;
class EmbedLiteAppThreadChild;
class EmbedLiteAppThreadParent;
class EmbedLiteSubThread;
class EmbedLiteSubProcess;
class EmbedLiteAppProcessParent;
class EmbedLiteView;
class EmbedLiteWindow;
class PEmbedLiteAppParent;
class EmbedLiteSecurity;
class EmbedLiteWindowListener;
class EmbedLiteAppListener
{
public:
  // StartChildThread must be called in case of native thread, otherwise return false
  virtual bool ExecuteChildThread() {
    return false;
  }
  // Native thread must be stopped here
  virtual bool StopChildThread() {
    return false;
  }
  // App Initialized and ready to API call
  virtual void Initialized() {}
  // App Destroyed, and ready to delete and program exit
  virtual void Destroyed() {}
  // Messaging interface, allow to receive json messages from content child scripts
  virtual void OnObserve(const char* aMessage, const char16_t* aData) {}
  // New Window request which is usually coming from WebPage new window request
  virtual uint32_t CreateNewWindowRequested(const uint32_t &chromeFlags,
                                            const bool &hidden,
                                            EmbedLiteView *aParentView,
                                            const uintptr_t &parentBrowsingContext) { return 0; }
  virtual void LastViewDestroyed() {};
  virtual void LastWindowDestroyed() {};
};

class EmbedLiteApp
{
public:
  virtual ~EmbedLiteApp();

  enum EmbedType {
    EMBED_INVALID,// Default value
    EMBED_THREAD, // Initialize XPCOM in child thread
    EMBED_PROCESS // Initialize XPCOM in separate process
  };

  enum RenderType {
    RENDER_AUTO,// Default value
    RENDER_SW,  // Initialize software rendering
    RENDER_HW   // Initialize hardware accelerated rendering
  };

  // Set Listener interface for EmbedLiteApp notifications
  virtual void SetListener(EmbedLiteAppListener* aListener);

  // Public Embedding API

  virtual EmbedType GetType() {
    return mEmbedType;
  }

  // Delayed post task helper for delayed functions call in main thread
  virtual void* PostTask(EMBEDTaskCallback callback, void* userData, int timeout = 0);
  virtual void* PostCompositorTask(EMBEDTaskCallback callback, void* userData, int timeout = 0);
  virtual void CancelTask(void* aTask);

  // Setup profile path for embedding, or null if embedding supposed to be profile-less
  virtual void SetProfilePath(const char* aPath);
  // Start UI embedding loop merged with Gecko GFX, blocking call until Stop() called
  virtual bool Start(EmbedType aEmbedType);
  // Exit from UI embedding loop started with Start()
  virtual void Stop();

  // if true then compositor will be started in separate own thread, and view->CompositorCreated notification will be called in non-main thread
  virtual void SetCompositorInSeparateThread(bool aOwnThread) { } // Deprecated

  // Create custom Event Message pump, alloc new object which must be destroyed in EmbedLiteAppListener::Destroyed, or later
  virtual EmbedLiteMessagePump* CreateEmbedLiteMessagePump(EmbedLiteMessagePumpListener* aListener);

  // Start UI embedding loop merged with Gecko GFX
  virtual bool StartWithCustomPump(EmbedType aEmbedType, EmbedLiteMessagePump* aMessageLoop);

  // Specify path to Gecko components manifest location
  virtual void AddManifestLocation(const char* manifest);

  // This must be called in native toolkit child thread, only after ExecuteChildThread call
  virtual bool StartChildThread();
  // Must be called from same thread as StartChildThread, and before Stop()
  virtual bool StopChildThread();

  virtual EmbedLiteView* CreateView(EmbedLiteWindow* aWindow,
                                    uint32_t aParent = 0,
                                    uintptr_t parentBrowsingContext = 0,
                                    bool aIsPrivateWindow = false,
                                    bool isDesktopMode = false,
                                    bool isHidden = false);
  virtual EmbedLiteWindow* CreateWindow(int width, int height, EmbedLiteWindowListener *aListener = nullptr);
  virtual EmbedLiteSecurity* CreateSecurity(const char *aStatus, unsigned int aState) const;
  virtual void DestroyView(EmbedLiteView* aView);
  virtual void DestroyWindow(EmbedLiteWindow* aWindow);
  virtual void DestroySecurity(EmbedLiteSecurity* aSecurity) const;

  virtual int GetNumberOfViews() const;
  virtual int GetNumberOfWindows() const;
  virtual void SetIsAccelerated(bool aIsAccelerated);
  virtual bool IsAccelerated() {
    return mRenderType == RENDER_HW ? true : false ;
  }
  virtual RenderType GetRenderType() {
    return mRenderType;
  }

  // Setup preferences
  virtual void SetBoolPref(const char* aName, bool aValue);
  virtual void SetCharPref(const char* aName, const char* aValue);
  virtual void SetIntPref(const char* aName, int aValue);

  virtual void LoadGlobalStyleSheet(const char* aUri, bool aEnable);

  // Observer interface
  virtual void SendObserve(const char* aMessageName, const char16_t* aMessage);
  virtual void AddObserver(const char* aMessageName);
  virtual void RemoveObserver(const char* aMessageName);
  virtual void AddObservers(const std::vector<std::string> &observersList);
  virtual void RemoveObservers(const std::vector<std::string> &observersList);

  // Only one EmbedHelper object allowed
  static EmbedLiteApp* GetInstance();

  // TODO: this must be hidden for API users
  void Shutdown();

private:
  EmbedLiteApp();

  /*
   * States of EmbedLiteApp's lifecycle
   */
  enum State {
    // This is the initial and final state of EmbedLiteApp's lifecycle
    // Allowed next states: STARTING
    STOPPED,

    // The app is in a start-up process. No messages can be sent to the
    // EmbedLiteApp instance from a toolkit in this state except Stop().
    // Allowed next states: INITIALIZED, DESTROYING
    STARTING,

    // The app is ready to operate with views.
    // Allowed next states: DESTROYING
    INITIALIZED,

    // The app is in shutdown process.
    // Allowed next states: STOPPED
    DESTROYING
  };

  void SetState(State aState);

  static void StartChild(EmbedLiteApp* aApp);
  void Initialized();

  friend class EmbedLiteAppProcessParent;
  friend class EmbedLiteAppThreadParent;
  friend class EmbedLiteCompositorBridgeParent;
  friend class EmbedLitePuppetWidget;
  friend class nsWindow;
  friend class EmbedLiteView;
  friend class EmbedLiteWindow;

  void ViewDestroyed(uint32_t id);
  void WindowDestroyed(uint32_t id);
  void ChildReadyToDestroy();
  uint32_t CreateWindowRequested(const uint32_t &chromeFlags,
                                 const bool &hidden,
                                 const uint32_t &parentId,
                                 const uintptr_t &parentBrowsingContext);
  EmbedLiteAppListener* GetListener();
  MessageLoop* GetUILoop();
  static void PreDestroy(EmbedLiteApp*);

  static EmbedLiteApp* sSingleton;
  EmbedLiteAppListener* mListener;
  EmbedLiteUILoop* mUILoop;

  RefPtr<EmbedLiteSubThread> mSubThread;
  PEmbedLiteAppParent* mAppParent;
  RefPtr<EmbedLiteAppThreadChild> mAppChild;

  EmbedType mEmbedType;
  std::map<uint32_t, EmbedLiteView*> mViews;
  std::map<uint32_t, EmbedLiteWindow*> mWindows;
  State mState;
  RenderType mRenderType;
  char* mProfilePath;
  bool mIsAsyncLoop;
};

} // namespace embedlite
} // namespace mozilla

#endif // EMBED_LITE_APP_H
