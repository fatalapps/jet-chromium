// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/background_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Symbol size for symbols on the keyboard accessory.
constexpr CGFloat kSymbolImagePointSize = 24;

// Checkmark symbol size.
constexpr CGFloat kCheckmarkSymbolPointSize = 17;

// Default height for the keyboard accessory.
constexpr CGFloat kDefaultAccessoryHeight = 44;

// Button target area for the large keyboard accessory.
constexpr CGFloat kLargeButtonTargetArea = 44;

// Trailing horizontal padding.
constexpr CGFloat kKeyboardHorizontalPadding = 16;

// The padding between the image and the title on the manual fill button.
// Only applies to the iPad version of this button.
constexpr CGFloat kManualFillTitlePadding = 4;

// The font size used for the title of the manual fill button.
// Only applies to the iPad version of this button.
constexpr CGFloat kManualFillTitleFontSize = 18;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The leading content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeadingInset = 7;

// The trailing content inset for the close button.
constexpr CGFloat ManualFillCloseButtonTrailingInset = 15;

// The trailing content inset for the close button when using liquid glass.
constexpr CGFloat LiquidGlassCloseButtonTrailingInset = 24;

// The bottom content inset for the close button.
constexpr CGFloat ManualFillCloseButtonBottomInset = 4;

// The height for the top and bottom separator lines.
constexpr CGFloat ManualFillSeparatorHeight = 0.5;

// Symbols used by FormInputAccessoryView.
NSString* const kCheckmarkSymbol = @"checkmark";
NSString* const kChevronDownSymbol = @"chevron.down";
NSString* const kChevronUpSymbol = @"chevron.up";

// Returns the image of the symbol with the provided name.
UIImage* SymbolNamed(NSString* imageName) {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  return [UIImage systemImageNamed:imageName withConfiguration:configuration];
}

#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0

// Padding around the keyboard accessory on iOS 26.0+.
constexpr CGFloat kSurroundingPadding = 12;

// Corner radius of the keyboard accessory on iOS 26.0+.
constexpr CGFloat kCornerRadius = 24;

// Width for the small keyboard accessory. Only when liquid glass effect is
// enabled.
constexpr CGFloat kSmallAccessoryWidth = 3 * kLargeButtonTargetArea + 4;

// Alpha of the tint color for the glass effect. A lower alpha will produce a
// more pronounced glass effect.
constexpr CGFloat kGlassTintAlpha = 0.5;

// Shadow parameters. Used when the liquid glass effect is enabled.
constexpr CGFloat kShadowRadius = 16.0;
constexpr CGFloat kShadowVerticalOffset = 4.0;
constexpr CGFloat kShadowOpacity = 0.12;

#endif  // defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_26_0

}  // namespace

// Large height for the keyboard accessory.
const CGFloat kLargeKeyboardAccessoryHeight = 59;

NSString* const kFormInputAccessoryViewAccessibilityID =
    @"kFormInputAccessoryViewAccessibilityID";

NSString* const kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID =
    @"kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID";

@interface FormInputAccessoryView ()

// The navigation delegate if any.
@property(nonatomic, weak) id<FormInputAccessoryViewDelegate> delegate;

@property(nonatomic, weak) UIButton* previousButton;

@property(nonatomic, weak) UIButton* nextButton;

@property(nonatomic, weak) UIButton* manualFillButton;

@property(nonatomic, weak) UIButton* passwordManualFillButton;

@property(nonatomic, weak) UIButton* creditCardManualFillButton;

@property(nonatomic, weak) UIButton* addressManualFillButton;

@property(nonatomic, weak) UIView* leadingView;

@property(nonatomic, weak) UIView* trailingView;

@property(nonatomic, strong) UIImage* manualFillSymbol;

@property(nonatomic, strong) UIImage* passwordManualFillSymbol;

@property(nonatomic, strong) UIImage* creditCardManualFillSymbol;

@property(nonatomic, strong) UIImage* addressManualFillSymbol;

@property(nonatomic, strong) UIImage* closeButtonSymbol;

@end

@implementation FormInputAccessoryView {
  // Transparent view on the top edge to let the keyboard know about the
  // omnibox.
  UIButton* _omniboxTypingShield;
  // Height constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldHeightConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldBottomConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield` when the view
  // is hidden.
  NSLayoutConstraint* _omniboxTypingShieldHiddenBottomConstraint;
  // View containing the leading and trailing buttons.
  UIView* _contentView;
  // The view used as the background for the content view.
  UIView* _backgroundView;
  // Whether we are using the large accessory view.
  BOOL _largeAccessoryViewEnabled;
  // Whether we are using the small width accessory view.
  BOOL _smallWidthAccessoryViewEnabled;
  // Whether the current form factor is a tablet.
  BOOL _isTabletFormFactor;
  // Whether the size of the accessory is compact.
  BOOL _isCompact;
  // Trailing constraint in non compact mode (tablet only).
  NSLayoutConstraint* _trailingConstraint;
  // Trailing constraint in compact mode (tablet only).
  NSLayoutConstraint* _compactTrailingConstraint;
}

#pragma mark - Public

// Override `intrinsicContentSize` so Auto Layout hugs the content of this view.
- (CGSize)intrinsicContentSize {
  return CGSizeZero;
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView {
  [self setSmallWidthAccessoryViewEnabled:!leadingView];
  [self setUpWithLeadingView:leadingView
              customTrailingView:customTrailingView
              navigationDelegate:nil
                manualFillSymbol:nil
        passwordManualFillSymbol:nil
      creditCardManualFillSymbol:nil
         addressManualFillSymbol:nil
               closeButtonSymbol:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  [self setSmallWidthAccessoryViewEnabled:!leadingView];
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:nil
        passwordManualFillSymbol:nil
      creditCardManualFillSymbol:nil
         addressManualFillSymbol:nil
               closeButtonSymbol:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol
            isTabletFormFactor:(BOOL)isTabletFormFactor {
  DCHECK(manualFillSymbol);
  _largeAccessoryViewEnabled = YES;
  _isTabletFormFactor = isTabletFormFactor;
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:manualFillSymbol
        passwordManualFillSymbol:passwordManualFillSymbol
      creditCardManualFillSymbol:creditCardManualFillSymbol
         addressManualFillSymbol:addressManualFillSymbol
               closeButtonSymbol:closeButtonSymbol];
}

- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight {
  _omniboxTypingShieldHeightConstraint.constant = typingShieldHeight;
  if (self.window) {
    [self layoutIfNeeded];
  }
}

- (void)setIsCompact:(BOOL)isCompact {
  if (_isCompact == isCompact) {
    return;
  }

  _isCompact = isCompact;
  [self adjustManualFillButtonTitle:self.manualFillButton];
  [self setHorizontalConstraints];
}

#pragma mark - UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark - Private Methods

// Whether the liquid glass effect is enabled. Restricted to iOS 26+.
- (BOOL)isLiquidGlassEffectEnabled {
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    if (_largeAccessoryViewEnabled || _smallWidthAccessoryViewEnabled) {
      return YES;
    }
  }
#endif  // defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_26_0

  return NO;
}

// Sets the small width mode. This mode is always disabled on iOS < 26.
- (void)setSmallWidthAccessoryViewEnabled:(BOOL)enabled {
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    _smallWidthAccessoryViewEnabled = enabled;
    return;
  }
#endif  // defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_26_0

  _smallWidthAccessoryViewEnabled = NO;
}

- (void)closeButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCloseButton:self];
}

- (void)nextButtonTapped {
  [self.delegate formInputAccessoryViewDidTapNextButton:self];
}

- (void)previousButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPreviousButton:self];
}

- (void)manualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapManualFillButton:self];
}

- (void)passwordManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPasswordManualFillButton:self];
}

- (void)creditCardManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCreditCardManualFillButton:self];
}

- (void)addressManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapAddressManualFillButton:self];
}

- (void)omniboxTypingShieldTapped {
  [self.delegate fromInputAccessoryViewDidTapOmniboxTypingShield:self];
}

// Sets up the view with the given `leadingView`. If `delegate` is not nil,
// navigation controls are shown on the right and use `delegate` for actions.
// Else navigation controls are replaced with `customTrailingView`. If none of
// `delegate` and `customTrailingView` is set, leadingView will take all the
// space.
- (void)setUpWithLeadingView:(UIView*)leadingView
            customTrailingView:(UIView*)customTrailingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol {
  DCHECK(!self.subviews.count);  // This should only be called once.

  self.accessibilityIdentifier = kFormInputAccessoryViewAccessibilityID;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = UIColor.clearColor;
  self.opaque = NO;
  self.manualFillSymbol = manualFillSymbol;
  self.passwordManualFillSymbol = passwordManualFillSymbol;
  self.creditCardManualFillSymbol = creditCardManualFillSymbol;
  self.addressManualFillSymbol = addressManualFillSymbol;
  self.closeButtonSymbol = closeButtonSymbol;

  // Attempt to set up the liquid glass effect, otherwise, use the non liquid
  // glass accessory.
  if (![self setupLiquidGlassEffect]) {
    _contentView = [[UIView alloc] init];
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    if (_largeAccessoryViewEnabled) {
      _backgroundView = PrimaryBackgroundBlurView();
      _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
      [_contentView addSubview:_backgroundView];
      AddSameConstraints(_backgroundView, _contentView);
    } else {
      _contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    }
    [self addSubview:_contentView];
    AddSameConstraintsToSides(
        self, _contentView,
        LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

    [self setOmniboxSafeTopConstraint:_contentView];
  }

  leadingView = leadingView ?: [[UIView alloc] init];
  self.leadingView = leadingView;
  leadingView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* trailingView;
  if (delegate) {
    self.delegate = delegate;
    trailingView = [self viewForNavigationButtons];
  } else {
    trailingView = customTrailingView;
  }
  self.trailingView = trailingView;

  // If there is no trailing view, set the leading view as the only view and
  // return early.
  if (!trailingView) {
    [_contentView addSubview:leadingView];
    AddSameConstraints(_contentView, leadingView);
    return;
  }

  UIView* leadingViewContainer = [[UIView alloc] init];
  leadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self clipToLiquidGlassEffectBounds:leadingViewContainer];
  [_contentView addSubview:leadingViewContainer];
  [leadingViewContainer addSubview:leadingView];
  if ([self isLiquidGlassEffectEnabled]) {
    AddSameConstraintsToSides(
        leadingViewContainer, leadingView,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    [self setBottomAnchorForView:leadingView];
  } else {
    AddSameConstraints(leadingViewContainer, leadingView);
  }

  trailingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addTrailingView:trailingView];

  [self setDefaultHeightConstraint:_contentView];

  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [leadingViewContainer.topAnchor
        constraintEqualToAnchor:_contentView.topAnchor],
    [leadingViewContainer.leadingAnchor
        constraintEqualToAnchor:[self isLiquidGlassEffectEnabled]
                                    ? _contentView.leadingAnchor
                                    : layoutGuide.leadingAnchor],
    [trailingView.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
  ]];

  [self setBottomAnchorForView:leadingViewContainer];
  [self setBottomAnchorForView:trailingView];

  if (_isTabletFormFactor && _largeAccessoryViewEnabled) {
    // On tablets, when using the large keyboard accessory, add padding at both
    // ends of the content view to match the keyboard's padding.

    // Trailing constraint in non compact mode.
    _trailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor
                       constant:-kKeyboardHorizontalPadding];
    // Trailing constraint in compact mode.
    _compactTrailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor];

    // When using multiple windows, ensure that the out of focus window keeps a
    // minimum top anchor preventing the keyboard accessory from being reduced
    // in height. This is only relevant on tablets.
    [self.topAnchor
        constraintLessThanOrEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                                 constant:-[self accessoryHeight]]
        .active = YES;

    [self setHorizontalConstraints];
  } else {
    [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor]
        .active = YES;
  }

  // When using the blur effect background, do not add top and bottom lines.
  if (!_largeAccessoryViewEnabled && ![self isLiquidGlassEffectEnabled]) {
    UIView* topGrayLine = [[UIView alloc] init];
    topGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
    topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:topGrayLine];

    UIView* bottomGrayLine = [[UIView alloc] init];
    bottomGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
    bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:bottomGrayLine];

    [NSLayoutConstraint activateConstraints:@[
      [topGrayLine.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
      [topGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [topGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [topGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [bottomGrayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [bottomGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [bottomGrayLine.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [bottomGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],
    ]];
  }

  [leadingViewContainer.trailingAnchor
      constraintEqualToAnchor:trailingView.leadingAnchor]
      .active = YES;

  [self createOmniboxTypingShield];
}

// If the liquid glass effect is enabled, clip the contents of the leading view
// to the liquid glass effect's bounds.
- (void)clipToLiquidGlassEffectBounds:(UIView*)leadingViewContainer {
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    if ([self isLiquidGlassEffectEnabled]) {
      // Set leading view container bounds to match the glass effect.
      leadingViewContainer.clipsToBounds = YES;
      leadingViewContainer.cornerConfiguration = [UICornerConfiguration
          configurationWithRadius:
              [UICornerRadius
                  containerConcentricRadiusWithMinimum:kCornerRadius]];
      leadingViewContainer.layer.maskedCorners =
          kCALayerMinXMaxYCorner | kCALayerMinXMinYCorner;
    }
  }
#endif  // defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_26_0
}

// Adds the trailing view in the accessory's view hierarchy.
- (void)addTrailingView:(UIView*)trailingView {
  // When the liquid glass effect is enabled, all views must be subviews of the
  // content view so that the glass effect can apply properly to all views.
  if ([self isLiquidGlassEffectEnabled]) {
    [_contentView addSubview:trailingView];
  } else {
    [self addSubview:trailingView];
  }
}

// Sets a top constraint which is bottom omnibox safe.
- (void)setOmniboxSafeTopConstraint:(UIView*)view {
  // Lower the top constraint as the omniboxTypingShield can be above it.
  NSLayoutConstraint* topConstraint =
      [self.topAnchor constraintEqualToAnchor:view.topAnchor];
  topConstraint.priority = UILayoutPriorityRequired - 1;
  topConstraint.active = YES;
}

// Sets the height constraint of the entire keyboard accessory.
- (void)setDefaultHeightConstraint:(UIView*)view {
  NSLayoutConstraint* defaultHeightConstraint =
      [view.heightAnchor constraintEqualToConstant:[self accessoryHeight]];
  defaultHeightConstraint.priority = UILayoutPriorityDefaultHigh;
  defaultHeightConstraint.active = YES;
}

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons {
  FormInputAccessoryViewTextData* textData =
      [self.delegate textDataforFormInputAccessoryView:self];

  UIButton* closeButton = [self createCloseButtonWithText:textData];

  UIStackView* navigationView = nil;
  if (_largeAccessoryViewEnabled) {
    UIButton* manualFillButton = [self createManualFillButtonWithText:textData];
    manualFillButton.hidden = YES;
    self.manualFillButton = manualFillButton;

    UIButton* passwordManualFillButton =
        [self createPasswordManualFillButtonWithText:textData];
    passwordManualFillButton.hidden = YES;
    self.passwordManualFillButton = passwordManualFillButton;

    UIButton* creditCardManualFillButton =
        [self createCreditCardManualFillButtonWithText:textData];
    creditCardManualFillButton.hidden = YES;
    self.creditCardManualFillButton = creditCardManualFillButton;

    UIButton* addressManualFillButton =
        [self createAddressManualFillButtonWithText:textData];
    addressManualFillButton.hidden = YES;
    self.addressManualFillButton = addressManualFillButton;

    if (_isTabletFormFactor) {
      closeButton.hidden = YES;
    }

    navigationView = [[UIStackView alloc] initWithArrangedSubviews:@[
      passwordManualFillButton, creditCardManualFillButton,
      addressManualFillButton, manualFillButton, closeButton
    ]];
  } else {
    UIButton* previousButton = [self createPreviousButtonWithText:textData];
    self.previousButton = previousButton;

    UIButton* nextButton = [self createNextButtonWithText:textData];
    self.nextButton = nextButton;

    navigationView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
  }
  navigationView.spacing = ManualFillNavigationItemSpacing;
  return navigationView;
}

// Sets the minimum size constraints for an image button.
- (void)setMinimumSizeForButton:(UIButton*)button {
  [button.widthAnchor
      constraintGreaterThanOrEqualToConstant:kLargeButtonTargetArea]
      .active = YES;
  [button.heightAnchor
      constraintGreaterThanOrEqualToConstant:kLargeButtonTargetArea]
      .active = YES;
}

// Create a button with the desired image, action and accessibility label.
- (UIButton*)createImageButton:(UIImage*)image
                        action:(SEL)action
            accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* imageButton = [self createButton:image];
  [imageButton setImage:[self applySymbolTint:image]
               forState:UIControlStateNormal];
  [self setMinimumSizeForButton:imageButton];
  [imageButton addTarget:self
                  action:action
        forControlEvents:UIControlEventTouchUpInside];
  [imageButton setAccessibilityLabel:accessibilityLabel];
  return imageButton;
}

// Sets or removes the title for the manual fill button based on whether the UI
// is currently in compact mode (tablets only).
- (void)adjustManualFillButtonTitle:(UIButton*)manualFillButton {
  // The manual fill button can only have a title when using the large accessory
  // view on a tablet.
  if (!_isTabletFormFactor || !_largeAccessoryViewEnabled) {
    return;
  }

  // The default configuration has no title or padding.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  // The image should always be set, whether or not there's a title.
  buttonConfiguration.image = self.manualFillSymbol;

  if ([self isLiquidGlassEffectEnabled]) {
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        0, 0, 0, LiquidGlassCloseButtonTrailingInset);
  }

  if (!_isCompact) {
    // Set the button title with a custom sized font.
    FormInputAccessoryViewTextData* textData =
        [self.delegate textDataforFormInputAccessoryView:self];
    UIFont* font = [UIFont systemFontOfSize:kManualFillTitleFontSize
                                     weight:UIFontWeightMedium];
    NSDictionary* attributes;
    if ([self isLiquidGlassEffectEnabled]) {
      attributes = @{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor]
      };
    } else {
      attributes = @{NSFontAttributeName : font};
    }

    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:textData.manualFillButtonTitle
            attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;

    // If the button has both a title and an image, add padding around the
    // title so that it's not directly next to the image.
    if (self.manualFillSymbol) {
      buttonConfiguration.imagePadding = kManualFillTitlePadding;
    }
  }

  manualFillButton.configuration = buttonConfiguration;
}

// Creates a button of the correct type, depending on whether it is an image
// button or not.
- (UIButton*)createButton:(BOOL)isImageButton {
  // With liquid glass, image buttons apply a tint to the symbols, so the type
  // needs to be set to UIButtonTypeCustom.
  return [UIButton
      buttonWithType:(isImageButton && [self isLiquidGlassEffectEnabled])
                         ? UIButtonTypeCustom
                         : UIButtonTypeSystem];
}

// With liquid glass, we apply a symbol tint to make the symbols visible with
// the glass background.
- (UIImage*)applySymbolTint:(UIImage*)image {
  if ([self isLiquidGlassEffectEnabled]) {
    return [image imageWithTintColor:[UIColor colorNamed:kTextPrimaryColor]
                       renderingMode:UIImageRenderingModeAlwaysOriginal];
  }

  return image;
}

// Create the manual fill button.
- (UIButton*)createManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  UIButton* manualFillButton =
      [self createImageButton:self.manualFillSymbol
                       action:@selector(manualFillButtonTapped)
           accessibilityLabel:textData.manualFillButtonAccessibilityLabel];

  [self adjustManualFillButtonTitle:manualFillButton];

  return manualFillButton;
}

// Create the password manual fill button.
- (UIButton*)createPasswordManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.passwordManualFillSymbol
                  action:@selector(passwordManualFillButtonTapped)
      accessibilityLabel:textData.passwordManualFillButtonAccessibilityLabel];
}

// Create the credit card manual fill button.
- (UIButton*)createCreditCardManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.creditCardManualFillSymbol
                  action:@selector(creditCardManualFillButtonTapped)
      accessibilityLabel:textData.creditCardManualFillButtonAccessibilityLabel];
}

// Create the address manual fill button.
- (UIButton*)createAddressManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.addressManualFillSymbol
                  action:@selector(addressManualFillButtonTapped)
      accessibilityLabel:textData.addressManualFillButtonAccessibilityLabel];
}

// Create the previous button.
- (UIButton*)createPreviousButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:SymbolNamed(kChevronUpSymbol)
                          action:@selector(previousButtonTapped)
              accessibilityLabel:textData.previousButtonAccessibilityLabel];
}

// Create the next button.
- (UIButton*)createNextButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:SymbolNamed(kChevronDownSymbol)
                          action:@selector(nextButtonTapped)
              accessibilityLabel:textData.nextButtonAccessibilityLabel];
}

// Create the close button.
- (UIButton*)createCloseButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  UIButton* closeButton = [self createButton:self.closeButtonSymbol];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  if (self.closeButtonSymbol) {
    buttonConfiguration.image = [self applySymbolTint:self.closeButtonSymbol];
  } else {
    UIImage* checkmarkSymbol;
    if ([self isLiquidGlassEffectEnabled]) {
      UIImageConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kCheckmarkSymbolPointSize
                              weight:UIImageSymbolWeightSemibold
                               scale:UIImageSymbolScaleMedium];
      checkmarkSymbol = [UIImage systemImageNamed:kCheckmarkSymbol
                                withConfiguration:configuration];
      [self setMinimumSizeForButton:closeButton];
    }

    if (checkmarkSymbol) {
      buttonConfiguration.image = [self applySymbolTint:checkmarkSymbol];
    } else {
      buttonConfiguration.title = textData.closeButtonTitle;
    }
  }

  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      0, ManualFillCloseButtonLeadingInset,
      self.closeButtonSymbol ? ManualFillCloseButtonBottomInset : 0,
      [self isLiquidGlassEffectEnabled] ? LiquidGlassCloseButtonTrailingInset
                                        : ManualFillCloseButtonTrailingInset);
  closeButton.configuration = buttonConfiguration;

  [closeButton setAccessibilityLabel:textData.closeButtonAccessibilityLabel];

  return closeButton;
}

- (void)createOmniboxTypingShield {
  if (!_omniboxTypingShield) {
    CHECK(_contentView);
    _omniboxTypingShield = [[UIButton alloc] init];
    _omniboxTypingShield.translatesAutoresizingMaskIntoConstraints = NO;
    _omniboxTypingShield.backgroundColor = UIColor.clearColor;
    _omniboxTypingShield.isAccessibilityElement = NO;
    _omniboxTypingShield.opaque = NO;
    _omniboxTypingShield.accessibilityIdentifier =
        kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID;
    [self addSubview:_omniboxTypingShield];

    AddSameConstraintsToSides(
        self, _omniboxTypingShield,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    _omniboxTypingShieldHeightConstraint =
        [_omniboxTypingShield.heightAnchor constraintEqualToConstant:0];
    _omniboxTypingShieldBottomConstraint = [_omniboxTypingShield.bottomAnchor
        constraintEqualToAnchor:_contentView.topAnchor];
    _omniboxTypingShieldHiddenBottomConstraint =
        [_omniboxTypingShield.bottomAnchor
            constraintEqualToAnchor:self.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      _omniboxTypingShieldHeightConstraint, _omniboxTypingShieldBottomConstraint
    ]];
    [_omniboxTypingShield addTarget:self
                             action:@selector(omniboxTypingShieldTapped)
                   forControlEvents:UIControlEventTouchUpInside];
  }
}

// Returns the height of the accessory. Returns a larger height when using the
// large accessory view.
- (CGFloat)accessoryHeight {
  return (_largeAccessoryViewEnabled || [self isLiquidGlassEffectEnabled])
             ? kLargeKeyboardAccessoryHeight
             : kDefaultAccessoryHeight;
}

// Sets up the liquid glass effect for the accessory. Returns whether liquid
// glass is enabled.
- (BOOL)setupLiquidGlassEffect {
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if ([self isLiquidGlassEffectEnabled]) {
    if (@available(iOS 26, *)) {
      // Create glass effect
      UIGlassEffect* glassEffect = [[UIGlassEffect alloc] init];
      glassEffect.interactive = YES;
      glassEffect.tintColor = [[UIColor colorNamed:kSecondaryBackgroundColor]
          colorWithAlphaComponent:kGlassTintAlpha];

      UIVisualEffectView* effectView =
          [[UIVisualEffectView alloc] initWithEffect:nil];
      effectView.effect = glassEffect;
      effectView.cornerConfiguration = [UICornerConfiguration
          configurationWithRadius:
              [UICornerRadius
                  containerConcentricRadiusWithMinimum:kCornerRadius]];

      [self addSubview:effectView];

      effectView.translatesAutoresizingMaskIntoConstraints = NO;
      [self setOmniboxSafeTopConstraint:effectView];

      // Add padding under and on the sides of the keyboard accessory.
      [self.bottomAnchor constraintEqualToAnchor:effectView.bottomAnchor
                                        constant:kSurroundingPadding]
          .active = YES;

      [self.trailingAnchor constraintEqualToAnchor:effectView.trailingAnchor
                                          constant:kSurroundingPadding]
          .active = YES;

      // For showing a smaller accessory, the width anchor is set instead of the
      // leading anchor.
      if (_smallWidthAccessoryViewEnabled) {
        [effectView.widthAnchor constraintEqualToConstant:kSmallAccessoryWidth]
            .active = YES;
      } else {
        [self.leadingAnchor constraintEqualToAnchor:effectView.leadingAnchor
                                           constant:-kSurroundingPadding]
            .active = YES;
      }

      _contentView = effectView.contentView;
      AddSameConstraints(effectView, _contentView);
      [self setDefaultHeightConstraint:effectView];

      // Add shadow around the glass effect.
      self.layer.shadowRadius = kShadowRadius;
      self.layer.shadowOffset = CGSizeMake(0, kShadowVerticalOffset);
      self.layer.shadowOpacity = kShadowOpacity;
      self.layer.shadowColor =
          [UIColor colorNamed:kBackgroundShadowColor].CGColor;
      self.layer.masksToBounds = NO;

      return YES;
    }
  }
#endif  // defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
  // __IPHONE_26_0

  return NO;
}

// Sets the bottom anchor depending on whether the liquid glass effect is
// enabled.
- (void)setBottomAnchorForView:(UIView*)view {
  if ([self isLiquidGlassEffectEnabled]) {
    // When using liquid glass, to ensure a constant height, we use the height
    // constraints, instead of the bottom constraint.
    [self setDefaultHeightConstraint:view];
  } else {
    [view.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor]
        .active = YES;
  }
}

// Applies the proper horizontal padding, depending on whether the keyboard
// accessory is in compact mode (tablet only).
- (void)setHorizontalConstraints {
  if (!_isTabletFormFactor || !_largeAccessoryViewEnabled) {
    return;
  }

  _trailingConstraint.active = !_isCompact;
  _compactTrailingConstraint.active = _isCompact;
}

#pragma mark - UIView

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];

  // The bottom omnibox is anchored to the typing shield. If we don't change the
  // shield's anchor, when hiding the accessory view, there is a blank space the
  // size of the keyboard accessory view between the top of the view below the
  // accessory view and the omnibox. By changing the anchor here, the omnibox
  // appears directly above the view below the accessory view, without any gaps.
  _omniboxTypingShieldBottomConstraint.active = !hidden;
  _omniboxTypingShieldHiddenBottomConstraint.active = hidden;

  [self layoutIfNeeded];
}

@end
