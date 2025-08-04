// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_SAFE_AREA_PROVIDER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_SAFE_AREA_PROVIDER_H_

#import <UIKit/UIKit.h>

class Browser;

// Provider used to determine the safe area insets.
@interface SafeAreaProvider : NSObject

// The safe area insets object.
@property(nonatomic, readonly) UIEdgeInsets safeArea;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
- (NSDirectionalEdgeInsets)directionalEdgeInsetsForLayoutRegion:
    (UIViewLayoutRegion*)layoutRegion API_AVAILABLE(ios(26.0));
#endif

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_SAFE_AREA_PROVIDER_H_
