// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UTILS_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UTILS_H_

#import <Foundation/Foundation.h>

/// Returns the accessibility identifier to set on a `SafariDataItemTableView`.
NSString* GetSafariDataItemTableViewAccessibilityIdentifier();

/// Returns the accessibility identifier to set on a cell in the
/// `SafariDataItemTableView`.
NSString* GetSafariDataItemTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index);

/// Returns the accessibility identifier to set on the table view for password
/// conflict resolution.
NSString* GetPasswordConflictResolutionTableViewAccessibilityIdentifier();

/// Returns the accessibility identifier to set on a cell in the table view for
/// password conflict resolution.
NSString* GetPasswordConflictResolutionTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index);

/// Returns the accessibility identifier to set on the table view for the list
/// of invalid passwords.
NSString* GetInvalidPasswordsTableViewAccessibilityIdentifier();

/// Returns the accessibility identifier to set on a cell in the table view for
/// the list of invalid passwords.
NSString* GetInvalidPasswordsTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index);

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_UTILS_H_
