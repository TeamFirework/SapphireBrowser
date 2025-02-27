// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace offline_pages {
class OfflineEventLogger;
class OfflineMetricsCollector;
class OfflinePageModel;
class PrefetchBackgroundTaskHandler;
class PrefetchDispatcher;
class PrefetchDownloader;
class PrefetchGCMHandler;
class PrefetchImporter;
class PrefetchNetworkRequestFactory;
class PrefetchStore;
class SuggestedArticlesObserver;
class SuggestionsProvider;
class ThumbnailFetcher;

// TODO(https://crbug.com/874293): This class is midway through a refactoring so
// it might look as it offers an inconsistent API.
//
// External doc (will remain here):
// Main entry point for the Offline Prefetch feature for external users.
//
// Internal doc (to be eventually moved out of here):
// Main class and entry point for the Offline Pages Prefetching feature, that
// controls the lifetime of all major subcomponents of the prefetching system.
// Setup and creation of concrete instances must be lightweight. All heavy work
// will be delayed to be done on-demand only.
class PrefetchService : public KeyedService {
 public:
  ~PrefetchService() override = default;

  // Externally used functions. They will remain part of this class.

  // Sets the SuggestionsProvider instance. Should be called at startup time and
  // before any other suggestion related calls are made.
  virtual void SetContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService* content_suggestions) = 0;

  // Sets the SuggestionsProvider instance. Should be called at startup time and
  // before any other suggestion related calls are made.
  virtual void SetSuggestionProvider(
      SuggestionsProvider* suggestions_provider) = 0;

  // Notifies that the list of suggestions has changed and contains fresh
  // content. This should be called any time new suggestions are fetched.
  virtual void NewSuggestionsAvailable() = 0;

  // Signals that a specific suggestion was removed due to user action (i.e.
  // user swiped out the item). This will cause the full removal of the
  // suggestion from the Prefetching pipeline and/or the Offline Pages database.
  virtual void RemoveSuggestion(GURL url) = 0;

  virtual PrefetchGCMHandler* GetPrefetchGCMHandler() = 0;

  // Internal usage only functions. They will eventually be moved out of this
  // class.

  // Sub-components that are created and owned by this service.
  // The service manages lifetime, hookup and initialization of Prefetch
  // system that consists of multiple specialized objects, all vended by this
  // class.
  virtual OfflineEventLogger* GetLogger() = 0;
  virtual OfflineMetricsCollector* GetOfflineMetricsCollector() = 0;
  virtual PrefetchDispatcher* GetPrefetchDispatcher() = 0;
  virtual PrefetchNetworkRequestFactory* GetPrefetchNetworkRequestFactory() = 0;
  virtual PrefetchDownloader* GetPrefetchDownloader() = 0;
  virtual PrefetchStore* GetPrefetchStore() = 0;
  virtual PrefetchImporter* GetPrefetchImporter() = 0;
  virtual PrefetchBackgroundTaskHandler* GetPrefetchBackgroundTaskHandler() = 0;
  virtual ThumbnailFetcher* GetThumbnailFetcher() = 0;
  virtual OfflinePageModel* GetOfflinePageModel() = 0;

  // May be |nullptr| in tests.  The PrefetchService does not depend on the
  // SuggestedArticlesObserver, it merely owns it for lifetime purposes.
  virtual SuggestedArticlesObserver* GetSuggestedArticlesObserver() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
