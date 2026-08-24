/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/CallArgs.h"
#include "js/PropertyAndElement.h"
#include "jsapi.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "nsFrameMessageManager.h"
#include "utils/EmbedLiteMessageSerialization.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::embedlite;

namespace {

nsTArray<nsCString>* sDeliveryOrder;

class AutoDeliveryOrder final {
 public:
  explicit AutoDeliveryOrder(nsTArray<nsCString>& aOrder) {
    MOZ_RELEASE_ASSERT(!sDeliveryOrder);
    sDeliveryOrder = &aOrder;
  }

  ~AutoDeliveryOrder() { sDeliveryOrder = nullptr; }
};

bool RecordViewDelivery(JSContext*, unsigned aArgc, JS::Value* aVp) {
  sDeliveryOrder->AppendElement("view"_ns);
  JS::CallArgsFromVp(aArgc, aVp).rval().setInt32(1);
  return true;
}

bool RecordGlobalDelivery(JSContext*, unsigned aArgc, JS::Value* aVp) {
  sDeliveryOrder->AppendElement("global"_ns);
  JS::CallArgsFromVp(aArgc, aVp).rval().setInt32(2);
  return true;
}

already_AddRefed<MessageListener> CreateMessageListener(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JSNative aNative) {
  JS::Rooted<JSObject*> listenerObject(aCx, JS_NewPlainObject(aCx));
  if (!listenerObject ||
      !JS_DefineFunction(aCx, listenerObject, "receiveMessage", aNative, 1,
                         0)) {
    return nullptr;
  }

  RefPtr<MessageListener> listener =
      new MessageListener(listenerObject, aGlobal, nullptr, nullptr);
  return listener.forget();
}

class RecordingCallback final : public MessageManagerCallback {
 public:
  explicit RecordingCallback(nsTArray<nsCString>* aOrder = nullptr)
      : mOrder(aOrder), mSawTransferables(false) {}

  bool WantsMessageReceived() const override { return true; }

  void DoReceiveMessage(
      JSContext* aCx, const nsAString& aMessage, bool aIsSync,
      JS::Handle<JS::Value> aData, bool aHasTransferables,
      nsTArray<NotNull<RefPtr<StructuredCloneData>>>* aRetVal) override {
    mMessages.AppendElement(aMessage);
    mSawTransferables |= aHasTransferables;

    nsAutoString json;
    if (!ConvertMessageToJSON(aCx, aData, aHasTransferables, json)) {
      return;
    }

    if (mOrder) {
      mOrder->AppendElement("embedlite"_ns);
    }
    mJSON.AppendElement(json);
    if (aIsSync && aRetVal) {
      AppendJSONReplies(aCx, mJSONReplies, *aRetVal);
    }
  }

  nsTArray<nsString> mMessages;
  nsTArray<nsString> mJSON;
  nsTArray<nsString> mJSONReplies;
  nsTArray<nsCString>* mOrder;
  bool mSawTransferables;
};

class MessageManagerHarness final {
 public:
  explicit MessageManagerHarness(nsTArray<nsCString>* aOrder = nullptr)
      : mCallback(aOrder),
        mGlobal(nsFrameMessageManager::GetGlobalMessageManager()),
        mView(new ChromeMessageSender(mGlobal)) {
    mView->InitWithCallback(&mCallback);
  }

  ~MessageManagerHarness() {
    for (const Registration& registration : mRegistrations) {
      IgnoredErrorResult error;
      registration.mManager->RemoveMessageListener(
          registration.mName, *registration.mListener, error);
    }
    mView->Disconnect();
  }

  void AddListener(nsFrameMessageManager* aManager, const nsAString& aName,
                   MessageListener* aListener) {
    IgnoredErrorResult error;
    aManager->AddMessageListener(aName, *aListener, false, error);
    mRegistrations.AppendElement(
        Registration{aManager, nsString(aName), aListener});
  }

  RecordingCallback mCallback;
  RefPtr<ChromeMessageBroadcaster> mGlobal;
  RefPtr<ChromeMessageSender> mView;

 private:
  struct Registration {
    RefPtr<nsFrameMessageManager> mManager;
    nsString mName;
    RefPtr<MessageListener> mListener;
  };

  nsTArray<Registration> mRegistrations;
};

NotNull<RefPtr<StructuredCloneData>> CloneValue(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    JS::Handle<JS::Value> aTransfer = JS::UndefinedHandleValue) {
  auto data = MakeNotNull<RefPtr<StructuredCloneData>>(
      JS::StructuredCloneScope::DifferentProcess,
      aTransfer.isUndefined()
          ? StructuredCloneHolder::TransferringNotSupported
          : StructuredCloneHolder::TransferringSupported);
  IgnoredErrorResult error;
  data->Write(aCx, aValue, aTransfer, JS::CloneDataPolicy(), error);
  MOZ_RELEASE_ASSERT(!error.Failed());
  return data;
}

class QueuedMessage final : public nsSameProcessAsyncMessageBase,
                            public SameProcessMessageQueue::Runnable {
 public:
  explicit QueuedMessage(ChromeMessageSender* aManager)
      : mManager(aManager) {}

  nsresult HandleMessage() override {
    ReceiveMessage(static_cast<EventTarget*>(mManager.get()), nullptr,
                   mManager);
    return NS_OK;
  }

 private:
  RefPtr<ChromeMessageSender> mManager;
};

void QueueMessage(ChromeMessageSender* aManager, const nsAString& aName,
                  NotNull<StructuredCloneData*> aData) {
  RefPtr<QueuedMessage> message = new QueuedMessage(aManager);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(message->Init(aName, aData)));
  SameProcessMessageQueue::Get()->Push(message);
}

}  // namespace

TEST(EmbedLiteMessageSerializationTest, HierarchyDeduplicatesAndOrdersReplies) {
  JS::Rooted<JSObject*> global(
      RootingCx(), SimpleGlobalObject::Create(
                       SimpleGlobalObject::GlobalType::BindingDetail));
  AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(global));
  JSContext* cx = jsAPI.cx();

  nsTArray<nsCString> order;
  AutoDeliveryOrder autoOrder(order);
  MessageManagerHarness managers(&order);
  RefPtr<MessageListener> viewListener =
      CreateMessageListener(cx, global, RecordViewDelivery);
  RefPtr<MessageListener> globalListener =
      CreateMessageListener(cx, global, RecordGlobalDelivery);
  ASSERT_TRUE(viewListener);
  ASSERT_TRUE(globalListener);

  constexpr auto messageName = u"EmbedLiteTest:Hierarchy"_ns;
  managers.AddListener(managers.mView, messageName, viewListener);
  managers.AddListener(managers.mView, messageName, viewListener);
  managers.AddListener(managers.mGlobal, messageName, globalListener);
  managers.mCallback.mJSONReplies.AppendElement(u"3"_ns);
  managers.mCallback.mJSONReplies.AppendElement(u"4"_ns);

  JS::Rooted<JS::Value> value(cx, JS::Int32Value(0));
  auto data = CloneValue(cx, value);
  nsTArray<NotNull<RefPtr<StructuredCloneData>>> replies;
  managers.mView->ReceiveMessage(
      static_cast<EventTarget*>(managers.mView.get()), nullptr, messageName,
      true, data, &replies);

  ASSERT_EQ(order.Length(), 3u);
  EXPECT_TRUE(order[0].EqualsLiteral("view"));
  EXPECT_TRUE(order[1].EqualsLiteral("global"));
  EXPECT_TRUE(order[2].EqualsLiteral("embedlite"));
  ASSERT_EQ(managers.mCallback.mMessages.Length(), 1u);
  ASSERT_EQ(managers.mCallback.mJSON.Length(), 1u);
  EXPECT_TRUE(managers.mCallback.mJSON[0].EqualsLiteral("0"));
  ASSERT_EQ(replies.Length(), 4u);
  EXPECT_NE(replies[2].get(), replies[3].get());
  for (size_t i = 0; i < replies.Length(); ++i) {
    JS::Rooted<JS::Value> reply(cx);
    IgnoredErrorResult error;
    replies[i]->Read(cx, &reply, error);
    ASSERT_FALSE(error.Failed());
    ASSERT_TRUE(reply.isInt32());
    EXPECT_EQ(reply.toInt32(), static_cast<int32_t>(i + 1));
  }
}

TEST(EmbedLiteMessageSerializationTest, AsyncDeliveryIsDeferredAndFIFO) {
  JS::Rooted<JSObject*> global(
      RootingCx(), SimpleGlobalObject::Create(
                       SimpleGlobalObject::GlobalType::BindingDetail));
  AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(global));
  JSContext* cx = jsAPI.cx();
  MessageManagerHarness managers;

  JS::Rooted<JS::Value> value(cx, JS::Int32Value(0));
  QueueMessage(managers.mView, u"EmbedLiteTest:First"_ns,
               CloneValue(cx, value));
  QueueMessage(managers.mView, u"EmbedLiteTest:Second"_ns,
               CloneValue(cx, value));
  EXPECT_TRUE(managers.mCallback.mMessages.IsEmpty());

  SameProcessMessageQueue::Get()->Flush();
  ASSERT_EQ(managers.mCallback.mMessages.Length(), 2u);
  EXPECT_TRUE(
      managers.mCallback.mMessages[0].EqualsLiteral("EmbedLiteTest:First"));
  EXPECT_TRUE(
      managers.mCallback.mMessages[1].EqualsLiteral("EmbedLiteTest:Second"));
}

TEST(EmbedLiteMessageSerializationTest,
     UnsupportedJSONDoesNotSuppressManagerDelivery) {
  JS::Rooted<JSObject*> global(
      RootingCx(), SimpleGlobalObject::Create(
                       SimpleGlobalObject::GlobalType::BindingDetail));
  AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(global));
  JSContext* cx = jsAPI.cx();

  nsTArray<nsCString> order;
  AutoDeliveryOrder autoOrder(order);
  MessageManagerHarness managers(&order);
  RefPtr<MessageListener> viewListener =
      CreateMessageListener(cx, global, RecordViewDelivery);
  RefPtr<MessageListener> globalListener =
      CreateMessageListener(cx, global, RecordGlobalDelivery);
  ASSERT_TRUE(viewListener);
  ASSERT_TRUE(globalListener);

  constexpr auto messageName = u"EmbedLiteTest:Cyclic"_ns;
  managers.AddListener(managers.mView, messageName, viewListener);
  managers.AddListener(managers.mGlobal, messageName, globalListener);

  JS::Rooted<JSObject*> cyclic(cx, JS_NewPlainObject(cx));
  ASSERT_TRUE(cyclic);
  ASSERT_TRUE(JS_DefineProperty(cx, cyclic, "self", cyclic, JSPROP_ENUMERATE));
  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*cyclic));
  auto data = CloneValue(cx, value);
  managers.mView->ReceiveMessage(
      static_cast<EventTarget*>(managers.mView.get()), nullptr, messageName,
      false, data, nullptr);

  ASSERT_EQ(order.Length(), 2u);
  EXPECT_TRUE(order[0].EqualsLiteral("view"));
  EXPECT_TRUE(order[1].EqualsLiteral("global"));
  ASSERT_EQ(managers.mCallback.mMessages.Length(), 1u);
  EXPECT_TRUE(managers.mCallback.mJSON.IsEmpty());
}

TEST(EmbedLiteMessageSerializationTest,
     TransferableDoesNotReachJSONBridge) {
  JS::Rooted<JSObject*> global(
      RootingCx(), SimpleGlobalObject::Create(
                       SimpleGlobalObject::GlobalType::BindingDetail));
  AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(global));
  JSContext* cx = jsAPI.cx();

  nsTArray<nsCString> order;
  AutoDeliveryOrder autoOrder(order);
  MessageManagerHarness managers(&order);
  RefPtr<MessageListener> viewListener =
      CreateMessageListener(cx, global, RecordViewDelivery);
  RefPtr<MessageListener> globalListener =
      CreateMessageListener(cx, global, RecordGlobalDelivery);
  ASSERT_TRUE(viewListener);
  ASSERT_TRUE(globalListener);

  constexpr auto messageName = u"EmbedLiteTest:Transferable"_ns;
  managers.AddListener(managers.mView, messageName, viewListener);
  managers.AddListener(managers.mGlobal, messageName, globalListener);

  JS::Rooted<JSObject*> buffer(cx, JS::NewArrayBuffer(cx, 8));
  ASSERT_TRUE(buffer);
  JS::Rooted<JSObject*> payload(cx, JS_NewPlainObject(cx));
  ASSERT_TRUE(payload);
  ASSERT_TRUE(JS_DefineProperty(cx, payload, "buffer", buffer,
                                JSPROP_ENUMERATE));
  JS::Rooted<JSObject*> transfer(cx, JS::NewArrayObject(cx, 1));
  ASSERT_TRUE(transfer);
  JS::Rooted<JS::Value> bufferValue(cx, JS::ObjectValue(*buffer));
  ASSERT_TRUE(JS_SetElement(cx, transfer, 0, bufferValue));
  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*payload));
  JS::Rooted<JS::Value> transferValue(cx, JS::ObjectValue(*transfer));
  auto data = CloneValue(cx, value, transferValue);
  managers.mView->ReceiveMessage(
      static_cast<EventTarget*>(managers.mView.get()), nullptr, messageName,
      false, data, nullptr);

  ASSERT_EQ(order.Length(), 2u);
  EXPECT_TRUE(order[0].EqualsLiteral("view"));
  EXPECT_TRUE(order[1].EqualsLiteral("global"));
  ASSERT_EQ(managers.mCallback.mMessages.Length(), 1u);
  EXPECT_TRUE(managers.mCallback.mSawTransferables);
  EXPECT_TRUE(managers.mCallback.mJSON.IsEmpty());
}
