// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_picker_coordinator.h"

#import "base/check.h"
#import "base/values.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mediator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_view_controller.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface HomeCustomizationBackgroundPhotoPickerCoordinator () <
    HomeCustomizationImageFramingViewControllerDelegate>
@end

@implementation HomeCustomizationBackgroundPhotoPickerCoordinator {
  // Strong reference to the framing view controller while it's being presented.
  HomeCustomizationImageFramingViewController* _framingViewController;
  // Mediator for handling background photo framing.
  HomeCustomizationBackgroundPhotoFramingMediator* _mediator;
}

- (void)start {
  [self presentPhotoPicker];
}

- (void)stop {
  // Dismiss the framing view controller if it's presented.
  if (_framingViewController) {
    [_framingViewController dismissViewControllerAnimated:YES completion:nil];
    _framingViewController = nil;
  }

  [super stop];
}

#pragma mark - Photo Library Picker

- (void)presentPhotoPicker {
  // Create a PHPickerConfiguration with appropriate filter options.
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc] init];
  // Only allow selecting one image.
  config.selectionLimit = 1;
  config.filter = [PHPickerFilter imagesFilter];

  // Create the picker view controller.
  PHPickerViewController* pickerViewController =
      [[PHPickerViewController alloc] initWithConfiguration:config];
  pickerViewController.delegate = self;

  // Present the picker.
  [self.baseViewController presentViewController:pickerViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  // Dismiss the picker.
  [picker dismissViewControllerAnimated:YES completion:nil];

  // Check if the user selected an image.
  if (results.count > 0) {
    PHPickerResult* result = results.firstObject;

    __weak __typeof(self) weakSelf = self;
    // Load the selected image.
    [result.itemProvider loadObjectOfClass:[UIImage class]
                         completionHandler:^(UIImage* image, NSError* error) {
                           if (image) {
                             CHECK(image);
                             dispatch_async(dispatch_get_main_queue(), ^{
                               [weakSelf handleSelectedImage:image];
                             });
                           }
                         }];
  } else {
    // User cancelled without selecting an image.
    [self.delegate photoPickerCoordinatorDidCancel:self];
  }
}

#pragma mark - Private

// Handles the selected image and presents the framing view.
- (void)handleSelectedImage:(UIImage*)image {
  if (!_mediator) {
    HomeBackgroundCustomizationService* backgroundService =
        HomeBackgroundCustomizationServiceFactory::GetForProfile(self.profile);
    _mediator = [[HomeCustomizationBackgroundPhotoFramingMediator alloc]
         initWithFilePath:self.profile->GetStatePath()
        backgroundService:backgroundService];
  }

  // Create the logo vendor
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  SearchEngineLogoMediator* searchEngineLogoMediator =
      [[SearchEngineLogoMediator alloc] initWithBrowser:self.browser
                                               webState:webState];

  // Create the framing view controller.
  _framingViewController = [[HomeCustomizationImageFramingViewController alloc]
                 initWithImage:image
      searchEngineLogoMediator:searchEngineLogoMediator];

  _framingViewController.mutator = _mediator;
  _framingViewController.delegate = self;

  _framingViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _framingViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  // Present the framing interface.
  [self.baseViewController presentViewController:_framingViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - HomeCustomizationImageFramingViewControllerDelegate

- (void)imageFramingViewControllerDidCancel:
    (HomeCustomizationImageFramingViewController*)controller {
  // Dismiss the framing view controller.
  __weak __typeof(self) weakSelf = self;
  [controller
      dismissViewControllerAnimated:YES
                         completion:^{
                           // Notify delegate of cancellation.
                           [weakSelf.delegate
                               photoPickerCoordinatorDidCancel:weakSelf];
                         }];

  _framingViewController = nil;
}

- (void)imageFramingViewControllerDidSucceed:
    (HomeCustomizationImageFramingViewController*)controller {
  [self.delegate photoPickerCoordinatorDidFinish:self];
}

@end
