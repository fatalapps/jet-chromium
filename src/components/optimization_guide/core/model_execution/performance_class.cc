// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/performance_class.h"

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/version_info/version_info.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/on_device_model/public/cpp/cpu.h"

namespace optimization_guide {

namespace {

// Whether image input is enabled for CPU backend.
BASE_FEATURE(kOnDeviceModelCpuImageInput,
             "OnDeviceModelCpuImageInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether audio input is enabled for CPU backend.
BASE_FEATURE(kOnDeviceModelCpuAudioInput,
             "OnDeviceModelCpuAudioInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Commandline switch to force a particular performance class.
const char kOverridePerformanceClassSwitch[] =
    "optimization-guide-performance-class";

bool NeedsPerformanceClassUpdate(const PrefService& local_state) {
  if (!features::CanLaunchOnDeviceModelService()) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kOnDeviceModelFetchPerformanceClassEveryStartup)) {
    return true;
  }
  return local_state.GetString(model_execution::prefs::localstate::
                                   kOnDevicePerformanceClassVersion) !=
         version_info::GetVersionNumber();
}

// Convert a number to a performance class.
OnDeviceModelPerformanceClass AsPerformanceClass(int value) {
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelPerformanceClass::kMaxValue)) {
    return OnDeviceModelPerformanceClass::kUnknown;
  }
  return static_cast<OnDeviceModelPerformanceClass>(value);
}

OnDeviceModelPerformanceClass GetPerformanceClassSwitch() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOverridePerformanceClassSwitch)) {
    return OnDeviceModelPerformanceClass::kUnknown;
  }
  int value = 0;
  if (!base::StringToInt(
          command_line->GetSwitchValueASCII(kOverridePerformanceClassSwitch),
          &value)) {
    return OnDeviceModelPerformanceClass::kUnknown;
  }
  return AsPerformanceClass(value);
}

}  // namespace

OnDeviceModelPerformanceClass ConvertToOnDeviceModelPerformanceClass(
    on_device_model::mojom::PerformanceClass performance_class) {
  switch (performance_class) {
    case on_device_model::mojom::PerformanceClass::kError:
      return OnDeviceModelPerformanceClass::kError;
    case on_device_model::mojom::PerformanceClass::kVeryLow:
      return OnDeviceModelPerformanceClass::kVeryLow;
    case on_device_model::mojom::PerformanceClass::kLow:
      return OnDeviceModelPerformanceClass::kLow;
    case on_device_model::mojom::PerformanceClass::kMedium:
      return OnDeviceModelPerformanceClass::kMedium;
    case on_device_model::mojom::PerformanceClass::kHigh:
      return OnDeviceModelPerformanceClass::kHigh;
    case on_device_model::mojom::PerformanceClass::kVeryHigh:
      return OnDeviceModelPerformanceClass::kVeryHigh;
    case on_device_model::mojom::PerformanceClass::kGpuBlocked:
      return OnDeviceModelPerformanceClass::kGpuBlocked;
    case on_device_model::mojom::PerformanceClass::kFailedToLoadLibrary:
      return OnDeviceModelPerformanceClass::kFailedToLoadLibrary;
  }
}

std::string SyntheticTrialGroupForPerformanceClass(
    OnDeviceModelPerformanceClass performance_class) {
  switch (performance_class) {
    case OnDeviceModelPerformanceClass::kUnknown:
      return "Unknown";
    case OnDeviceModelPerformanceClass::kError:
      return "Error";
    case OnDeviceModelPerformanceClass::kVeryLow:
      return "VeryLow";
    case OnDeviceModelPerformanceClass::kLow:
      return "Low";
    case OnDeviceModelPerformanceClass::kMedium:
      return "Medium";
    case OnDeviceModelPerformanceClass::kHigh:
      return "High";
    case OnDeviceModelPerformanceClass::kVeryHigh:
      return "VeryHigh";
    case OnDeviceModelPerformanceClass::kGpuBlocked:
      return "GpuBlocked";
    case OnDeviceModelPerformanceClass::kFailedToLoadLibrary:
      return "FailedToLoadLibrary";
    case OnDeviceModelPerformanceClass::kServiceCrash:
      return "ServiceCrash";
  }
}

bool IsPerformanceClassCompatible(
    std::string perf_classes_string,
    OnDeviceModelPerformanceClass performance_class) {
  if (perf_classes_string == "*") {
    return true;
  }
  std::vector<std::string_view> perf_classes_list = base::SplitStringPiece(
      perf_classes_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::Contains(perf_classes_list,
                        base::ToString(static_cast<int>(performance_class)));
}

OnDeviceModelPerformanceClass PerformanceClassFromPref(
    const PrefService& local_state) {
  int value = local_state.GetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelPerformanceClass::kMaxValue)) {
    return OnDeviceModelPerformanceClass::kUnknown;
  }
  return static_cast<OnDeviceModelPerformanceClass>(value);
}

void UpdatePerformanceClassPref(
    PrefService* local_state,
    OnDeviceModelPerformanceClass performance_class) {
  local_state->SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(performance_class));
  local_state->SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      version_info::GetVersionNumber());
}

PerformanceClassifier::PerformanceClassifier(
    PrefService* local_state,
    base::SafeRef<on_device_model::ServiceClient> service_client)
    : local_state_(local_state), service_client_(std::move(service_client)) {}
PerformanceClassifier::~PerformanceClassifier() = default;

void PerformanceClassifier::Init() {
  CHECK_EQ(performance_class_state_, PerformanceClassState::kNotStarted);
  CHECK(performance_class_callbacks_.empty());
  OnDeviceModelPerformanceClass override_class = GetPerformanceClassSwitch();
  if (override_class != OnDeviceModelPerformanceClass::kUnknown) {
    UpdatePerformanceClassPref(local_state_, override_class);
    performance_class_state_ = PerformanceClassState::kComplete;
    return;
  }
  if (!NeedsPerformanceClassUpdate(*local_state_)) {
    performance_class_state_ = PerformanceClassState::kComplete;
  }
}

void PerformanceClassifier::ScheduleEvaluation() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PerformanceClassifier::EnsurePerformanceClassAvailable,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()),
      optimization_guide::features::GetOnDeviceStartupMetricDelay());
}

void PerformanceClassifier::EnsurePerformanceClassAvailable(
    base::OnceClosure complete) {
  if (ListenForPerformanceClassAvailable(std::move(complete))) {
    return;
  }

  if (performance_class_state_ != PerformanceClassState::kNotStarted) {
    return;
  }

  CHECK(features::CanLaunchOnDeviceModelService());

  performance_class_state_ = PerformanceClassState::kComputing;
  service_client_->Get()->GetDevicePerformanceInfo(
      base::BindOnce([](on_device_model::mojom::DevicePerformanceInfoPtr info) {
        return info->performance_class;
      })
          .Then(base::BindOnce(&ConvertToOnDeviceModelPerformanceClass)
                    .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                        base::BindOnce(
                            &PerformanceClassifier::PerformanceClassEvaluated,
                            weak_ptr_factory_.GetWeakPtr()),
                        OnDeviceModelPerformanceClass::kServiceCrash))));
}

bool PerformanceClassifier::ListenForPerformanceClassAvailable(
    base::OnceClosure available) {
  if (IsPerformanceClassAvailable()) {
    std::move(available).Run();
    return true;
  }

  // Use unsafe because cancellation isn't needed.
  performance_class_callbacks_.AddUnsafe(std::move(available));
  return false;
}

OnDeviceModelPerformanceClass PerformanceClassifier::GetPerformanceClass()
    const {
  CHECK(IsPerformanceClassAvailable());
  return PerformanceClassFromPref(*local_state_);
}

bool PerformanceClassifier::IsDeviceGPUCapable() const {
  return IsPerformanceClassCompatible(
      features::kPerformanceClassListForOnDeviceModel.Get(),
      GetPerformanceClass());
}

bool PerformanceClassifier::IsDeviceCapable() const {
  return IsDeviceGPUCapable() || on_device_model::IsCpuCapable();
}

bool PerformanceClassifier::IsLowTierDevice() const {
  return IsPerformanceClassCompatible(
      features::kLowTierPerformanceClassListForOnDeviceModel.Get(),
      GetPerformanceClass());
}

bool PerformanceClassifier::SupportsImageInput() const {
  return (IsDeviceGPUCapable() &&
          IsPerformanceClassCompatible(
              features::kPerformanceClassListForImageInput.Get(),
              GetPerformanceClass())) ||
         (IsDeviceCapable() &&
          base::FeatureList::IsEnabled(kOnDeviceModelCpuImageInput));
}

bool PerformanceClassifier::SupportsAudioInput() const {
  return (IsDeviceGPUCapable() &&
          IsPerformanceClassCompatible(
              features::kPerformanceClassListForAudioInput.Get(),
              GetPerformanceClass())) ||
         (IsDeviceCapable() &&
          base::FeatureList::IsEnabled(kOnDeviceModelCpuAudioInput));
}

std::vector<proto::OnDeviceModelPerformanceHint>
PerformanceClassifier::GetPossibleHints() const {
  std::vector<proto::OnDeviceModelPerformanceHint> hints;
  if (IsDeviceGPUCapable()) {
    // Best option is highest quality for GPU device that is not low tier.
    if (!IsLowTierDevice()) {
      hints.push_back(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY);
    }
    // Other GPU capable devices get fastest inference.
    hints.push_back(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE);
  }
  if (on_device_model::IsCpuCapable()) {
    // Last option is CPU if the device is capable but not GPU capable.
    hints.push_back(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
  }
  return hints;
}

on_device_model::Capabilities
PerformanceClassifier::GetPossibleOnDeviceCapabilities() const {
  on_device_model::Capabilities capabilities;
  if (SupportsImageInput()) {
    capabilities.Put(on_device_model::CapabilityFlags::kImageInput);
  }
  if (SupportsAudioInput()) {
    capabilities.Put(on_device_model::CapabilityFlags::kAudioInput);
  }
  return capabilities;
}

void PerformanceClassifier::PerformanceClassEvaluated(
    OnDeviceModelPerformanceClass perf_class) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      perf_class);
  UpdatePerformanceClassPref(local_state_, perf_class);
  performance_class_state_ = PerformanceClassState::kComplete;
  performance_class_callbacks_.Notify();
}

}  // namespace optimization_guide
