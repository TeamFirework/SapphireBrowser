// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_VIEW_H_

#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

// A view to show an icon in the status tray when the device is managed by
// an organization admin. Observes change of the enterprise domain in order to
// show/hide the icon reflecting the latest status.
class ManagedDeviceView : public TrayItemView, public EnterpriseDomainObserver {
 public:
  ManagedDeviceView();
  ~ManagedDeviceView() override;

  // EnterpriseDomainObserver:
  void OnEnterpriseDomainChanged() override;

 private:
  void Update();

  DISALLOW_COPY_AND_ASSIGN(ManagedDeviceView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_VIEW_H_
