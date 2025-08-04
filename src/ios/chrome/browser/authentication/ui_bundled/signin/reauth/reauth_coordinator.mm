// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/reauth/reauth_coordinator.h"

#import <string>
#import <variant>

#import "absl/functional/overload.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

@implementation ReauthCoordinator {
  raw_ptr<Browser> _browser;
  CoreAccountInfo _account;
  std::variant<signin_metrics::ReauthAccessPoint, signin_metrics::AccessPoint>
      _accessPoint;
  id<SystemIdentityInteractionManager> _identityInteractionManager;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   account:(const CoreAccountInfo&)account
                         reauthAccessPoint:
                             (signin_metrics::ReauthAccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _account = account;
    _accessPoint = accessPoint;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   account:(const CoreAccountInfo&)account
                         signinAccessPoint:
                             (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _account = account;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_identityInteractionManager, base::NotFatalUntil::M144);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kStarted];

  _identityInteractionManager = GetApplicationContext()
                                    ->GetSystemIdentityManager()
                                    ->CreateInteractionManager();
  __weak __typeof(self) weakSelf = self;
  [_identityInteractionManager
      startAuthActivityWithViewController:self.baseViewController
                                userEmail:base::SysUTF8ToNSString(
                                              _account.email)
                               completion:^(id<SystemIdentity> identity,
                                            NSError* error) {
                                 [weakSelf
                                     operationCompletedWithIdentity:identity
                                                              error:error];
                               }];
}

- (void)stop {
  if (_identityInteractionManager) {
    // The operation hasn't finished yet - cancel.
    [_identityInteractionManager cancelAuthActivityAnimated:NO];
    _identityInteractionManager = nil;
    [self.delegate reauthFinishedWithResult:ReauthResult::kInterrupted];
    [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kInterrupted];
  }

  [super stop];
}

#pragma mark - Private

// Propagates the result to the delegate.
- (void)operationCompletedWithIdentity:(id<SystemIdentity>)identity
                                 error:(NSError*)error {
  CHECK(_identityInteractionManager);
  _identityInteractionManager = nil;

  ReauthResult result;
  if (!error) {
    GaiaId id = GaiaId(identity.gaiaID);
    if (id == _account.gaia) {
      result = ReauthResult::kSuccess;
      [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kCompleted];
    } else {
      result = ReauthResult::kCancelledByUser;
      [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kCancelled];
    }
  } else if (ShouldHandleSigninError(error)) {
    result = ReauthResult::kError;
    [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kError];
  } else {
    result = ReauthResult::kCancelledByUser;
    [self recordReauthFlowEvent:signin_metrics::ReauthFlowEvent::kCancelled];
  }

  [self.delegate reauthFinishedWithResult:result];
}

- (void)recordReauthFlowEvent:(signin_metrics::ReauthFlowEvent)event {
  std::visit(absl::Overload{
                 [event](signin_metrics::ReauthAccessPoint ap) {
                   signin_metrics::RecordReauthFlowEventInExplicitFlow(ap,
                                                                       event);
                 },
                 [event](signin_metrics::AccessPoint ap) {
                   signin_metrics::RecordReauthFlowEventInSigninFlow(ap, event);
                 },
             },
             _accessPoint);
}

@end
