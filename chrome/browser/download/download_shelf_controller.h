// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_

#include <map>

#include "base/macros.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

class Profile;

using ContentId = offline_items_collection::ContentId;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineItem = offline_items_collection::OfflineItem;

// Class for notifying UI when an OfflineItem should be displayed.
class DownloadShelfController : public OfflineContentProvider::Observer {
 public:
  explicit DownloadShelfController(Profile* profile);
  ~DownloadShelfController() override;

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item) override;

  // Called when a new OfflineItem is to be displayed on UI.
  void OnNewOfflineItemReady(const OfflineItemModel& item);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfController);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_
