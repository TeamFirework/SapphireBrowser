// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class ChangeRequestsStateTask : public Task {
 public:
  ChangeRequestsStateTask(RequestQueueStore* store,
                          const std::vector<int64_t>& request_ids,
                          const SavePageRequest::RequestState new_state,
                          RequestQueueStore::UpdateCallback callback);
  ~ChangeRequestsStateTask() override;

  // TaskQueue::Task implementation.
  void Run() override;

 private:
  // Step 1. Reading the requests.
  void ReadRequests();
  // Step 2. Updates available requests.
  void UpdateRequests(std::unique_ptr<UpdateRequestsResult> read_result);
  // Step 3. Processes update result, calls callback.
  void UpdateCompleted(std::unique_ptr<UpdateRequestsResult> update_result);

  // Store that this task updates.
  RequestQueueStore* store_;
  // Request IDs to be updated. Kept as a set to remove duplicates and simplify
  // the look up of requests that are not found in step 3.
  std::unordered_set<int64_t> request_ids_;
  // New state to be set on all entries.
  SavePageRequest::RequestState new_state_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;

  base::WeakPtrFactory<ChangeRequestsStateTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChangeRequestsStateTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_
