// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_initializer.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupInitializer::Factory*
    MultiDeviceSetupInitializer::Factory::test_factory_ = nullptr;

// static
MultiDeviceSetupInitializer::Factory*
MultiDeviceSetupInitializer::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupInitializer::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupInitializer::Factory::~Factory() = default;

std::unique_ptr<MultiDeviceSetupBase>
MultiDeviceSetupInitializer::Factory::BuildInstance(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    AuthTokenValidator* auth_token_validator,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate,
    std::unique_ptr<AndroidSmsPairingStateTracker>
        android_sms_pairing_state_tracker,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider) {
  return base::WrapUnique(new MultiDeviceSetupInitializer(
      pref_service, device_sync_client, secure_channel_client,
      auth_token_validator, std::move(android_sms_app_helper_delegate),
      std::move(android_sms_pairing_state_tracker), gcm_device_info_provider));
}

MultiDeviceSetupInitializer::MultiDeviceSetupInitializer(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    AuthTokenValidator* auth_token_validator,
    std::unique_ptr<AndroidSmsAppHelperDelegate>
        android_sms_app_helper_delegate,
    std::unique_ptr<AndroidSmsPairingStateTracker>
        android_sms_pairing_state_tracker,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider)
    : pref_service_(pref_service),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      auth_token_validator_(auth_token_validator),
      android_sms_app_helper_delegate_(
          std::move(android_sms_app_helper_delegate)),
      android_sms_pairing_state_tracker_(
          std::move(android_sms_pairing_state_tracker)),
      gcm_device_info_provider_(gcm_device_info_provider) {
  if (device_sync_client_->is_ready()) {
    InitializeImplementation();
    return;
  }

  device_sync_client_->AddObserver(this);
}

MultiDeviceSetupInitializer::~MultiDeviceSetupInitializer() = default;

void MultiDeviceSetupInitializer::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->SetAccountStatusChangeDelegate(
        std::move(delegate));
    return;
  }

  pending_delegate_ = std::move(delegate);
}

void MultiDeviceSetupInitializer::AddHostStatusObserver(
    mojom::HostStatusObserverPtr observer) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->AddHostStatusObserver(std::move(observer));
    return;
  }

  pending_host_status_observers_.push_back(std::move(observer));
}

void MultiDeviceSetupInitializer::AddFeatureStateObserver(
    mojom::FeatureStateObserverPtr observer) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->AddFeatureStateObserver(std::move(observer));
    return;
  }

  pending_feature_state_observers_.push_back(std::move(observer));
}

void MultiDeviceSetupInitializer::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->GetEligibleHostDevices(std::move(callback));
    return;
  }

  pending_get_eligible_hosts_args_.push_back(std::move(callback));
}

void MultiDeviceSetupInitializer::SetHostDevice(
    const std::string& host_device_id,
    const std::string& auth_token,
    SetHostDeviceCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->SetHostDevice(host_device_id, auth_token,
                                           std::move(callback));
    return;
  }

  // If a pending request to set another device exists, invoke its callback. It
  // is stale, since now an updated request has been made to set the host.
  if (pending_set_host_args_) {
    std::move(std::get<2>(*pending_set_host_args_)).Run(false /* success */);
  }

  // If a pending request to remove the current device exists, cancel it.
  pending_should_remove_host_device_ = false;

  pending_set_host_args_ =
      std::make_tuple(host_device_id, auth_token, std::move(callback));
}

void MultiDeviceSetupInitializer::RemoveHostDevice() {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->RemoveHostDevice();
    return;
  }

  // If a pending request to set another device exists, invoke its callback. It
  // is stale, since now a request has been made to remove the host.
  if (pending_set_host_args_) {
    std::move(std::get<2>(*pending_set_host_args_)).Run(false /* success */);
    pending_set_host_args_.reset();
  }

  pending_should_remove_host_device_ = true;
}

void MultiDeviceSetupInitializer::GetHostStatus(
    GetHostStatusCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->GetHostStatus(std::move(callback));
    return;
  }

  pending_get_host_args_.push_back(std::move(callback));
}

void MultiDeviceSetupInitializer::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const base::Optional<std::string>& auth_token,
    SetFeatureEnabledStateCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->SetFeatureEnabledState(
        feature, enabled, auth_token, std::move(callback));
    return;
  }

  pending_set_feature_enabled_args_.emplace_back(feature, enabled, auth_token,
                                                 std::move(callback));
}

void MultiDeviceSetupInitializer::GetFeatureStates(
    GetFeatureStatesCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->GetFeatureStates(std::move(callback));
    return;
  }

  pending_get_feature_states_args_.emplace_back(std::move(callback));
}

void MultiDeviceSetupInitializer::RetrySetHostNow(
    RetrySetHostNowCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->RetrySetHostNow(std::move(callback));
    return;
  }

  pending_retry_set_host_args_.push_back(std::move(callback));
}

void MultiDeviceSetupInitializer::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  if (multidevice_setup_impl_) {
    multidevice_setup_impl_->TriggerEventForDebugging(type,
                                                      std::move(callback));
    return;
  }

  // If initialization is not complete, no debug event is sent.
  std::move(callback).Run(false /* success */);
}

void MultiDeviceSetupInitializer::OnReady() {
  device_sync_client_->RemoveObserver(this);
  InitializeImplementation();
}

void MultiDeviceSetupInitializer::InitializeImplementation() {
  DCHECK(!multidevice_setup_impl_);

  multidevice_setup_impl_ = MultiDeviceSetupImpl::Factory::Get()->BuildInstance(
      pref_service_, device_sync_client_, secure_channel_client_,
      auth_token_validator_, std::move(android_sms_app_helper_delegate_),
      std::move(android_sms_pairing_state_tracker_), gcm_device_info_provider_);

  if (pending_delegate_) {
    multidevice_setup_impl_->SetAccountStatusChangeDelegate(
        std::move(pending_delegate_));
  }

  for (auto& observer : pending_host_status_observers_)
    multidevice_setup_impl_->AddHostStatusObserver(std::move(observer));
  pending_host_status_observers_.clear();

  for (auto& observer : pending_feature_state_observers_)
    multidevice_setup_impl_->AddFeatureStateObserver(std::move(observer));
  pending_feature_state_observers_.clear();

  if (pending_set_host_args_) {
    DCHECK(!pending_should_remove_host_device_);
    multidevice_setup_impl_->SetHostDevice(
        std::get<0>(*pending_set_host_args_),
        std::get<1>(*pending_set_host_args_),
        std::move(std::get<2>(*pending_set_host_args_)));
    pending_set_host_args_.reset();
  }

  if (pending_should_remove_host_device_)
    multidevice_setup_impl_->RemoveHostDevice();
  pending_should_remove_host_device_ = false;

  for (auto& set_feature_enabled_args : pending_set_feature_enabled_args_) {
    multidevice_setup_impl_->SetFeatureEnabledState(
        std::get<0>(set_feature_enabled_args),
        std::get<1>(set_feature_enabled_args),
        std::get<2>(set_feature_enabled_args),
        std::move(std::get<3>(set_feature_enabled_args)));
  }
  pending_set_feature_enabled_args_.clear();

  for (auto& get_feature_states_callback : pending_get_feature_states_args_) {
    multidevice_setup_impl_->GetFeatureStates(
        std::move(get_feature_states_callback));
  }
  pending_get_feature_states_args_.clear();

  for (auto& retry_callback : pending_retry_set_host_args_)
    multidevice_setup_impl_->RetrySetHostNow(std::move(retry_callback));
  pending_retry_set_host_args_.clear();

  for (auto& get_eligible_callback : pending_get_eligible_hosts_args_) {
    multidevice_setup_impl_->GetEligibleHostDevices(
        std::move(get_eligible_callback));
  }
  pending_get_eligible_hosts_args_.clear();

  for (auto& get_host_callback : pending_get_host_args_)
    multidevice_setup_impl_->GetHostStatus(std::move(get_host_callback));
  pending_get_host_args_.clear();
}

}  // namespace multidevice_setup

}  // namespace chromeos
