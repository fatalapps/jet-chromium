// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_METRICS_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_METRICS_H_

namespace split_tabs {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SplitTabCreatedSource)
enum class SplitTabCreatedSource {
  kToolbarButton = 0,
  kDragAndDropLink = 1,
  kDragAndDropTab = 2,
  kBookmarkContextMenu = 3,
  kLinkContextMenu = 4,
  kTabContextMenu = 5,
  kDuplicateSplit = 6,
  // Extensions API is used to open bookmarks in Split View from the Bookmarks
  // Side Panel.
  kExtensionsApi = 7,
  kMaxValue = kExtensionsApi,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SplitTabCreatedSource)

void RecordSplitTabCreated(SplitTabCreatedSource source);
}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_METRICS_H_
