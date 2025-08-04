// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_H_

#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/renderer/modules/remote_objects/remote_object_gateway_impl.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "v8/include/cppgc/prefinalizer.h"

namespace blink {

// Gin wrapper for representing objects that could be injected by the browser.
// Recreated every time the window object is cleared.
class RemoteObject
    : public gin::WrappableWithNamedPropertyInterceptor<RemoteObject> {
  USING_PRE_FINALIZER(RemoteObject, Dispose);

 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kRemoteObject};

  RemoteObject(RemoteObjectGatewayImpl*, int32_t);
  // Not copyable or movable
  RemoteObject(const RemoteObject&) = delete;
  RemoteObject& operator=(const RemoteObject&) = delete;
  ~RemoteObject() override = default;

  void Trace(cppgc::Visitor* visitor) const override;

  const gin::WrapperInfo* wrapper_info() const override {
    return &kWrapperInfo;
  }
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override;
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override;

  int32_t object_id() const { return object_id_; }

 private:
  static void RemoteObjectInvokeCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  void Dispose();
  void EnsureRemoteIsBound();

  WeakMember<RemoteObjectGatewayImpl> gateway_{nullptr};
  HeapMojoRemote<mojom::blink::RemoteObject> object_;
  int32_t object_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTE_OBJECTS_REMOTE_OBJECT_H_
