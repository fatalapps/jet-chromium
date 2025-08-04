// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

enum class SafariDataImportStage;
@class SafariDataItemTableView;

/// Main screen presented for `SafariDataImportImportCoordinator`.
@interface SafariDataImportImportViewController : PromoStyleViewController

/// The current import stage.
@property(nonatomic, assign) SafariDataImportStage importStage;

/// The table view  containing the import stages of each Safari data items.
@property(nonatomic, strong) SafariDataItemTableView* itemTableView;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_VIEW_CONTROLLER_H_
