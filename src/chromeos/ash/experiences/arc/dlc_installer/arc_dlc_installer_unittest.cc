// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_install_hardware_checker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

namespace arc {

class ArcDlcInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    ash::DlcserviceClient::InitializeFake();
    ash::UpstartClient::InitializeFake();

    auto fake_message_center =
        std::make_unique<message_center::FakeMessageCenter>();
    fake_message_center_ = fake_message_center.get();
    message_center::MessageCenter::InitializeForTesting(
        std::move(fake_message_center));

    std::unique_ptr<FakeArcDlcInstallHardwareChecker> fake_hardware_checker =
        std::make_unique<FakeArcDlcInstallHardwareChecker>(true);
    cros_settings_ = std::make_unique<ash::CrosSettings>();
    auto provider =
        std::make_unique<ash::FakeCrosSettingsProvider>(base::DoNothing());
    fake_provider_ = provider.get();
    cros_settings_->AddSettingsProvider(std::move(provider));
    // TODO(b/405341089): Update fake provider to accept unset value for
    // specific path.
    fake_provider_->Set(ash::kDeviceFlexArcPreloadEnabled, base::Value());
    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>(
        std::move(fake_hardware_checker), cros_settings_.get());
  }

  void TearDown() override {
    arc_dlc_installer_.reset();
    fake_provider_ = nullptr;
    cros_settings_.reset();
    ash::UpstartClient::Shutdown();
    ash::DlcserviceClient::Shutdown();
    fake_message_center_ = nullptr;
    message_center::MessageCenter::Shutdown();
  }

  void SetFlexArcPreloadEnabled(bool enabled) {
    fake_provider_->Set(ash::kDeviceFlexArcPreloadEnabled,
                        base::Value(enabled));
  }

  void PrepareArcAndWait(bool expected_result) {
    base::RunLoop run_loop;
    arc_dlc_installer_->PrepareArc(base::BindOnce(
        [](base::RunLoop* loop, bool expected, bool actual_result) {
          EXPECT_EQ(expected, actual_result);
          loop->Quit();
        },
        base::Unretained(&run_loop), expected_result));
    run_loop.Run();
  }

  void VerifyNotifications(base::span<const std::string_view> expected_ids) {
    const auto& notifications = fake_message_center_->GetNotifications();
    ASSERT_EQ(notifications.size(), expected_ids.size());
    size_t index = 0;
    for (const auto& notification : notifications) {
      ASSERT_TRUE(notification);
      EXPECT_EQ(notification->id(), expected_ids[index++]);
    }
  }

  ash::FakeDlcserviceClient* fake_dlcservice_client() {
    return static_cast<ash::FakeDlcserviceClient*>(
        ash::DlcserviceClient::Get());
  }

  base::test::TaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  raw_ptr<message_center::FakeMessageCenter> fake_message_center_ = nullptr;
  std::unique_ptr<ash::CrosSettings> cros_settings_;
  raw_ptr<ash::FakeCrosSettingsProvider> fake_provider_ = nullptr;
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
};

// Verifies that ARCVM DLC image preparation fails when the arcvm-dlc command
// flag is not enabled.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_NoArcvmDlcCommandFlag) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  SetFlexArcPreloadEnabled(true);
  PrepareArcAndWait(/*expected_result=*/false);
}

// Verifies that ARCVM DLC image preparation fails for unmanaged devices.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_UnmanagedDevice) {
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);

  PrepareArcAndWait(/*expected_result=*/false);
}

// Verifies that ARCVM DLC image preparation fails when the
// kDeviceFlexArcPreloadEnabled policy is unset.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_WithPolicyUnset) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(false);

  PrepareArcAndWait(/*expected_result=*/false);
}

TEST_F(ArcDlcInstallerTest, VerifyNotifications_DlcServiceNotAvailable) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  fake_dlcservice_client()->set_service_availability(false);

  PrepareArcAndWait(/*expected_result=*/false);

  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadFailedId});
}

TEST_F(ArcDlcInstallerTest, VerifyNotifications_InstallSuccess) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  fake_dlcservice_client()->set_trigger_install_progress(true);
  fake_dlcservice_client()->set_install_error(dlcservice::kErrorNone);

  PrepareArcAndWait(/*expected_result=*/true);

  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadSucceededId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

TEST_F(ArcDlcInstallerTest, VerifyNotifications_InstallFail) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  fake_dlcservice_client()->set_trigger_install_progress(true);
  fake_dlcservice_client()->set_install_error(dlcservice::kErrorInternal);

  PrepareArcAndWait(/*expected_result=*/false);

  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadFailedId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

// Verifies that installation completion notifications are triggered only once
// even after repeated DLC installations.
TEST_F(ArcDlcInstallerTest, CompletionNotificationTriggerOnce_RepeatInstall) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  fake_dlcservice_client()->set_trigger_install_progress(true);
  fake_dlcservice_client()->set_install_error(dlcservice::kErrorNone);

  // Simulate the first DLC installation.
  PrepareArcAndWait(/*expected_result=*/true);
  // Simulate the second DLC installation.
  PrepareArcAndWait(/*expected_result=*/true);

  // Expect two notifications: one for the preload start and one for the
  // success, even after triggering the installation twice.
  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadSucceededId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

// Verifies that the correct upstart jobs are restarted upon a successful DLC
// installation.
TEST_F(ArcDlcInstallerTest, VerifyUpstartJobs_InstallSuccess) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  auto* fake_upstart_client =
      static_cast<ash::FakeUpstartClient*>(ash::UpstartClient::Get());

  // Start recording calls to the fake upstart client before running the
  // installation flow.
  fake_upstart_client->StartRecordingUpstartOperations();
  PrepareArcAndWait(/*expected_result=*/true);

  const auto& ops = fake_upstart_client->upstart_operations();

  // We expect a STOP and a START for each of the two jobs.
  ASSERT_EQ(4u, ops.size());

  // Define the job names from arc_dlc_installer.cc for clarity.
  constexpr const char kArcvmBindMountDlcPath[] =
      "arcvm_2dbind_2dmount_2ddlc_2dpath";
  constexpr const char kVmConciergeServiceName[] = "vm_5fconcierge";

  // Verify the first job was stopped, then started.
  EXPECT_EQ(ops[0].name, kArcvmBindMountDlcPath);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].name, kArcvmBindMountDlcPath);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);

  // Verify the second job was stopped, then started.
  EXPECT_EQ(ops[2].name, kVmConciergeServiceName);
  EXPECT_EQ(ops[2].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[3].name, kVmConciergeServiceName);
  EXPECT_EQ(ops[3].type, ash::FakeUpstartClient::UpstartOperationType::START);
}

// Verifies that no upstart jobs are restarted upon a failed DLC installation.
TEST_F(ArcDlcInstallerTest, VerifyUpstartJobs_InstallFail) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  fake_dlcservice_client()->set_trigger_install_progress(true);
  fake_dlcservice_client()->set_install_error(dlcservice::kErrorInternal);
  auto* fake_upstart_client =
      static_cast<ash::FakeUpstartClient*>(ash::UpstartClient::Get());

  // Start recording calls to the fake upstart client.
  fake_upstart_client->StartRecordingUpstartOperations();
  PrepareArcAndWait(/*expected_result=*/false);

  EXPECT_TRUE(fake_upstart_client->upstart_operations().empty());
}

}  // namespace arc
