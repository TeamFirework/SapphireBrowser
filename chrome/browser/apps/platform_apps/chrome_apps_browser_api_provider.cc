// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/chrome_apps_browser_api_provider.h"

#include "chrome/browser/apps/platform_apps/api/generated_api_registration.h"

namespace apps {

ChromeAppsBrowserAPIProvider::ChromeAppsBrowserAPIProvider() = default;
ChromeAppsBrowserAPIProvider::~ChromeAppsBrowserAPIProvider() = default;

void ChromeAppsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  apps::api::ChromeAppsGeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace apps
