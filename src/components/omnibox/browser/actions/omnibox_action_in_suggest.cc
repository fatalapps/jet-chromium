// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"
#include "url/android/gurl_android.h"
#endif

namespace {
// UMA reported Type of ActionInSuggest.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ActionInSuggestUmaType
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The values should remain
// synchronized with the enum ActionInSuggestType in
// //tools/metrics/histograms/metadata/omnibox/enums.xml.
enum class ActionInSuggestUmaType {
  kUnknown = 0,
  kCall,
  kDirections,
  kWebsite,
  kReviews,
  kAim,

  // Sentinel value. Must be set to the last valid ActionInSuggestUmaType.
  kMaxValue = kAim
};

constexpr const char* ToUmaUsageHistogramName(
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType type) {
  switch (type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return "Omnibox.ActionInSuggest.UsageByType.Call";
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return "Omnibox.ActionInSuggest.UsageByType.Directions";
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return "Omnibox.ActionInSuggest.UsageByType.Reviews";
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM:
      return "Omnibox.ActionInSuggest.UsageByType.AIM";
  }
  NOTREACHED() << "Unexpected type of Action: " << (int)type;
}

// Get the UMA action type from TemplateAction::ActionType.
constexpr ActionInSuggestUmaType ToUmaActionType(
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType action_type) {
  switch (action_type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return ActionInSuggestUmaType::kCall;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return ActionInSuggestUmaType::kDirections;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return ActionInSuggestUmaType::kReviews;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM:
      return ActionInSuggestUmaType::kAim;
  }
  NOTREACHED() << "Unrecognized action type: " << action_type;
}

constexpr int ToActionHint(
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType action_type) {
  switch (action_type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_CALL_HINT;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_DIRECTIONS_HINT;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_REVIEWS_HINT;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_AIM_HINT;
  }
  NOTREACHED() << "Unrecognized action type: " << action_type;
}

constexpr int ToActionContents(
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType action_type) {
  switch (action_type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_CALL_CONTENTS;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_DIRECTIONS_CONTENTS;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_REVIEWS_CONTENTS;
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM:
      return IDS_OMNIBOX_ACTION_IN_SUGGEST_AIM_CONTENTS;
  }
  NOTREACHED() << "Unrecognized action type: " << action_type;
}
}  // namespace

OmniboxActionInSuggest::OmniboxActionInSuggest(
    omnibox::SuggestTemplateInfo::TemplateAction template_action,
    std::optional<TemplateURLRef::SearchTermsArgs> search_terms_args)
    : OmniboxAction(OmniboxAction::LabelStrings(
                        ToActionHint(template_action.action_type()),
                        ToActionContents(template_action.action_type()),
                        IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX,
                        ToActionContents(template_action.action_type())),
                    {}),
      template_action{std::move(template_action)},
      search_terms_args{std::move(search_terms_args)} {}

OmniboxActionInSuggest::~OmniboxActionInSuggest() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
OmniboxActionInSuggest::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    j_omnibox_action_.Reset(BuildOmniboxActionInSuggest(
        env, reinterpret_cast<intptr_t>(this), strings_.hint,
        strings_.accessibility_hint, template_action.action_type(),
        template_action.action_uri()));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif

void OmniboxActionInSuggest::RecordActionShown(size_t position,
                                               bool used) const {
  RecordShownAndUsedMetrics(template_action.action_type(), used);
}

void OmniboxActionInSuggest::Execute(ExecutionContext& context) const {
  // Note: this is platform-dependent.
  // There's currently no code wiring ActionInSuggest on Desktop.
  // TODO(crbug.com/40257536): log searchboxstats metrics.
  NOTREACHED() << "Not implemented";
}

OmniboxActionId OmniboxActionInSuggest::ActionId() const {
  return OmniboxActionId::ACTION_IN_SUGGEST;
}

// static
const OmniboxActionInSuggest* OmniboxActionInSuggest::FromAction(
    const OmniboxAction* action) {
  return FromAction(const_cast<OmniboxAction*>(action));
}

// static
OmniboxActionInSuggest* OmniboxActionInSuggest::FromAction(
    OmniboxAction* action) {
  if (action && action->ActionId() == OmniboxActionId::ACTION_IN_SUGGEST) {
    return static_cast<OmniboxActionInSuggest*>(action);
  }
  return nullptr;
}

// static
void OmniboxActionInSuggest::RecordShownAndUsedMetrics(
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType type,
    bool used) {
  base::UmaHistogramEnumeration("Omnibox.ActionInSuggest.Shown",
                                ToUmaActionType(type));
  if (used) {
    base::UmaHistogramEnumeration("Omnibox.ActionInSuggest.Used",
                                  ToUmaActionType(type));
  }

  base::UmaHistogramBoolean(ToUmaUsageHistogramName(type), used);
}

omnibox::SuggestTemplateInfo_TemplateAction_ActionType
OmniboxActionInSuggest::Type() const {
  return template_action.action_type();
}
