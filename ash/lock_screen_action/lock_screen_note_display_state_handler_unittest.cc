// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_note_display_state_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/events/devices/stylus_state.h"

namespace ash {

namespace {

constexpr double kVisibleBrightnessPercent = 10;

class TestPowerManagerObserver : public chromeos::PowerManagerClient::Observer {
 public:
  explicit TestPowerManagerObserver(
      chromeos::FakePowerManagerClient* power_manager)
      : power_manager_(power_manager), scoped_observer_(this) {
    scoped_observer_.Add(power_manager_);
    power_manager_->set_user_activity_callback(base::BindRepeating(
        &TestPowerManagerObserver::OnUserActivity, base::Unretained(this)));
  }
  ~TestPowerManagerObserver() override {
    power_manager_->set_user_activity_callback(base::RepeatingClosure());
  }

  const std::vector<double>& brightness_changes() const {
    return brightness_changes_;
  }

  void ClearBrightnessChanges() { brightness_changes_.clear(); }

  // chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override {
    brightness_changes_.push_back(change.percent());
  }

  void OnUserActivity() {
    if (!power_manager_->backlights_forced_off() &&
        power_manager_->screen_brightness_percent() == 0) {
      brightness_changes_.push_back(kVisibleBrightnessPercent);
    }
  }

 private:
  chromeos::FakePowerManagerClient* power_manager_;
  std::vector<double> brightness_changes_;

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestPowerManagerObserver);
};

}  // namespace

class LockScreenNoteDisplayStateHandlerTest : public AshTestBase {
 public:
  LockScreenNoteDisplayStateHandlerTest() = default;
  ~LockScreenNoteDisplayStateHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);

    auto power_manager_client =
        std::make_unique<chromeos::FakePowerManagerClient>();
    power_manager_client_ = power_manager_client.get();
    power_manager_client_->SetScreenBrightnessPercent(kVisibleBrightnessPercent,
                                                      false /*gradual*/);
    power_manager_observer_ =
        std::make_unique<TestPowerManagerObserver>(power_manager_client_);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::move(power_manager_client));

    AshTestBase::SetUp();

    BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
    InitializeTabletPowerButtonState();

    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateInterfacePtrAndBind(),
        mojom::TrayActionState::kAvailable);
    Shell::Get()->tray_action()->FlushMojoForTesting();
    // Run the loop so the lock screen note display state handler picks up
    // initial screen brightness.
    base::RunLoop().RunUntilIdle();
    power_manager_observer_->ClearBrightnessChanges();

    // Advance the tick clock so it's not close to the null clock value.
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(10000));
  }
  void TearDown() override {
    power_manager_observer_.reset();
    AshTestBase::TearDown();
    chromeos::DBusThreadManager::Shutdown();
  }

  bool LaunchTimeoutRunning() {
    return Shell::Get()
        ->tray_action()
        ->lock_screen_note_display_state_handler_for_test()
        ->launch_timer_for_test()
        ->IsRunning();
  }

  bool TriggerNoteLaunchTimeout() {
    base::OneShotTimer* timer =
        Shell::Get()
            ->tray_action()
            ->lock_screen_note_display_state_handler_for_test()
            ->launch_timer_for_test();
    if (!timer->IsRunning())
      return false;

    timer->FireNow();
    return true;
  }

  void TurnScreenOffForUserInactivity() {
    power_manager_client_->set_screen_brightness_percent(0);
    power_manager::BacklightBrightnessChange change;
    change.set_percent(0.0);
    change.set_cause(power_manager::BacklightBrightnessChange_Cause_OTHER);
    power_manager_client_->SendScreenBrightnessChanged(change);
    power_manager_observer_->ClearBrightnessChanges();
  }

  void SimulatePowerButtonPress() {
    power_manager_client_->SendPowerButtonEvent(true, tick_clock_.NowTicks());
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
    power_manager_client_->SendPowerButtonEvent(false, tick_clock_.NowTicks());
    base::RunLoop().RunUntilIdle();
  }

  testing::AssertionResult SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin expected_note_origin) {
    Shell::Get()->tray_action()->FlushMojoForTesting();

    if (tray_action_client_.note_origins().size() != 1) {
      return testing::AssertionFailure()
             << "Expected a single note action request, found "
             << tray_action_client_.note_origins().size();
    }

    if (tray_action_client_.note_origins()[0] != expected_note_origin) {
      return testing::AssertionFailure()
             << "Unexpected note request origin: "
             << tray_action_client_.note_origins()[0]
             << "; expected: " << expected_note_origin;
    }

    Shell::Get()->tray_action()->UpdateLockScreenNoteState(
        mojom::TrayActionState::kLaunching);
    return testing::AssertionSuccess();
  }

 protected:
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;
  std::unique_ptr<TestPowerManagerObserver> power_manager_observer_;
  TestTrayActionClient tray_action_client_;

 private:
  void InitializeTabletPowerButtonState() {
    // Initialize the tablet power button controller only if the tablet mode
    // switch is set.
    Shell::Get()->power_button_controller()->OnGetSwitchStates(
        chromeos::PowerManagerClient::SwitchStates{
            chromeos::PowerManagerClient::LidState::OPEN,
            chromeos::PowerManagerClient::TabletMode::ON});
    Shell::Get()->power_button_controller()->OnTabletModeStarted();
  }

  base::SimpleTestTickClock tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenNoteDisplayStateHandlerTest);
};

TEST_F(LockScreenNoteDisplayStateHandlerTest, EjectWhenScreenOn) {
  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest, EjectWhenScreenOff) {
  TurnScreenOffForUserInactivity();

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({0.0}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest,
       EjectWhenScreenOffAndNoteNotAvailable) {
  TurnScreenOffForUserInactivity();

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  // Styluls eject is expected to turn the screen on due to user activity.
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  EXPECT_TRUE(tray_action_client_.note_origins().empty());

  // Verify that later (unrelated) note launch does not affect power manager
  // state.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kLaunching);
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest, TurnScreenOnWhenAppLaunchFails) {
  TurnScreenOffForUserInactivity();

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({0.0}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

// Tests stylus eject when the backlights was turned off by a power button
// getting pressed - in particular, it makes sure that power button display
// controller cancelling backlights forced off on stylus eject does not happen
// before lock screen note display state handler requests backlights to be
// forced off (i.e. that backlights are continuosly kept forced off).
TEST_F(LockScreenNoteDisplayStateHandlerTest, EjectWhileScreenForcedOff) {
  SimulatePowerButtonPress();

  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({0.0}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest, DisplayNotTurnedOffIndefinitely) {
  TurnScreenOffForUserInactivity();

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({0.0}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  ASSERT_TRUE(TriggerNoteLaunchTimeout());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

// Test that verifies that lock screen note request is delayed until screen is
// turned off if stylus eject happens soon after power button is pressed, while
// display configuration to off is still in progress.
TEST_F(LockScreenNoteDisplayStateHandlerTest,
       StylusEjectWhileForcingDisplayOff) {
  power_manager_client_
      ->set_enqueue_brightness_changes_on_backlights_forced_off(true);

  SimulatePowerButtonPress();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_manager_observer_->brightness_changes().empty());
  EXPECT_EQ(1u,
            power_manager_client_->pending_screen_brightness_changes().size());

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  // Verify that there are no note requests when the screen brightness due to
  // backlights being forced off is still being updated.
  Shell::Get()->tray_action()->FlushMojoForTesting();
  EXPECT_TRUE(tray_action_client_.note_origins().empty());

  // Apply screen brightness set by forcing backlights off,
  EXPECT_EQ(1u,
            power_manager_client_->pending_screen_brightness_changes().size());
  ASSERT_TRUE(power_manager_client_->ApplyPendingScreenBrightnessChange());

  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(
      power_manager_client_->pending_screen_brightness_changes().empty());
  EXPECT_EQ(std::vector<double>({0.0}),
            power_manager_observer_->brightness_changes());
  power_manager_observer_->ClearBrightnessChanges();

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  ASSERT_TRUE(power_manager_client_->ApplyPendingScreenBrightnessChange());
  EXPECT_EQ(std::vector<double>({kVisibleBrightnessPercent}),
            power_manager_observer_->brightness_changes());

  ASSERT_FALSE(LaunchTimeoutRunning());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest, ScreenA11yAlerts) {
  TestAccessibilityControllerClient a11y_client;
  AccessibilityController* a11y_controller =
      Shell::Get()->accessibility_controller();
  a11y_controller->SetClient(a11y_client.CreateInterfacePtrAndBind());

  SimulatePowerButtonPress();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  a11y_controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::SCREEN_OFF,
            a11y_client.last_a11y_alert());

  ws::InputDeviceClientTestApi devices_test_api;
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(SimulateNoteLaunchStartedIfNoteActionRequested(
      mojom::LockScreenNoteOrigin::kStylusEject));
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  // Screen ON alert is delayed until the screen is turned on after lock screen
  // note launch.
  a11y_controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::SCREEN_OFF,
            a11y_client.last_a11y_alert());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  base::RunLoop().RunUntilIdle();

  // Verify that screen on a11y alert has been sent.
  a11y_controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::SCREEN_ON,
            a11y_client.last_a11y_alert());
}

TEST_F(LockScreenNoteDisplayStateHandlerTest,
       PowerButtonPressClosesLockScreenNote) {
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);

  SimulatePowerButtonPress();

  Shell::Get()->tray_action()->FlushMojoForTesting();
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kScreenDimmed}),
            tray_action_client_.close_note_reasons());
}

}  // namespace ash
