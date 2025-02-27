// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/common/safe_browsing/download_file_types.pb.h"
#include "components/download/public/common/download_item.h"

namespace gfx {
class FontList;
}

// Implementation of DownloadUIModel that wrappers around a |DownloadItem*|. As
// such, the caller is expected to ensure that the |download| passed into the
// constructor outlives this |DownloadItemModel|. In addition, multiple
// DownloadItemModel objects could be wrapping the same DownloadItem.
class DownloadItemModel : public DownloadUIModel,
                          public download::DownloadItem::Observer {
 public:
  // Constructs a DownloadItemModel. The caller must ensure that |download|
  // outlives this object.
  explicit DownloadItemModel(download::DownloadItem* download);
  ~DownloadItemModel() override;

  // DownloadUIModel implementation.
  void AddObserver(DownloadUIModel::Observer* observer) override;
  void RemoveObserver(DownloadUIModel::Observer* observer) override;
  base::string16 GetInterruptReasonText() const override;
  base::string16 GetStatusText() const override;
  base::string16 GetTabProgressStatusText() const override;
  base::string16 GetTooltipText(const gfx::FontList& font_list,
                                int max_width) const override;
  base::string16 GetWarningText(const gfx::FontList& font_list,
                                int base_width) const override;
  base::string16 GetWarningConfirmButtonText() const override;
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool IsDangerous() const override;
  bool MightBeMalicious() const override;
  bool IsMalicious() const override;
  bool HasSupportedImageMimeType() const override;
  bool ShouldAllowDownloadFeedback() const override;
  bool ShouldRemoveFromShelfWhenComplete() const override;
  bool ShouldShowDownloadStartedAnimation() const override;
  bool ShouldShowInShelf() const override;
  void SetShouldShowInShelf(bool should_show) override;
  bool ShouldNotifyUI() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;
  bool ShouldPreferOpeningInBrowser() const override;
  void SetShouldPreferOpeningInBrowser(bool preference) override;
  safe_browsing::DownloadFileType::DangerLevel GetDangerLevel() const override;
  void SetDangerLevel(
      safe_browsing::DownloadFileType::DangerLevel danger_level) override;
  void OpenUsingPlatformHandler() override;
  bool IsBeingRevived() const override;
  void SetIsBeingRevived(bool is_being_revived) override;
  download::DownloadItem* download() override;
  base::string16 GetProgressSizesString() const override;

  base::FilePath GetFileNameToReportUser() const override;
  base::FilePath GetTargetFilePath() const override;
  void OpenDownload() override;
  download::DownloadItem::DownloadState GetState() const override;
  bool IsPaused() const override;
  download::DownloadDangerType GetDangerType() const override;
  bool GetOpenWhenComplete() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  bool GetOpened() const override;
  void SetOpened(bool opened) override;
#if !defined(OS_ANDROID)
  DownloadCommands GetDownloadCommands() const override;
#endif

  // download::DownloadItem::Observer implementation.
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

 private:
  // Returns a string indicating the status of an in-progress download.
  base::string16 GetInProgressStatusString() const;

  // The DownloadItem that this model represents. Note that DownloadItemModel
  // itself shouldn't maintain any state since there can be more than one
  // DownloadItemModel in use with the same DownloadItem.
  download::DownloadItem* download_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
