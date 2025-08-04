// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_WRAPPER_INFO_H_
#define GIN_PUBLIC_WRAPPER_INFO_H_

#include "gin/gin_export.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"

namespace gin {

struct GIN_EXPORT WrapperInfo : v8::Object::WrapperTypeInfo {
  const WrappablePointerTag pointer_tag;
};

}  // namespace gin

#endif  // GIN_PUBLIC_WRAPPER_INFO_H_
