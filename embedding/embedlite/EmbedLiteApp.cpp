/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "mozilla/Logging.h"

#include "EmbedLiteApp.h"
#include "nsISupports.h"
#include "nsIFile.h"
#include "base/at_exit.h"
#include "mozilla/Unused.h"
#include "base/message_loop.h"               // for MessageLoop

#include "mozilla/embedlite/EmbedLiteAPI.h"
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/dom/MessageChannel.h"       // for MessageChannel
#include "mozilla/Hal.h"

#include "EmbedLiteUILoop.h"
#include "EmbedLiteSubThread.h"
#include "GeckoLoader.h"

#include "EmbedLiteAppThreadParent.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteView.h"
#include "EmbedLiteWindow.h"
#include "nsXULAppAPI.h"
#include "EmbedLiteMessagePump.h"
#include "EmbedLiteSecurity.h"

#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteAppProcessParent.h"

namespace mozilla {
namespace startup {
extern bool sIsEmbedlite;
extern GeckoProcessType sChildProcessType;
}
namespace embedlite {

namespace {
class FakeWindowListener : public EmbedLiteWindowListener {};
static FakeWindowListener sFakeWindowListener;
}

EmbedLiteApp* EmbedLiteApp::sSingleton = nullptr;
static nsTArray<nsCString> sComponentDirs;

EmbedLiteApp*
EmbedLiteApp::GetInstance()
{
  if (!sSingleton) {
    // We don't have the arguments by hand here.  If logging has already been
    // initialized by a previous call to LogModule::Init with the arguments
    // passed, passing (0, nullptr) is alright here.
    // Copied from xpcom/build/XPCOMInit.cpp
    mozilla::LogModule::Init(0, nullptr);
    sSingleton = new EmbedLiteApp();
    NS_ASSERTION(sSingleton, "not initialized");
  }
  return sSingleton;
}

EmbedLiteApp::EmbedLiteApp()
  : mListener(nullptr)
  , mUILoop(nullptr)
  , mSubThread(nullptr)
  , mAppParent(nullptr)
  , mAppChild(nullptr)
  , mEmbedType(EMBED_INVALID)
  , mState(STOPPED)
  , mRenderType(RENDER_AUTO)
  , mProfilePath(strdup("mozembed"))
  , mIsAsyncLoop(false)
{
  LOGT();
  sSingleton = this;

  hal::Init();
}

EmbedLiteApp::~EmbedLiteApp()
{
  LOGT();
  NS_ASSERTION(!mUILoop, "Main Loop not stopped before destroy");
  NS_ASSERTION(!mSubThread, "Thread not stopped/destroyed before destroy");
  NS_ASSERTION(mState == STOPPED, "Pre-mature deletion of still running application");

  hal::Shutdown();

  sSingleton = NULL;
  if (mProfilePath) {
    free(mProfilePath);
    mProfilePath = nullptr;
  }
}

void
EmbedLiteApp::SetListener(EmbedLiteAppListener* aListener)
{
  LOGT();
  // Assert with XOR that either mListener or aListener is NULL and not both.
  NS_ASSERTION((!mListener != !aListener), "App listener is supposed to be set only once by embedder");
  mListener = aListener;
}

EmbedLiteAppListener*
EmbedLiteApp::GetListener() {

  if (!mListener) {
    // No listener provided by embedder thus lazily create a stub object for EmbedLiteAppListener interface.
    // TODO: the instance is not refcounted and is going to leak memory when EmbedLiteApp::SetListener()
    //       is called. If embedder is supposed to set a listener always then it'd make sense
    //       to be less defensive and to crash instead of creating a stub.
    mListener = new EmbedLiteAppListener();
  }

  return mListener;
}

MessageLoop*
EmbedLiteApp::GetUILoop() {
  return static_cast<MessageLoop*>(mUILoop);
};

void*
EmbedLiteApp::PostTask(EMBEDTaskCallback callback, void* userData, int timeout)
{
  RefPtr<Runnable> newTask = NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::EMBEDTaskCallback",
                                                 callback, userData);
  if (timeout) {
    mUILoop->PostDelayedTask(newTask.forget(), timeout);
  } else {
    mUILoop->PostTask(newTask.forget());
  }

  return (void*)newTask;
}

void*
EmbedLiteApp::PostCompositorTask(EMBEDTaskCallback callback, void* userData, int timeout)
{
  if (!mozilla::layers::CompositorThreadHolder::IsActive()) {
    // Can't post compositor task if gecko compositor thread has not been initialized, yet.
    return nullptr;
  }

  RefPtr<Runnable> newTask = NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::EMBEDTaskCallback",
                                                 callback, userData);
  MOZ_ASSERT(mozilla::layers::CompositorThread());

  if (timeout) {
    mozilla::layers::CompositorThread()->DelayedDispatch(newTask.forget(), timeout);
  } else {
    mozilla::layers::CompositorThread()->Dispatch(newTask.forget());
  }

  return (void*)newTask;
}

void
EmbedLiteApp::CancelTask(void* aTask)
{
  if (aTask) {
    static_cast<CancelableRunnable*>(aTask)->Cancel();
  }
}

void
EmbedLiteApp::StartChild(EmbedLiteApp* aApp)
{
  LOGT();
  NS_ASSERTION(aApp->mState == STARTING, "Wrong timing");
  if (aApp->mEmbedType == EMBED_THREAD) {
    if (!aApp->mListener ||
        !aApp->mListener->ExecuteChildThread()) {
      // If toolkit hasn't started a child thread we have to create the thread on our own
      aApp->mSubThread = new EmbedLiteSubThread(aApp);
      if (!aApp->mSubThread->StartEmbedThread()) {
        LOGE("Failed to start child thread");
      }
    }
  } else if (aApp->mEmbedType == EMBED_PROCESS) {
    aApp->mAppParent = EmbedLiteAppProcessParent::CreateEmbedLiteAppProcessParent();
  }
}

void
EmbedLiteApp::SetProfilePath(const char* aPath)
{
  NS_ASSERTION(mState == STOPPED, "SetProfilePath must be called before Start");
  if (mProfilePath)
    free(mProfilePath);

  mProfilePath = aPath ? strdup(aPath) : nullptr;
}

EmbedLiteMessagePump*
EmbedLiteApp::CreateEmbedLiteMessagePump(EmbedLiteMessagePumpListener* aListener)
{
  return new EmbedLiteMessagePump(aListener);
}

bool
EmbedLiteApp::StartWithCustomPump(EmbedType aEmbedType, EmbedLiteMessagePump* aEventLoop)
{
  LOGT("Type: %s", aEmbedType == EMBED_THREAD ? "Thread" : "Process");
  mozilla::startup::sIsEmbedlite = aEmbedType == EMBED_PROCESS;
  NS_ASSERTION(mState == STOPPED, "App can be started only when it stays still");
  NS_ASSERTION(!mUILoop, "Start called twice");
  SetState(STARTING);
  mEmbedType = aEmbedType;
  mUILoop = aEventLoop->GetMessageLoop();
  mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp",
                                        &EmbedLiteApp::StartChild, this));
  mUILoop->StartLoop();
  mIsAsyncLoop = true;
  return true;
}

bool
EmbedLiteApp::Start(EmbedType aEmbedType)
{
  LOGT("Type: %s", aEmbedType == EMBED_THREAD ? "Thread" : "Process");
  mozilla::startup::sIsEmbedlite = aEmbedType == EMBED_PROCESS;
  NS_ASSERTION(mState == STOPPED, "App can be started only when it stays still");
  NS_ASSERTION(!mUILoop, "Start called twice");
  SetState(STARTING);
  mEmbedType = aEmbedType;
  base::AtExitManager exitManager;
  mUILoop = new EmbedLiteUILoop();
  mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::StartChild",
                                        &EmbedLiteApp::StartChild, this));
  mUILoop->StartLoop();
  if (mSubThread) {
    mSubThread->Stop();
    mSubThread = nullptr;
  } else if (mListener) {
    NS_ASSERTION(mListener->StopChildThread(),
                      "StopChildThread must be implemented when ExecuteChildThread defined");
  }
  if (mUILoop) {
    delete mUILoop;
    mUILoop = NULL;
  }

  if (mListener) {
    mListener->Destroyed();
  }

  return true;
}

void
EmbedLiteApp::AddManifestLocation(const char* manifest)
{
  if (mState == INITIALIZED) {
    Unused << mAppParent->SendLoadComponentManifest(nsDependentCString(manifest));
  } else {
    sComponentDirs.AppendElement(nsCString(manifest));
  }
}

bool
EmbedLiteApp::StartChildThread()
{
  NS_ENSURE_TRUE(mEmbedType == EMBED_THREAD, false);
  LOGT("mUILoop:%p, current:%p", mUILoop, MessageLoop::current());
  NS_ASSERTION(MessageLoop::current() != mUILoop,
               "Current message loop must be null and not equals to mUILoop");

  for (unsigned int i = 0; i < sComponentDirs.Length(); i++) {
    nsCOMPtr<nsIFile> f;
    NS_NewNativeLocalFile(sComponentDirs[i], true,
                          getter_AddRefs(f));
    if (f) {
      LOGT("Loading manifest: %s", sComponentDirs[i].get());
      XRE_AddManifestLocation(NS_APP_LOCATION, f);
    } else {
      NS_ERROR(nsPrintfCString("Failed to create nsIFile for manifest location: %s", sComponentDirs[i].get()).get());
    }
  }

  GeckoLoader::InitEmbedding(mProfilePath);

  mAppParent = new EmbedLiteAppThreadParent();
  mAppChild = new EmbedLiteAppThreadChild(mUILoop);
  MessageLoop::current()->PostTask(NewRunnableMethod<mozilla::ipc::MessageChannel*>("mozilla::embedlite::EmbedLiteAppThreadChild::Init",
                                                                                    mAppChild.get(),
                                                                                    &EmbedLiteAppThreadChild::Init,
                                                                                    mAppParent->GetIPCChannel()));

  return true;
}

bool
EmbedLiteApp::StopChildThread()
{
  NS_ENSURE_TRUE(mEmbedType == EMBED_THREAD, false);
  LOGT("mUILoop:%p, current:%p", mUILoop, MessageLoop::current());

  if (!mUILoop || !MessageLoop::current() ||
      mUILoop == MessageLoop::current()) {
    NS_ERROR("Wrong thread? StartChildThread called? Stop() already called?");
    return false;
  }

  mAppChild->Close();
  delete mAppParent;
  mAppParent = nullptr;
  mAppChild = nullptr;

  GeckoLoader::TermEmbedding();

  return true;
}

void _FinalStop(EmbedLiteApp* app)
{
  app->Shutdown();
}

void
EmbedLiteApp::PreDestroy(EmbedLiteApp* app)
{
  if (app->mAppParent == nullptr) {
    LOGE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!app->mAppParent is null, wrong logic?");
    return;
  }
  Unused << app->mAppParent->SendPreDestroy();
}

void
EmbedLiteApp::Stop()
{
  LOGT();
  NS_ASSERTION(mState == STARTING || mState == INITIALIZED, "Wrong timing");

  if (mState == INITIALIZED) {
    if (mViews.empty() && mWindows.empty()) {
      mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::PreDestroy",
                                            &EmbedLiteApp::PreDestroy, this));
    } else {
      for (auto viewPair : mViews) {
        viewPair.second->Destroy();
      }
      for (auto winPair: mWindows) {
        winPair.second->Destroy();
      }
    }
  }

  SetState(DESTROYING);
}

void
EmbedLiteApp::Shutdown()
{
  LOGT();
  NS_ASSERTION(mState == DESTROYING, "Wrong timing");

  if (mIsAsyncLoop) {
    if (mEmbedType == EMBED_THREAD) {
      if (mSubThread) {
        mSubThread->Stop();
        mSubThread = nullptr;
      } else if (mListener) {
        NS_ASSERTION(mListener->StopChildThread(),
            "StopChildThread must be implemented when ExecuteChildThread defined");
      }
    } else if (mEmbedType == EMBED_PROCESS) {
      delete mAppParent;
    }
  }

  mUILoop->DoQuit();

  if (mIsAsyncLoop) {
    delete mUILoop;
    mUILoop = nullptr;
  }

  if (mListener) {
    mListener->Destroyed();
  }

  SetState(STOPPED);
}

void
EmbedLiteApp::SetBoolPref(const char* aName, bool aValue)
{
  NS_ENSURE_TRUE(mState == INITIALIZED, );
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendSetBoolPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::SetCharPref(const char* aName, const char* aValue)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendSetCharPref(nsDependentCString(aName), nsDependentCString(aValue));
}

void
EmbedLiteApp::SetIntPref(const char* aName, int aValue)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendSetIntPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::LoadGlobalStyleSheet(const char* aUri, bool aEnable)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendLoadGlobalStyleSheet(nsDependentCString(aUri), aEnable);
}

void
EmbedLiteApp::SendObserve(const char* aMessageName, const char16_t* aMessage)
{
  LOGT("topic:%s", aMessageName);
  NS_ENSURE_TRUE(mState == INITIALIZED, );
  Unused << mAppParent->SendObserve(nsDependentCString(aMessageName), aMessage ? nsDependentString((const char16_t*)aMessage) : nsString());
}

void
EmbedLiteApp::AddObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendAddObserver(nsDependentCString(aMessageName));
}

void
EmbedLiteApp::RemoveObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  Unused << mAppParent->SendRemoveObserver(nsDependentCString(aMessageName));
}

void EmbedLiteApp::AddObservers(const std::vector<std::string> &observersList)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");

  nsTArray<nsCString> list;
  for (const auto &observer : observersList) {
      list.AppendElement(nsDependentCString(observer.c_str()));
  }

  Unused << mAppParent->SendAddObservers(list);
}

void EmbedLiteApp::RemoveObservers(const std::vector<std::string>& observersList)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");

  nsTArray<nsCString> list;
  for (const auto &observer : observersList) {
      list.AppendElement(nsDependentCString(observer.c_str()));
  }

  Unused << mAppParent->SendRemoveObservers(list);
}

EmbedLiteView*
EmbedLiteApp::CreateView(EmbedLiteWindow* aWindow, uint32_t aParent, uintptr_t aParentBrowsingContext, bool aIsPrivateWindow, bool isDesktopMode, bool isHidden)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "The app must be up and runnning by now");
  static uint32_t sViewCreateID = 0;
  sViewCreateID++;

  PEmbedLiteViewParent* viewParent = static_cast<PEmbedLiteViewParent*>(
      mAppParent->SendPEmbedLiteViewConstructor(aWindow->GetUniqueID(), sViewCreateID,
                                                aParent, aParentBrowsingContext, aIsPrivateWindow,
                                                isDesktopMode, isHidden));
  EmbedLiteView* view = new EmbedLiteView(this, aWindow, viewParent, sViewCreateID);
  mViews[sViewCreateID] = view;
  return view;
}

EmbedLiteWindow*
EmbedLiteApp::CreateWindow(int width, int height, EmbedLiteWindowListener *aListener)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "The app must be up and runnning by now");
  static uint32_t sWindowCreateID = 0;
  sWindowCreateID++;

  if (!aListener) {
      aListener = &sFakeWindowListener;
  }

  PEmbedLiteWindowParent* windowParent = static_cast<PEmbedLiteWindowParent*>(
      mAppParent->SendPEmbedLiteWindowConstructor(width, height, sWindowCreateID, reinterpret_cast<uintptr_t>(aListener)));
  EmbedLiteWindow* window = new EmbedLiteWindow(this, windowParent, sWindowCreateID);
  mWindows[sWindowCreateID] = window;
  return window;
}

EmbedLiteSecurity* EmbedLiteApp::CreateSecurity(const char *aStatus, unsigned int aState) const
{
    LOGT();
    NS_ASSERTION(mState == INITIALIZED, "The app must be up and runnning by now");

    EmbedLiteSecurity * embedSecurity = new EmbedLiteSecurity(aStatus, aState);
    return embedSecurity;
}

void
EmbedLiteApp::ChildReadyToDestroy()
{
  LOGT();
  if (mState == DESTROYING) {
    mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::_FinalStop",
                                          &_FinalStop, this));
  }
  if (mEmbedType == EMBED_PROCESS) {
      mAppParent = nullptr;
  }
}

uint32_t
EmbedLiteApp::CreateWindowRequested(const uint32_t &chromeFlags,
                                    const bool &hidden,
                                    const uint32_t &parentId,
                                    const uintptr_t &parentBrowsingContext)
{
  EmbedLiteView* view = nullptr;
  std::map<uint32_t, EmbedLiteView*>::iterator it;
  for (it = mViews.begin(); it != mViews.end(); it++) {
    if (it->second && it->second->GetUniqueID() == parentId) {
      LOGT("Found parent view:%p", it->second);
      view = it->second;
      break;
    }
  }
  uint32_t viewId = mListener ? mListener->CreateNewWindowRequested(chromeFlags, hidden, view, parentBrowsingContext) : 0;
  return viewId;
}

void
EmbedLiteApp::ViewDestroyed(uint32_t id)
{
  LOGT("id:%i", id);
  std::map<uint32_t, EmbedLiteView*>::iterator it = mViews.find(id);
  if (it != mViews.end()) {
    EmbedLiteView* view = it->second;
    mViews.erase(it);
    delete view;
  }
  if (mViews.empty()) {
    if (mListener) {
      mListener->LastViewDestroyed();
    }
    if (mState == DESTROYING) {
      mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::PreDestroy",
                                            &EmbedLiteApp::PreDestroy, this));
    }
  }
}

void
EmbedLiteApp::WindowDestroyed(uint32_t id)
{
  LOGT("id:%i", id);
  std::map<uint32_t, EmbedLiteWindow*>::iterator it = mWindows.find(id);
  if (it != mWindows.end()) {
    EmbedLiteWindow* win = it->second;
    mWindows.erase(it);
    delete win;
  }
  if (mWindows.empty()) {
    if (mListener) {
      mListener->LastWindowDestroyed();
    }
  }
}

void EmbedLiteApp::DestroyView(EmbedLiteView* aView)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  for (auto elm : mViews) {
    if (aView == elm.second) {
      elm.second->Destroy();
      return;
    }
  }
  MOZ_ASSERT(false, "Invalid EmbedLiteView pointer!");
}

void EmbedLiteApp::DestroyWindow(EmbedLiteWindow* aWindow)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  for (auto elm : mWindows) {
    if (aWindow == elm.second) {
      elm.second->Destroy();
      return;
    }
  }
  MOZ_ASSERT(false, "Invalid EmbedLiteWindow pointer!");
}

int EmbedLiteApp::GetNumberOfViews() const
{
    return mViews.size();
}

int EmbedLiteApp::GetNumberOfWindows() const
{
    return mWindows.size();
}

void EmbedLiteApp::DestroySecurity(EmbedLiteSecurity* aSecurity) const
{
    LOGT();
    NS_ASSERTION(mState == INITIALIZED, "Wrong timing");

    delete aSecurity;
}

void
EmbedLiteApp::SetIsAccelerated(bool aIsAccelerated)
{
#if defined(GL_PROVIDER_EGL) || defined(GL_PROVIDER_GLX)
  if (aIsAccelerated) {
    mRenderType = RENDER_HW;
  } else
#endif
  {
    mRenderType = RENDER_SW;
  }
}

void
EmbedLiteApp::Initialized()
{
  LOGT();
  NS_ASSERTION(mState == STARTING || mState == DESTROYING, "Wrong timing");

  if (mState == DESTROYING) {
    mUILoop->PostTask(NewRunnableFunction("mozilla::embedlite::EmbedLiteApp::PreDestroy",
                                          &EmbedLiteApp::PreDestroy, this));
    return;
  }

  SetState(INITIALIZED);
  if (mListener) {
    mListener->Initialized();
  }
}

void
EmbedLiteApp::SetState(State aState)
{
  LOGT("State transition: %d -> %d", mState, aState);
  mState = aState;
}

} // namespace embedlite
} // namespace mozilla

mozilla::embedlite::EmbedLiteApp*
XRE_GetEmbedLite()
{
  return mozilla::embedlite::EmbedLiteApp::GetInstance();
}
