// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_

namespace arc {

// Defines ARC App user interaction types to track how users use ARC apps.
// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with tools/metrics/histograms/enums.xml.
enum class UserInteractionType {
  // Default to not user-initiated.
  // Can be used temporarily for a new action path or to denote an action
  // that was not directly user-initiated.
  NOT_USER_INITIATED = 0,

  // User started an app from the launcher.
  APP_STARTED_FROM_LAUNCHER = 1,

  // User started an app from a context menu click in the launcher.
  APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU = 2,

  // User started an app from a launcher search result.
  APP_STARTED_FROM_LAUNCHER_SEARCH = 3,

  // User started an app from a a context menu click on a search result.
  APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU = 4,

  // User started a suggested app in the launcher.
  APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP = 5,

  // User started a suggested app using the context menu in the launcher.
  APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP_CONTEXT_MENU = 6,

  // User started an app from the shelf.
  APP_STARTED_FROM_SHELF = 7,

  // User started an app from the shelf using the context menu.
  // TODO(crbug.com/862901): Record this separately from APP_STARTED_FROM_SHELF
  APP_STARTED_FROM_SHELF_CONTEXT_MENU = 8,

  // User started an app from settings.
  APP_STARTED_FROM_SETTINGS = 9,

  // User interacted with an ARC++ notification. Dismissal of notifications such
  // as closing and swiping out are not being considered.
  NOTIFICATION_INTERACTION = 10,

  // User interacted with the content window.
  APP_CONTENT_WINDOW_INTERACTION = 11,

  // User started an app from chrome.arcAppsPrivate.launchApp.
  APP_STARTED_FROM_EXTENSION_API = 12,

  // User started note-taking app from stylus tools.
  APP_STARTED_FROM_STYLUS_TOOLS = 13,

  // User started an app by opening files in the file manager.
  APP_STARTED_FROM_FILE_MANAGER = 14,

  // User started an app by left-clicking on links in the browser.
  APP_STARTED_FROM_LINK = 15,

  // User started an app from context menu by right-clicking on links in the
  // browser.
  APP_STARTED_FROM_LINK_CONTEXT_MENU = 16,

  // User started an app from Smart Text Selection context menu.
  APP_STARTED_FROM_SMART_TEXT_SELECTION_CONTEXT_MENU = 17,

  // The size of this enum; keep last.
  SIZE,
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_CONSTANTS_H_
