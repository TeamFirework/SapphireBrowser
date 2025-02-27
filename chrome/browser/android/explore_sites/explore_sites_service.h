// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_

#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace explore_sites {

// Main class and entry point for the Explore Sites feature, that
// controls the lifetime of all major subcomponents.
class ExploreSitesService : public KeyedService {
 public:
  ~ExploreSitesService() override = default;

  // Returns via callback the current catalog stored locally.
  virtual void GetCatalog(CatalogCallback callback) = 0;
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_
