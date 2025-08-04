// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_WRAPPABLE_H_
#define GIN_WRAPPABLE_H_

#include <type_traits>

#include "gin/converter.h"
#include "gin/gin_export.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/v8-sandbox.h"

namespace gin {

class NamedPropertyInterceptor;

// Wrappable is a base class for C++ objects that have corresponding v8 wrapper
// objects. To retain a Wrappable object on the stack, use a gin::Handle.
//
// USAGE:
// // my_class.h
// class MyClass : Wrappable<MyClass> {
//  public:
//   static WrapperInfo kWrapperInfo;
//
//   WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }
//
//   // Optional, only required if non-empty template should be used.
//   virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
//       v8::Isolate* isolate);
//   ...
// };
//
// // my_class.cc
// WrapperInfo MyClass::kWrapperInfo = {{kEmbedderNativeGin}, kMyClass};
//
// gin::ObjectTemplateBuilder MyClass::GetObjectTemplateBuilder(
//     v8::Isolate* isolate) {
//   return Wrappable<MyClass>::GetObjectTemplateBuilder(isolate)
//       .SetValue("foobar", 42);
// }
//
// Subclasses should also typically have private constructors and expose a
// static Create function that returns a gin::Handle. Forcing creators through
// this static Create function will enforce that clients actually create a
// wrapper for the object. If clients fail to create a wrapper for a wrappable
// object, the object will leak because we use the weak callback from the
// wrapper as the signal to delete the wrapped object.
//
// Wrappable<T> explicitly does not support further subclassing of T.
// Subclasses of Wrappable<T> should be declared final. Because Wrappable<T>
// caches the object template using &T::kWrapperInfo as the key, all subclasses
// would share a single object template. This will lead to hard to debug crashes
// that look like use-after-free errors.

static_assert(sizeof(v8::CppHeapPointerTag) == sizeof(WrappablePointerTag),
              "WrappablePointerTag defines a subrange of v8::CppHeapPointerTag "
              "which is used for subclasses of gin::Wrappable. They should "
              "therefore have the same underlying type.");

class ObjectTemplateBuilder;

// Non-template base class to share code between templates instances.
class GIN_EXPORT WrappableBase : public v8::Object::Wrappable {
 public:
  WrappableBase(const WrappableBase&) = delete;
  WrappableBase& operator=(const WrappableBase&) = delete;

  void Trace(cppgc::Visitor*) const override;

  // We use a virtual getter for the WrapperInfo instead of just accessing the
  // static object because we need the virtual dispatch for a type check. If we
  // unwrap an object of the wrong type, then we detect the issue by comparing
  // the static object with the WrapperInfo from the virtual dispatch.
  virtual const WrapperInfo* wrapper_info() const = 0;

  virtual NamedPropertyInterceptor* GetNamedPropertyInterceptor();

  v8::MaybeLocal<v8::Object> GetWrapper(v8::Isolate* isolate);
  void SetWrapper(v8::Isolate* isolate, v8::Local<v8::Object> wrapper);

 protected:
  WrappableBase() = default;

  // Overrides of this method should be declared final and not overridden again.
  virtual ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate);

 private:
  void AssociateWithWrapper(v8::Isolate* isolate,
                            v8::Local<v8::Object> wrapper);
  v8::TracedReference<v8::Object> wrapper_;
};

template <typename T>
class Wrappable : public WrappableBase {
 public:
  Wrappable(const Wrappable&) = delete;
  Wrappable& operator=(const Wrappable&) = delete;

 protected:
  Wrappable() = default;
};

// This converter handles any subclass of Wrappable.
template <typename T>
  requires(std::is_convertible_v<T*, WrappableBase*>)
struct Converter<T*> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Isolate* isolate, T* val) {
    if (val == nullptr) {
      return v8::Null(isolate);
    }
    v8::Local<v8::Object> wrapper;
    if (!val->GetWrapper(isolate).ToLocal(&wrapper)) {
      return v8::MaybeLocal<v8::Value>();
    }
    return v8::MaybeLocal<v8::Value>(wrapper);
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val, T** out) {
    if (!val->IsObject()) {
      *out = nullptr;
      return false;
    }
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(val);
    if (!obj->IsApiWrapper()) {
      *out = nullptr;
      return false;
    }

    auto tag = static_cast<v8::CppHeapPointerTag>(T::kWrapperInfo.pointer_tag);
    WrappableBase* wrappable =
        v8::Object::Unwrap<WrappableBase>(isolate, obj, {tag, tag});
    if (!wrappable) {
      *out = nullptr;
      return false;
    }
    if (wrappable->wrapper_info() != &T::kWrapperInfo) {
      *out = nullptr;
      return false;
    }
    *out = static_cast<T*>(wrappable);
    return *out != nullptr;
  }
};
}  // namespace gin

#endif  // GIN_WRAPPABLE_H_
