// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARTIAL_TRANSLATE_UI_BUNDLED_PARTIAL_TRANSLATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PARTIAL_TRANSLATE_UI_BUNDLED_PARTIAL_TRANSLATE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"

@protocol BrowserCoordinatorCommands;
@protocol EditMenuAlertDelegate;
class FullscreenController;
class PrefService;

// Mediator that mediates between the browser container views and the
// partial translate tab helpers.
@interface PartialTranslateMediator : NSObject <EditMenuBuilder>

// Initializer for a mediator.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                               prefService:(PrefService*)prefs
                      fullscreenController:
                          (FullscreenController*)fullscreenController
                                 incognito:(BOOL)incognito
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)shutdown;

// The handler for BrowserCoordinator commands (to trigger full page translate).
@property(nonatomic, weak) id<BrowserCoordinatorCommands> browserHandler;

// The delegate to present error message alerts.
@property(nonatomic, weak) id<EditMenuAlertDelegate> alertDelegate;

// Handles the partial translate menu item selection.
// Used for testing to bypass the UIDeferredMenuElement logic.
- (void)handlePartialTranslateSelectionForTestingInWebState:
    (web::WebState*)webState;

// Returns whether a partial translate can be handled.
- (BOOL)canHandlePartialTranslateSelectionInWebState:(web::WebState*)webState;

// Whether partial translate action should be proposed (independently of the
// current selection).
- (BOOL)shouldInstallPartialTranslate;

@end

#endif  // IOS_CHROME_BROWSER_PARTIAL_TRANSLATE_UI_BUNDLED_PARTIAL_TRANSLATE_MEDIATOR_H_
