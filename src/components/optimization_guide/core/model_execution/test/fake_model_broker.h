// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

class FakeModelBroker {
 public:
  explicit FakeModelBroker(const FakeAdaptationAsset& asset);
  ~FakeModelBroker();

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote();

  on_device_model::FakeOnDeviceServiceSettings& settings() {
    return fake_settings_;
  }

  void CrashService() { fake_launcher_.CrashService(); }

  void UpdateModelAdaptation(const FakeAdaptationAsset& asset);
  void UpdateSafetyModel(const optimization_guide::ModelInfo& model_info) {
    controller().MaybeUpdateSafetyModel(model_info);
  }

  std::unique_ptr<OnDeviceAssetManager> CreateAssetManager(
      OptimizationGuideModelProvider* provider);

  TestingPrefServiceSimple& local_state() { return local_state_; }
  OnDeviceModelServiceController& controller() {
    return model_broker_state_.service_controller();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  FakeBaseModelAsset base_model_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  TestComponentState component_state_;
  ModelBrokerState model_broker_state_{&local_state_,
                                       component_state_.CreateDelegate(),
                                       fake_launcher_.LaunchFn()};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
