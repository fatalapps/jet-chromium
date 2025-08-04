// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Let the DevTools front-end query an AIDA endpoint for explanations and
// insights regarding console (error) messages.
BASE_FEATURE(kDevToolsConsoleInsights,
             "DevToolsConsoleInsights",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId{
    &kDevToolsConsoleInsights, "aida_model_id", /*default_value=*/""};
const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature{
    &kDevToolsConsoleInsights, "aida_temperature", /*default_value=*/-1};
const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn{
    &kDevToolsConsoleInsights, "opt_in", /*default_value=*/false};

const base::FeatureParam<DevToolsFreestylerUserTier>::Option
    devtools_freestyler_user_tier_options[] = {
        {DevToolsFreestylerUserTier::kTesters, "TESTERS"},
        {DevToolsFreestylerUserTier::kBeta, "BETA"},
        {DevToolsFreestylerUserTier::kPublic, "PUBLIC"}};

const base::FeatureParam<DevToolsFreestylerExecutionMode>::Option
    devtools_freestyler_execution_mode_options[] = {
        {DevToolsFreestylerExecutionMode::kAllScripts, "ALL_SCRIPTS"},
        {DevToolsFreestylerExecutionMode::kSideEffectFreeScriptsOnly,
         "SIDE_EFFECT_FREE_SCRIPTS_ONLY"},
        {DevToolsFreestylerExecutionMode::kNoScripts, "NO_SCRIPTS"}};

// Whether the DevTools styling assistant is enabled.
BASE_FEATURE(kDevToolsFreestyler,
             "DevToolsFreestyler",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsFreestylerModelId{
    &kDevToolsFreestyler, "aida_model_id", /*default_value=*/""};
const base::FeatureParam<double> kDevToolsFreestylerTemperature{
    &kDevToolsFreestyler, "aida_temperature", /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsFreestylerUserTier{
        &kDevToolsFreestyler, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};
const base::FeatureParam<DevToolsFreestylerExecutionMode>
    kDevToolsFreestylerExecutionMode{
        &kDevToolsFreestyler, "execution_mode",
        /*default_value=*/DevToolsFreestylerExecutionMode::kAllScripts,
        &devtools_freestyler_execution_mode_options};
const base::FeatureParam<bool> kDevToolsFreestylerPatching{
    &kDevToolsFreestyler, "patching", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerMultimodal{
    &kDevToolsFreestyler, "multimodal", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerMultimodalUploadInput{
    &kDevToolsFreestyler, "multimodal_upload_input", /*default_value=*/true};
const base::FeatureParam<bool> kDevToolsFreestylerFunctionCalling{
    &kDevToolsFreestyler, "function_calling", /*default_value=*/true};

// Whether the DevTools AI Assistance Network Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceNetworkAgent,
             "DevToolsAiAssistanceNetworkAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiAssistanceNetworkAgentModelId{
    &kDevToolsAiAssistanceNetworkAgent, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiAssistanceNetworkAgentTemperature{
    &kDevToolsAiAssistanceNetworkAgent, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceNetworkAgentUserTier{
        &kDevToolsAiAssistanceNetworkAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Assistance Performance Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistancePerformanceAgent,
             "DevToolsAiAssistancePerformanceAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kDevToolsAiAssistancePerformanceAgentModelId{
        &kDevToolsAiAssistancePerformanceAgent, "aida_model_id",
        /*default_value=*/""};
const base::FeatureParam<double>
    kDevToolsAiAssistancePerformanceAgentTemperature{
        &kDevToolsAiAssistancePerformanceAgent, "aida_temperature",
        /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistancePerformanceAgentUserTier{
        &kDevToolsAiAssistancePerformanceAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};
const base::FeatureParam<bool>
    kDevToolsAiAssistancePerformanceAgentInsightsEnabled{
        &kDevToolsAiAssistancePerformanceAgent, "insights_enabled",
        /*default_value=*/true};

// Whether the DevTools AI Assistance File Agent is enabled.
BASE_FEATURE(kDevToolsAiAssistanceFileAgent,
             "DevToolsAiAssistanceFileAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiAssistanceFileAgentModelId{
    &kDevToolsAiAssistanceFileAgent, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiAssistanceFileAgentTemperature{
    &kDevToolsAiAssistanceFileAgent, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiAssistanceFileAgentUserTier{
        &kDevToolsAiAssistanceFileAgent, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether the DevTools AI Code Completion is enabled.
BASE_FEATURE(kDevToolsAiCodeCompletion,
             "DevToolsAiCodeCompletion",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kDevToolsAiCodeCompletionModelId{
    &kDevToolsAiCodeCompletion, "aida_model_id",
    /*default_value=*/""};
const base::FeatureParam<double> kDevToolsAiCodeCompletionTemperature{
    &kDevToolsAiCodeCompletion, "aida_temperature",
    /*default_value=*/-1};
const base::FeatureParam<DevToolsFreestylerUserTier>
    kDevToolsAiCodeCompletionUserTier{
        &kDevToolsAiCodeCompletion, "user_tier",
        /*default_value=*/DevToolsFreestylerUserTier::kPublic,
        &devtools_freestyler_user_tier_options};

// Whether an infobar is shown when the process is shared.
BASE_FEATURE(kDevToolsSharedProcessInfobar,
             "DevToolsSharedProcessInfobar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether showing animation styles in the styles tab is enabled.
BASE_FEATURE(kDevToolsAnimationStylesInStylesTab,
             "DevToolsAnimationStylesInStylesTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools will attempt to load project settings from a well-known
// URI. See https://goo.gle/devtools-json-design for additional details.
// This is enabled by default starting with M-136.
BASE_FEATURE(kDevToolsWellKnown,
             "DevToolsWellKnown",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsAiGeneratedTimelineLabels,
             "DevToolsAiGeneratedTimelineLabels",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsNewPermissionDialog,
             "DevToolsNewPermissionDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools drawer can be toggled to vertical orientation.
BASE_FEATURE(kDevToolsVerticalDrawer,
             "DevToolsVerticalDrawer",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_PWA_INSTALL_ON_CROS_TEST)
// Enables creating PWA handler for DevTools.
BASE_FEATURE(kDevToolsPwaHandler,
             "DevToolsPwaHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_PWA_INSTALL_ON_CROS_TEST)

// Whether DevTools shows submenu example prompts for the AI Assistance panel
// in context menus.
BASE_FEATURE(kDevToolsAiSubmenuPrompts,
             "DevToolsAiSubmenuPrompts",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
