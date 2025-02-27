# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/android/webapk/shell_apk:webapk

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.

This presubmit checks for two rules:
1. If anything in the webapk/libs/common or the webapk/shell_apk directories
has changed (excluding test files), $WAM_MINT_TRIGGER_VARIABLE should be
updated.
2. If $CHROME_UPDATE_TIRGGER_VARIABLE is changed in
$SHELL_APK_VERSION_LOCAL_PATH, $SHELL_APK_VERSION_LOCAL_PATH should be the
only changed file and changing $CHROME_UPDATE_TRIGGER_VARIABLE should be
the only change.
"""

WAM_MINT_TRIGGER_VARIABLE = r'template_shell_apk_version'
CHROME_UPDATE_TRIGGER_VARIABLE = r'expected_shell_apk_version'

SHELL_APK_VERSION_LOCAL_PATH = r'shell_apk/shell_apk_version.gni'

WAM_MINT_TRIGGER_LOCAL_PATHS = [
    'libs/common/src/',
    'shell_apk/AndroidManifest.xml',
    'shell_apk/res/',
    'shell_apk/src/',
]

def _DoChangedContentsContain(changed_contents, key):
  for _, line in changed_contents:
    if key in line:
      return True
  return False


def _CheckVersionVariableChanged(input_api, version_file_local_path,
                                 variable_name):
  for f in input_api.AffectedFiles():
    local_path = input_api.os_path.relpath(f.AbsoluteLocalPath(),
                                           input_api.PresubmitLocalPath())
    if local_path == version_file_local_path:
      return _DoChangedContentsContain(f.ChangedContents(), variable_name)

  return False


def _CheckChromeUpdateTriggerRule(input_api, output_api):
  """
  Check that if |expected_shell_apk_version| is updated it is the only
  change in the CL.
  """
  if _CheckVersionVariableChanged(input_api, SHELL_APK_VERSION_LOCAL_PATH,
                                  CHROME_UPDATE_TRIGGER_VARIABLE):
    if (len(input_api.AffectedFiles()) != 1 or
        len(input_api.AffectedFiles[0].ChangedContents()) != 1):
      return [
        output_api.PresubmitError(
            '{} in {} must be updated in a standalone CL.'.format(
                CHROME_UPDATE_TRIGGER_VARIABLE,
                SHELL_APK_VERSION_LOCAL_PATH))
        ]
  return []


def _CheckWamMintTriggerRule(input_api, output_api):
  """
  Check that if a file in $WAM_MINT_TRIGGER_LOCAL_PATHS is updated that
  |template_shell_apk_version| is updated as well.
  """
  files_requiring_version_increase = []
  for f in input_api.AffectedFiles():
    local_path = input_api.os_path.relpath(f.AbsoluteLocalPath(),
                                           input_api.PresubmitLocalPath())
    for wam_mint_trigger_local_path in WAM_MINT_TRIGGER_LOCAL_PATHS:
      if local_path.startswith(wam_mint_trigger_local_path):
        files_requiring_version_increase.append(local_path)

  if not files_requiring_version_increase:
    return []

  if not _CheckVersionVariableChanged(input_api, SHELL_APK_VERSION_LOCAL_PATH,
                                      WAM_MINT_TRIGGER_VARIABLE):
    return [output_api.PresubmitPromptWarning(
        '{} in {} needs to updated due to changes in:'.format(
            WAM_MINT_TRIGGER_VARIABLE, SHELL_APK_VERSION_LOCAL_PATH),
        items=files_requiring_version_increase)]

  return []


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckChromeUpdateTriggerRule(input_api, output_api))
  result.extend(_CheckWamMintTriggerRule(input_api, output_api))

  return result


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
