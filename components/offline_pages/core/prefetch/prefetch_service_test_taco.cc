// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/mock_thumbnail_fetcher.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/test_download_client.h"
#include "components/offline_pages/core/prefetch/test_download_service.h"
#include "components/offline_pages/core/prefetch/test_offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/test_prefetch_importer.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/offline_pages/core/stub_offline_page_model.h"

namespace offline_pages {

namespace {

const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;

class StubPrefetchBackgroundTaskHandler : public PrefetchBackgroundTaskHandler {
 public:
  StubPrefetchBackgroundTaskHandler() = default;
  ~StubPrefetchBackgroundTaskHandler() override = default;
  void CancelBackgroundTask() override {}
  void EnsureTaskScheduled() override {}
  void Backoff() override {}
  void ResetBackoff() override {}
  void PauseBackoffUntilNextRun() override {}
  void Suspend() override {}
  void RemoveSuspension() override {}
  int GetAdditionalBackoffSeconds() const override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPrefetchBackgroundTaskHandler);
};

}  // namespace

PrefetchServiceTestTaco::PrefetchServiceTestTaco() {
  dispatcher_ = std::make_unique<TestPrefetchDispatcher>();
  metrics_collector_ = std::make_unique<TestOfflineMetricsCollector>(nullptr);
  gcm_handler_ = std::make_unique<TestPrefetchGCMHandler>();
  network_request_factory_ =
      std::make_unique<TestPrefetchNetworkRequestFactory>();
  prefetch_store_ =
      std::make_unique<PrefetchStore>(base::ThreadTaskRunnerHandle::Get());
  suggested_articles_observer_ = std::make_unique<SuggestedArticlesObserver>();
  download_service_ = std::make_unique<TestDownloadService>();
  prefetch_downloader_ = base::WrapUnique(
      new PrefetchDownloaderImpl(download_service_.get(), kTestChannel));
  download_client_ =
      std::make_unique<TestDownloadClient>(prefetch_downloader_.get());
  download_service_->SetClient(download_client_.get());
  prefetch_importer_ = std::make_unique<TestPrefetchImporter>();
  // This sets up the testing articles as an empty vector, we can ignore the
  // result here.  This allows us to not create a ContentSuggestionsService.
  suggested_articles_observer_->GetTestingArticles();
  prefetch_background_task_handler_ =
      std::make_unique<StubPrefetchBackgroundTaskHandler>();
  offline_page_model_ = std::make_unique<StubOfflinePageModel>();
  thumbnail_fetcher_ = std::make_unique<MockThumbnailFetcher>();
}

PrefetchServiceTestTaco::~PrefetchServiceTestTaco() = default;

void PrefetchServiceTestTaco::SetOfflineMetricsCollector(
    std::unique_ptr<OfflineMetricsCollector> metrics_collector) {
  CHECK(!prefetch_service_);
  CHECK(metrics_collector);
  metrics_collector_ = std::move(metrics_collector);
}

void PrefetchServiceTestTaco::SetPrefetchDispatcher(
    std::unique_ptr<PrefetchDispatcher> dispatcher) {
  CHECK(!prefetch_service_);
  CHECK(dispatcher);
  dispatcher_ = std::move(dispatcher);
}

void PrefetchServiceTestTaco::SetPrefetchGCMHandler(
    std::unique_ptr<PrefetchGCMHandler> gcm_handler) {
  CHECK(!prefetch_service_);
  CHECK(gcm_handler);
  gcm_handler_ = std::move(gcm_handler);
}

void PrefetchServiceTestTaco::SetPrefetchNetworkRequestFactory(
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory) {
  CHECK(!prefetch_service_);
  CHECK(network_request_factory);
  network_request_factory_ = std::move(network_request_factory);
}

void PrefetchServiceTestTaco::SetPrefetchStore(
    std::unique_ptr<PrefetchStore> prefetch_store) {
  CHECK(!prefetch_service_);
  CHECK(prefetch_store);
  prefetch_store_ = std::move(prefetch_store);
}

void PrefetchServiceTestTaco::SetSuggestedArticlesObserver(
    std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer) {
  CHECK(!prefetch_service_);
  suggested_articles_observer_ = std::move(suggested_articles_observer);
}

void PrefetchServiceTestTaco::SetPrefetchDownloader(
    std::unique_ptr<PrefetchDownloader> prefetch_downloader) {
  CHECK(!prefetch_service_);
  CHECK(prefetch_downloader);
  prefetch_downloader_ = std::move(prefetch_downloader);
}

void PrefetchServiceTestTaco::SetPrefetchImporter(
    std::unique_ptr<PrefetchImporter> prefetch_importer) {
  CHECK(!prefetch_service_);
  prefetch_importer_ = std::move(prefetch_importer);
}

void PrefetchServiceTestTaco::SetPrefetchBackgroundTaskHandler(
    std::unique_ptr<PrefetchBackgroundTaskHandler>
        prefetch_background_task_handler) {
  CHECK(!prefetch_service_);
  prefetch_background_task_handler_ =
      std::move(prefetch_background_task_handler);
}

void PrefetchServiceTestTaco::SetThumbnailFetcher(
    std::unique_ptr<ThumbnailFetcher> thumbnail_fetcher) {
  CHECK(!prefetch_service_);
  thumbnail_fetcher_ = std::move(thumbnail_fetcher);
}

void PrefetchServiceTestTaco::SetOfflinePageModel(
    std::unique_ptr<OfflinePageModel> offline_page_model) {
  CHECK(!prefetch_service_);
  offline_page_model_ = std::move(offline_page_model);
}

void PrefetchServiceTestTaco::CreatePrefetchService() {
  CHECK(!prefetch_service_);
  prefetch_service_ = std::make_unique<PrefetchServiceImpl>(
      std::move(metrics_collector_), std::move(dispatcher_),
      std::move(gcm_handler_), std::move(network_request_factory_),
      offline_page_model_.get(), std::move(prefetch_store_),
      std::move(suggested_articles_observer_), std::move(prefetch_downloader_),
      std::move(prefetch_importer_),
      std::move(prefetch_background_task_handler_),
      std::move(thumbnail_fetcher_));
}

std::unique_ptr<PrefetchService>
PrefetchServiceTestTaco::CreateAndReturnPrefetchService() {
  CreatePrefetchService();
  return std::move(prefetch_service_);
}

}  // namespace offline_pages
