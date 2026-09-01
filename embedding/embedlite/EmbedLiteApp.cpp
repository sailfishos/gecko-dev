/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteApp.h"
#include "EmbedLiteAPI.h"

#include "EmbedLiteAppService.h"
#include "EmbedLiteCompositorBridgeParent.h"
#include "EmbedLiteJSON.h"
#include "EmbedLiteMessagePump.h"
#include "EmbedLiteSecurity.h"
#include "EmbedLiteUILoop.h"
#include "EmbedLiteWindow.h"
#include "EmbedLiteWindowParent.h"
#include "EmbedLog.h"
#include "GeckoLoader.h"
#include "GLContextProvider.h"

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "mozilla/Assertions.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/Hal.h"
#include "mozilla/Logging.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIStyleSheetService.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

#include <algorithm>

namespace mozilla {
namespace embedlite {

NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteJSON)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedLiteAppService)

namespace {

class FakeWindowListener final : public EmbedLiteWindowListener {};
static FakeWindowListener sFakeWindowListener;
static nsTArray<nsCString> sComponentDirs;

} // namespace

class EmbedLiteAppObserver final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit EmbedLiteAppObserver(EmbedLiteApp* aApp) : mApp(aApp) {}
  void Detach() { mApp = nullptr; }

private:
  ~EmbedLiteAppObserver() = default;
  EmbedLiteApp* mApp;
};

NS_IMPL_ISUPPORTS(EmbedLiteAppObserver, nsIObserver)

NS_IMETHODIMP
EmbedLiteAppObserver::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData)
{
  if (mApp && mApp->mListener) {
    mApp->mListener->OnObserve(aTopic, aData);
  }
  return NS_OK;
}

EmbedLiteApp* EmbedLiteApp::sSingleton = nullptr;

EmbedLiteApp*
EmbedLiteApp::GetInstance()
{
  if (!sSingleton) {
    mozilla::LogModule::Init(0, nullptr);
    sSingleton = new EmbedLiteApp();
    NS_ASSERTION(sSingleton, "not initialized");
  }
  return sSingleton;
}

EmbedLiteApp::EmbedLiteApp()
  : mListener(nullptr)
  , mUILoop(nullptr)
  , mAtExitManager(nullptr)
  , mState(STOPPED)
  , mRenderType(RENDER_AUTO)
  , mProfilePath(strdup("mozembed"))
  , mEmbeddingInitialized(false)
  , mShutdownScheduled(false)
{
  LOGT();
  sSingleton = this;
  hal::Init();
}

EmbedLiteApp::~EmbedLiteApp()
{
  LOGT();
  NS_ASSERTION(!mUILoop, "Main loop not stopped before destruction");
  NS_ASSERTION(!mEmbeddingInitialized,
               "Gecko not terminated before destruction");
  NS_ASSERTION(mState == STOPPED,
               "Premature deletion of a running application");

  hal::Shutdown();
  delete mAtExitManager;
  mAtExitManager = nullptr;
  sSingleton = nullptr;
  free(mProfilePath);
  mProfilePath = nullptr;
}

void
EmbedLiteApp::SetListener(EmbedLiteAppListener* aListener)
{
  NS_ASSERTION((!mListener != !aListener),
               "App listener may only be attached or detached once");
  mListener = aListener;
}

EmbedLiteAppListener*
EmbedLiteApp::GetListener()
{
  if (!mListener) {
    mListener = new EmbedLiteAppListener();
  }
  return mListener;
}

MessageLoop*
EmbedLiteApp::GetUILoop()
{
  return static_cast<MessageLoop*>(mUILoop);
}

void*
EmbedLiteApp::PostTask(EMBEDTaskCallback aCallback, void* aUserData,
                       int aTimeout)
{
  if (!mUILoop) {
    return nullptr;
  }
  RefPtr<CancelableRunnable> task = NS_NewCancelableRunnableFunction(
    "mozilla::embedlite::EmbedLiteApp::PostTask",
    [aCallback, aUserData]() { aCallback(aUserData); });
  CancelableRunnable* handle = task.get();
  if (aTimeout) {
    mUILoop->PostDelayedTask(task.forget(), aTimeout);
  } else {
    mUILoop->PostTask(task.forget());
  }
  return handle;
}

void*
EmbedLiteApp::PostCompositorTask(EMBEDTaskCallback aCallback,
                                 void* aUserData, int aTimeout)
{
  if (!layers::CompositorThreadHolder::IsActive()) {
    return nullptr;
  }

  RefPtr<CancelableRunnable> task = NS_NewCancelableRunnableFunction(
    "mozilla::embedlite::EmbedLiteApp::PostCompositorTask",
    [aCallback, aUserData]() { aCallback(aUserData); });
  CancelableRunnable* handle = task.get();
  MOZ_ASSERT(layers::CompositorThread());
  if (aTimeout) {
    layers::CompositorThread()->DelayedDispatch(task.forget(), aTimeout);
  } else {
    layers::CompositorThread()->Dispatch(task.forget());
  }
  return handle;
}

void
EmbedLiteApp::CancelTask(void* aTask)
{
  if (aTask) {
    static_cast<CancelableRunnable*>(aTask)->Cancel();
  }
}

void
EmbedLiteApp::SetProfilePath(const char* aPath)
{
  NS_ASSERTION(mState == STOPPED,
               "SetProfilePath must be called before Start");
  free(mProfilePath);
  mProfilePath = aPath ? strdup(aPath) : nullptr;
}

EmbedLiteMessagePump*
EmbedLiteApp::CreateEmbedLiteMessagePump(
    EmbedLiteMessagePumpListener* aListener)
{
  return new EmbedLiteMessagePump(aListener);
}

bool
EmbedLiteApp::StartInternal(EmbedLiteUILoop* aLoop)
{
  if (!aLoop || mState != STOPPED || mUILoop) {
    return false;
  }

  mAtExitManager = new base::AtExitManager();
  mUILoop = aLoop;
  mShutdownScheduled = false;
  SetState(STARTING);
  mUILoop->PostTask(NewRunnableFunction(
    "mozilla::embedlite::EmbedLiteApp::StartRuntime",
    &EmbedLiteApp::StartRuntimeTask, this));
  mUILoop->StartLoop();
  return true;
}

bool
EmbedLiteApp::StartWithCustomPump(EmbedLiteMessagePump* aMessageLoop)
{
  return aMessageLoop &&
    StartInternal(aMessageLoop->GetMessageLoop());
}

bool
EmbedLiteApp::Start()
{
  EmbedLiteUILoop* loop = new EmbedLiteUILoop();
  const bool started = StartInternal(loop);
  if (!started) {
    delete loop;
    return false;
  }
  delete loop;
  if (mUILoop == loop) {
    mUILoop = nullptr;
  }
  return true;
}

void
EmbedLiteApp::StartRuntimeTask(EmbedLiteApp* aApp)
{
  if (!aApp || aApp->mState != STARTING) {
    if (aApp) {
      aApp->MaybeFinishShutdown();
    }
    return;
  }

  if (!aApp->InitializeRuntime()) {
    LOGE("Failed to initialize Gecko on the toolkit main thread");
    aApp->SetState(DESTROYING);
    aApp->MaybeFinishShutdown();
    return;
  }

  aApp->SetState(INITIALIZED);
  if (aApp->mListener) {
    aApp->mListener->Initialized();
  }

  nsCOMPtr<nsIObserverService> observers =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observers) {
    observers->NotifyObservers(nullptr, "embedliteInitialized", nullptr);
  }
}

bool
EmbedLiteApp::InitializeRuntime()
{
  MOZ_ASSERT(MessageLoop::current() == mUILoop);

  for (const nsCString& manifest : sComponentDirs) {
    nsCOMPtr<nsIFile> file;
    NS_NewNativeLocalFile(manifest, getter_AddRefs(file));
    if (!file) {
      NS_ERROR(nsPrintfCString("Invalid component manifest: %s",
                               manifest.get()).get());
      return false;
    }
    XRE_AddManifestLocation(NS_APP_LOCATION, file);
  }

  if (!GeckoLoader::InitEmbedding(mProfilePath)) {
    return false;
  }
  mEmbeddingInitialized = true;

  if (!InitializeAppServices()) {
    return false;
  }

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetBoolPref("layers.offmainthreadcomposition.enabled", true);
  }
  mObserver = new EmbedLiteAppObserver(this);
  return true;
}

bool
EmbedLiteApp::InitializeAppServices()
{
  nsCOMPtr<nsIComponentRegistrar> registrar;
  if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(registrar))) ||
      !registrar) {
    return false;
  }

  nsCOMPtr<nsIFactory> appFactory =
    new mozilla::GenericFactory(EmbedLiteAppServiceConstructor);
  nsCID appCID = NS_EMBED_LITE_APP_SERVICE_CID;
  if (NS_FAILED(registrar->RegisterFactory(
        appCID, NS_EMBED_LITE_APP_SERVICE_CLASSNAME,
        NS_EMBED_LITE_APP_CONTRACTID, appFactory))) {
    return false;
  }

  nsCOMPtr<nsIFactory> jsonFactory =
    new mozilla::GenericFactory(EmbedLiteJSONConstructor);
  nsCID jsonCID = NS_IEMBEDLITEJSON_IID;
  return NS_SUCCEEDED(registrar->RegisterFactory(
    jsonCID, NS_EMBED_LITE_JSON_SERVICE_CLASSNAME,
    NS_EMBED_LITE_JSON_CONTRACTID, jsonFactory));
}

void
EmbedLiteApp::AddManifestLocation(const char* aManifest)
{
  if (!aManifest || !*aManifest) {
    return;
  }

  if (mState != INITIALIZED) {
    sComponentDirs.AppendElement(nsDependentCString(aManifest));
    return;
  }

  nsCOMPtr<nsIFile> file;
  NS_NewNativeLocalFile(nsDependentCString(aManifest), getter_AddRefs(file));
  if (file) {
    XRE_AddManifestLocation(NS_APP_LOCATION, file);
  }
}

void
EmbedLiteApp::Stop()
{
  if (mState != STARTING && mState != INITIALIZED) {
    return;
  }

  const State oldState = mState;
  SetState(DESTROYING);
  if (oldState == INITIALIZED) {
    std::vector<EmbedLiteWindow*> windows;
    windows.reserve(mWindows.size());
    for (const auto& entry : mWindows) {
      windows.push_back(entry.second);
    }
    for (EmbedLiteWindow* window : windows) {
      window->Destroy();
    }
  }
  MaybeFinishShutdown();
}

void
EmbedLiteApp::MaybeFinishShutdown()
{
  if (mState != DESTROYING || !mWindows.empty() || mShutdownScheduled ||
      !mUILoop) {
    return;
  }
  mShutdownScheduled = true;
  mUILoop->PostTask(NewRunnableFunction(
    "mozilla::embedlite::EmbedLiteApp::FinishShutdown",
    &EmbedLiteApp::FinishShutdownTask, this));
}

void
EmbedLiteApp::FinishShutdownTask(EmbedLiteApp* aApp)
{
  if (aApp) {
    aApp->FinishShutdown();
  }
}

void
EmbedLiteApp::FinishShutdown()
{
  if (mState != DESTROYING || !mWindows.empty()) {
    mShutdownScheduled = false;
    return;
  }

  RemoveAllObservers();
  if (mObserver) {
    mObserver->Detach();
    mObserver = nullptr;
  }

  if (mEmbeddingInitialized) {
    layers::ImageBridgeChild::ShutDown();
    GeckoLoader::TermEmbedding();
    mEmbeddingInitialized = false;
  }

  EmbedLiteUILoop* loop = mUILoop;
  if (loop) {
    loop->DoQuit();
  }
  mUILoop = nullptr;
  mShutdownScheduled = false;

  delete mAtExitManager;
  mAtExitManager = nullptr;
  SetState(STOPPED);
  if (mListener) {
    mListener->Destroyed();
  }
}

void
EmbedLiteApp::SetBoolPref(const char* aName, bool aValue)
{
  if (mState != INITIALIZED || !aName) {
    return;
  }
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetBoolPref(aName, aValue);
  }
}

void
EmbedLiteApp::SetCharPref(const char* aName, const char* aValue)
{
  if (mState != INITIALIZED || !aName || !aValue) {
    return;
  }
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetCharPref(aName, nsDependentCString(aValue));
  }
}

void
EmbedLiteApp::SetIntPref(const char* aName, int aValue)
{
  if (mState != INITIALIZED || !aName) {
    return;
  }
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetIntPref(aName, aValue);
  }
}

static void
LoadStyleSheet(const char* aUri, bool aEnable, uint32_t aType)
{
  if (!aUri) {
    return;
  }
  nsCOMPtr<nsIStyleSheetService> service =
    do_GetService("@mozilla.org/content/style-sheet-service;1");
  nsCOMPtr<nsIURI> uri;
  if (!service || NS_FAILED(NS_NewURI(getter_AddRefs(uri), aUri)) || !uri) {
    return;
  }
  if (aEnable) {
    service->LoadAndRegisterSheet(uri, aType);
  } else {
    service->UnregisterSheet(uri, aType);
  }
}

void
EmbedLiteApp::LoadGlobalStyleSheet(const char* aUri, bool aEnable)
{
  if (mState == INITIALIZED) {
    LoadStyleSheet(aUri, aEnable, nsIStyleSheetService::AGENT_SHEET);
  }
}

void
EmbedLiteApp::LoadUserStyleSheet(const char* aUri, bool aEnable)
{
  if (mState == INITIALIZED) {
    LoadStyleSheet(aUri, aEnable, nsIStyleSheetService::USER_SHEET);
  }
}

void
EmbedLiteApp::SendObserve(const char* aTopic, const char16_t* aData)
{
  if (mState != INITIALIZED || !aTopic) {
    return;
  }
  nsCOMPtr<nsIObserverService> service =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (service) {
    service->NotifyObservers(nullptr, aTopic, aData);
  }
}

void
EmbedLiteApp::AddObserver(const char* aTopic)
{
  if (mState != INITIALIZED || !aTopic || !*aTopic || !mObserver ||
      std::find(mObservedTopics.begin(), mObservedTopics.end(), aTopic) !=
        mObservedTopics.end()) {
    return;
  }
  nsCOMPtr<nsIObserverService> service =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (service && NS_SUCCEEDED(service->AddObserver(
                   mObserver, aTopic, false))) {
    mObservedTopics.emplace_back(aTopic);
  }
}

void
EmbedLiteApp::RemoveObserver(const char* aTopic)
{
  if (!aTopic || !mObserver) {
    return;
  }
  auto topic = std::find(mObservedTopics.begin(), mObservedTopics.end(),
                         aTopic);
  if (topic == mObservedTopics.end()) {
    return;
  }
  nsCOMPtr<nsIObserverService> service =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (service) {
    service->RemoveObserver(mObserver, aTopic);
  }
  mObservedTopics.erase(topic);
}

void
EmbedLiteApp::AddObservers(const std::vector<std::string>& aObservers)
{
  for (const std::string& observer : aObservers) {
    AddObserver(observer.c_str());
  }
}

void
EmbedLiteApp::RemoveObservers(const std::vector<std::string>& aObservers)
{
  for (const std::string& observer : aObservers) {
    RemoveObserver(observer.c_str());
  }
}

void
EmbedLiteApp::RemoveAllObservers()
{
  nsCOMPtr<nsIObserverService> service =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (service && mObserver) {
    for (const std::string& topic : mObservedTopics) {
      service->RemoveObserver(mObserver, topic.c_str());
    }
  }
  mObservedTopics.clear();
}

EmbedLiteWindow*
EmbedLiteApp::CreateWindow(int aWidth, int aHeight,
                           EmbedLiteWindowListener* aListener)
{
  return CreateChromeWindow(aWidth, aHeight, "about:blank", aListener,
                            false);
}

EmbedLiteWindow*
EmbedLiteApp::CreateChromeWindow(int aWidth, int aHeight,
                                 const char* aInitialContentURI,
                                 EmbedLiteWindowListener* aListener,
                                 bool aPrivateBrowsing)
{
  const char* uri = aInitialContentURI && *aInitialContentURI
                      ? aInitialContentURI : "about:blank";
  return CreateWindowInternal(aWidth, aHeight, uri, aPrivateBrowsing,
                              aListener);
}

EmbedLiteWindow*
EmbedLiteApp::CreateChromeTabWindow(int aWidth, int aHeight,
                                    EmbedLiteWindowListener* aListener,
                                    bool aPrivateBrowsing)
{
  return CreateWindowInternal(aWidth, aHeight, "", aPrivateBrowsing,
                              aListener);
}

EmbedLiteWindow*
EmbedLiteApp::CreateWindowInternal(int aWidth, int aHeight,
                                   const char* aInitialContentURI,
                                   bool aPrivateBrowsing,
                                   EmbedLiteWindowListener* aListener)
{
  if (mState != INITIALIZED || aWidth <= 0 || aHeight <= 0 ||
      aWidth > UINT16_MAX || aHeight > UINT16_MAX) {
    return nullptr;
  }
  if (!aListener) {
    aListener = &sFakeWindowListener;
  }

  static uint32_t sWindowCreateID = 0;
  uint32_t id = ++sWindowCreateID;
  if (!id) {
    id = ++sWindowCreateID;
  }

  RefPtr<EmbedLiteWindowParent> parent = new EmbedLiteWindowParent(
    aWidth, aHeight, id, aListener,
    nsDependentCString(aInitialContentURI ? aInitialContentURI : ""),
    aPrivateBrowsing);
  EmbedLiteWindowParent::Register(parent);

  EmbedLiteWindow* window = new EmbedLiteWindow(this, parent, id);
  mWindows.emplace(id, window);
  parent->Initialize();
  return window;
}

void
EmbedLiteApp::WindowDestroyed(uint32_t aId)
{
  const auto found = mWindows.find(aId);
  if (found == mWindows.end()) {
    return;
  }
  EmbedLiteWindow* window = found->second;
  mWindows.erase(found);
  delete window;
  if (mWindows.empty()) {
    if (mListener) {
      mListener->LastWindowDestroyed();
    }
    MaybeFinishShutdown();
  }
}

void
EmbedLiteApp::DestroyWindow(EmbedLiteWindow* aWindow)
{
  if (!aWindow || (mState != INITIALIZED && mState != DESTROYING)) {
    return;
  }
  for (const auto& entry : mWindows) {
    if (entry.second == aWindow) {
      aWindow->Destroy();
      return;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Invalid EmbedLiteWindow pointer");
}

int
EmbedLiteApp::GetNumberOfWindows() const
{
  return mWindows.size();
}

EmbedLiteSecurity*
EmbedLiteApp::CreateSecurity(const char* aStatus, unsigned int aState) const
{
  return new EmbedLiteSecurity(aStatus, aState);
}

void
EmbedLiteApp::DestroySecurity(EmbedLiteSecurity* aSecurity) const
{
  delete aSecurity;
}

void
EmbedLiteApp::SetIsAccelerated(bool aIsAccelerated)
{
#ifdef MOZ_GL_PROVIDER
  if (aIsAccelerated) {
    mRenderType = RENDER_HW;
  } else
#endif
  {
    mRenderType = RENDER_SW;
  }
}

void
EmbedLiteApp::SetEGLDisplay(void* aDisplay)
{
  NS_ASSERTION(mState == STOPPED,
               "SetEGLDisplay must be called before Gecko starts");
  if (mState != STOPPED) {
    return;
  }
#ifdef MOZ_WIDGET_QT
  gl::SetQtEGLDisplay(aDisplay);
#else
  MOZ_ASSERT(!aDisplay);
#endif
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
