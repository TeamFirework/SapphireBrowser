// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/tab_switcher_mode.h"

#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabSwitcherMode GetTabSwitcherMode() {
  if (IsUIRefreshPhase1Enabled()) {
    return TabSwitcherMode::GRID;
  } else if (IsIPadIdiom()) {
    return TabSwitcherMode::TABLET_SWITCHER;
  } else {
    return TabSwitcherMode::STACK;
  }
}
