// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_SPLIT_TAB_VISUAL_DATA_TRAITS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_SPLIT_TAB_VISUAL_DATA_TRAITS_H_

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoSplitLayout = tabs_api::mojom::SplitTabVisualData_Layout;
using NativeSplitLayout = split_tabs::SplitTabLayout;

template <>
struct mojo::EnumTraits<MojoSplitLayout, NativeSplitLayout> {
  static MojoSplitLayout ToMojom(NativeSplitLayout input);
  static bool FromMojom(MojoSplitLayout input, NativeSplitLayout* out);
};

using MojoSplitTabVisualDataView = tabs_api::mojom::SplitTabVisualDataDataView;
using NativeSplitTabVisualData = split_tabs::SplitTabVisualData;

template <>
struct mojo::StructTraits<MojoSplitTabVisualDataView,
                          NativeSplitTabVisualData> {
  static NativeSplitLayout layout(const NativeSplitTabVisualData& native);
  static double split_ratio(const NativeSplitTabVisualData& native);

  // Decoder:
  static bool Read(MojoSplitTabVisualDataView view,
                   NativeSplitTabVisualData* out);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_SPLIT_TAB_VISUAL_DATA_TRAITS_H_
