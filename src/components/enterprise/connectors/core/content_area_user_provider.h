// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_AREA_USER_PROVIDER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_AREA_USER_PROVIDER_H_

#include "url/gurl.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_connectors {

// Returns email of the active Gaia user based on the values provided by
// tab url and identity manager. Only returns a value for Workspace
// sites.
// TODO(crbug.com/415002299): Add tests for this.
std::string GetActiveContentAreaUser(signin::IdentityManager* im,
                                     const GURL& tab_url);

// Returns email of the active Gaia user based on the values found in the
// provided frame URL and identity manager. Only returns a value if `tab_url` is
// a Workspace site.
std::string GetActiveFrameUser(signin::IdentityManager* im,
                               const GURL& tab_url,
                               const GURL& frame_url);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_AREA_USER_PROVIDER_H_
