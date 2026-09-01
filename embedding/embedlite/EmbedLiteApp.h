/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_APP_H
#define EMBED_LITE_APP_H

#include "mozilla/RefPtr.h"
#include "mozilla/Types.h"

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

class MessageLoop;

namespace base {
class AtExitManager;
}

namespace mozilla {
namespace embedlite {

typedef void (*EMBEDTaskCallback)(void* userData);

class EmbedLiteAppObserver;
class EmbedLiteMessagePump;
class EmbedLiteMessagePumpListener;
class EmbedLiteSecurity;
class EmbedLiteUILoop;
class EmbedLiteWindow;
class EmbedLiteWindowListener;

class EmbedLiteAppListener
{
public:
  virtual void Initialized() {}
  virtual void Destroyed() {}
  virtual void OnObserve(const char* aMessage, const char16_t* aData) {}
  virtual void LastWindowDestroyed() {}
};

class EmbedLiteApp
{
public:
  virtual ~EmbedLiteApp();

  enum RenderType {
    RENDER_AUTO,
    RENDER_SW,
    RENDER_HW
  };

  virtual void SetListener(EmbedLiteAppListener* aListener);

  virtual void* PostTask(EMBEDTaskCallback callback, void* userData,
                         int timeout = 0);
  virtual void* PostCompositorTask(EMBEDTaskCallback callback, void* userData,
                                   int timeout = 0);
  virtual void CancelTask(void* aTask);

  virtual void SetProfilePath(const char* aPath);
  virtual void SetEGLDisplay(void* aDisplay);

  // Gecko is initialized on the calling toolkit's main thread and its work is
  // dispatched by the selected message pump until Stop() completes.
  virtual bool Start();
  virtual void Stop();

  virtual void SetCompositorInSeparateThread(bool aOwnThread) {}

  virtual EmbedLiteMessagePump* CreateEmbedLiteMessagePump(
    EmbedLiteMessagePumpListener* aListener);
  virtual bool StartWithCustomPump(EmbedLiteMessagePump* aMessageLoop);

  virtual void AddManifestLocation(const char* manifest);

  // Every window is a Gecko chrome AppWindow containing ordinary remote XUL
  // browsers. CreateWindow remains as the generic API spelling.
  virtual EmbedLiteWindow* CreateWindow(
    int width, int height, EmbedLiteWindowListener* aListener = nullptr);
  MOZ_EXPORT EmbedLiteWindow* CreateChromeWindow(
    int width, int height, const char* initialContentURI,
    EmbedLiteWindowListener* aListener = nullptr,
    bool aPrivateBrowsing = false);
  MOZ_EXPORT EmbedLiteWindow* CreateChromeTabWindow(
    int width, int height, EmbedLiteWindowListener* aListener = nullptr,
    bool aPrivateBrowsing = false);

  virtual EmbedLiteSecurity* CreateSecurity(
    const char* aStatus, unsigned int aState) const;
  virtual void DestroyWindow(EmbedLiteWindow* aWindow);
  virtual void DestroySecurity(EmbedLiteSecurity* aSecurity) const;

  virtual int GetNumberOfWindows() const;
  virtual void SetIsAccelerated(bool aIsAccelerated);
  virtual bool IsAccelerated() { return mRenderType == RENDER_HW; }
  virtual RenderType GetRenderType() { return mRenderType; }

  virtual void SetBoolPref(const char* aName, bool aValue);
  virtual void SetCharPref(const char* aName, const char* aValue);
  virtual void SetIntPref(const char* aName, int aValue);
  virtual void LoadGlobalStyleSheet(const char* aUri, bool aEnable);
  virtual void LoadUserStyleSheet(const char* aUri, bool aEnable);

  virtual void SendObserve(const char* aMessageName,
                           const char16_t* aMessage);
  virtual void AddObserver(const char* aMessageName);
  virtual void RemoveObserver(const char* aMessageName);
  virtual void AddObservers(
    const std::vector<std::string>& aObservers);
  virtual void RemoveObservers(
    const std::vector<std::string>& aObservers);

  static EmbedLiteApp* GetInstance();

private:
  EmbedLiteApp();

  enum State {
    STOPPED,
    STARTING,
    INITIALIZED,
    DESTROYING
  };

  bool StartInternal(EmbedLiteUILoop* aLoop);
  bool InitializeRuntime();
  bool InitializeAppServices();
  void FinishShutdown();
  void MaybeFinishShutdown();
  void RemoveAllObservers();
  void SetState(State aState);

  EmbedLiteWindow* CreateWindowInternal(
    int width, int height, const char* aInitialContentURI,
    bool aPrivateBrowsing, EmbedLiteWindowListener* aListener);

  static void StartRuntimeTask(EmbedLiteApp* aApp);
  static void FinishShutdownTask(EmbedLiteApp* aApp);

  friend class EmbedLiteAppObserver;
  friend class EmbedLiteCompositorBridgeParent;
  friend class EmbedLitePuppetWidget;
  friend class EmbedLiteWindow;
  friend class EmbedLiteWindowParent;
  friend class nsWindow;

  void WindowDestroyed(uint32_t aId);
  EmbedLiteAppListener* GetListener();
  MessageLoop* GetUILoop();

  static EmbedLiteApp* sSingleton;
  EmbedLiteAppListener* mListener;
  EmbedLiteUILoop* mUILoop;
  RefPtr<EmbedLiteAppObserver> mObserver;
  std::vector<std::string> mObservedTopics;
  std::map<uint32_t, EmbedLiteWindow*> mWindows;
  base::AtExitManager* mAtExitManager;
  State mState;
  RenderType mRenderType;
  char* mProfilePath;
  bool mEmbeddingInitialized;
  bool mShutdownScheduled;
};

} // namespace embedlite
} // namespace mozilla

#endif // EMBED_LITE_APP_H
