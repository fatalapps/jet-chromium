// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PLANE_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_PLANE_MANAGER_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// The Plane detection feature is not yet implemented for any OpenXR Scene
// Understanding set of extensions; however, many of the hit test
// implementations rely on the presence of some form of plane tracker or set
// of planes. This base class is thus a placeholder to help setup a future
// plane detection implementation.
class OpenXrPlaneManager {
 public:
  virtual ~OpenXrPlaneManager();
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_PLANE_MANAGER_H_
