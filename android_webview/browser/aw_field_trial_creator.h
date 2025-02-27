// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIAL_CREATOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIAL_CREATOR_H_

#include <memory>

#include "android_webview/browser/aw_field_trials.h"
#include "android_webview/browser/aw_variations_service_client.h"
#include "base/metrics/field_trial.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_field_trial_creator.h"

namespace android_webview {

// Used by WebView to set up field trials based on the stored variations
// seed data. Once created this object must exist for the lifetime of the
// process as it contains the FieldTrialList that can be queried for the state
// of experiments.
class AwFieldTrialCreator {
 public:
  AwFieldTrialCreator();
  ~AwFieldTrialCreator();

  // Sets up the field trials and related initialization.
  void SetUpFieldTrials(PrefService* pref_service);

 private:
  void DoSetUpFieldTrials(PrefService* pref_service);

  // Stores the seed. VariationsSeedStore keeps a raw pointer to this, so it
  // must persist for the process lifetime. Not persisted accross runs.
  std::unique_ptr<PrefService> local_state_;

  // A/B testing infrastructure for the entire application. empty until
  // |SetupFieldTrials()| is called.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  // Performs set up for any WebView specific field trials.
  std::unique_ptr<AwFieldTrials> aw_field_trials_;

  // Responsible for creating a feature list from the seed.
  std::unique_ptr<variations::VariationsFieldTrialCreator>
      variations_field_trial_creator_;

  std::unique_ptr<AwVariationsServiceClient> client_;

  DISALLOW_COPY_AND_ASSIGN(AwFieldTrialCreator);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIAL_CREATOR_H_
