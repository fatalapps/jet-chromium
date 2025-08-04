// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_icon_element.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/html/html_permission_element_utils.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

namespace {
constexpr float kMaxIconSizeToFontSizeRatio = 1.5;
constexpr float kMaxMarginInlineEndToFontSizeRatio = 3.0;
}  // namespace

using mojom::blink::PermissionName;

HTMLPermissionIconElement::HTMLPermissionIconElement(Document& document)
    : HTMLSpanElement(document) {
  SetIdAttribute(shadow_element_names::kIdPermissionIcon);
  SetShadowPseudoId(shadow_element_names::kIdPermissionIcon);
  SetHasCustomStyleCallbacks();
}

void HTMLPermissionIconElement::SetIcon(PermissionName permission_type,
                                        bool is_precise_location) {
  if (is_icon_set_) {
    return;
  }
  switch (permission_type) {
    case PermissionName::GEOLOCATION:
      SetInnerHTMLWithoutTrustedTypes(UncompressResourceAsASCIIString(
          is_precise_location ? IDR_PERMISSION_ICON_LOCATION_PRECISE_SVG
                              : IDR_PERMISSION_ICON_LOCATION_SVG));
      break;
    case PermissionName::VIDEO_CAPTURE:
      SetInnerHTMLWithoutTrustedTypes(String(
          UncompressResourceAsASCIIString(IDR_PERMISSION_ICON_CAMERA_SVG)));
      break;
    case PermissionName::AUDIO_CAPTURE:
      SetInnerHTMLWithoutTrustedTypes(
          UncompressResourceAsASCIIString(IDR_PERMISSION_ICON_MICROPHONE_SVG));
      break;
    default:
      return;
  }
  is_icon_set_ = true;
}

void HTMLPermissionIconElement::AdjustStyle(ComputedStyleBuilder& builder) {
  Element::AdjustStyle(builder);
  if (!builder.Width().IsAuto() && !width_console_error_sent_) {
    width_console_error_sent_ = true;
    const String warning =
        "Setting the width has no effect on the icon size. Please use the "
        "height property to control the icon size.";
    AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                      mojom::blink::ConsoleMessageLevel::kWarning, warning);
  }

  float max_height = builder.FontSize() * kMaxIconSizeToFontSizeRatio;

  builder.SetHeight(
      AdjustedBoundedLengthWrapper(builder.Height(),
                                   /*lower_bound=*/0,
                                   /*upper_bound=*/max_height,
                                   /*should_multiply_by_content_size=*/false));
  // Width is set to be same as height, as the icon is always a square.
  builder.SetWidth(builder.Height());

  builder.SetMinHeight(
      AdjustedBoundedLengthWrapper(builder.MinHeight(),
                                   /*lower_bound=*/0,
                                   /*upper_bound=*/max_height,
                                   /*should_multiply_by_content_size=*/false));
  // Min-width is set to be same as min-height, as the icon is always a square.
  builder.SetMinWidth(builder.MinHeight());

  float max_margin = builder.FontSize() * kMaxMarginInlineEndToFontSizeRatio;
  if (builder.Direction() == TextDirection::kLtr) {
    builder.SetMarginLeft(Length(0, Length::Type::kFixed));
    builder.SetMarginRight(AdjustedBoundedLengthWrapper(
        builder.MarginRight(),
        /*lower_bound=*/0,
        /*upper_bound=*/max_margin,
        /*should_multiply_by_content_size=*/false));
  } else {
    builder.SetMarginRight(Length(0, Length::Type::kFixed));
    builder.SetMarginLeft(AdjustedBoundedLengthWrapper(
        builder.MarginLeft(),
        /*lower_bound=*/0,
        /*upper_bound=*/max_margin,
        /*should_multiply_by_content_size=*/false));
  }
}

Length HTMLPermissionIconElement::AdjustedBoundedLengthWrapper(
    const Length& length,
    std::optional<float> lower_bound,
    std::optional<float> upper_bound,
    bool should_multiply_by_content_size) {
  CHECK(lower_bound.has_value() || upper_bound.has_value());
  bool is_content_or_stretch =
      length.HasContentOrIntrinsic() || length.HasStretch();
  if (is_content_or_stretch && !length_console_error_sent_) {
    length_console_error_sent_ = true;
    const String warning =
        "content, intrinsic, or stretch sizes are not supported as values for "
        "the height or min height of the permission element's icon";
    AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                      mojom::blink::ConsoleMessageLevel::kWarning, warning);
  }
  return HTMLPermissionElementUtils::AdjustedBoundedLength(
      length, lower_bound, upper_bound, should_multiply_by_content_size);
}

}  // namespace blink
