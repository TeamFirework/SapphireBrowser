// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_mouse_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_input_monitor_win.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

// From the HID Usage Tables specification.
const USHORT kGenericDesktopPage = 1;
const USHORT kMouseUsage = 2;

class MouseRawInputHandlerWin : public LocalInputMonitorWin::RawInputHandler {
 public:
  MouseRawInputHandlerWin(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::MouseMoveCallback on_mouse_move,
      base::OnceClosure disconnect_callback);
  ~MouseRawInputHandlerWin() override;

  // LocalInputMonitorWin::RawInputHandler implementation.
  bool Register(HWND hwnd) override;
  void Unregister() override;
  void OnInputEvent(const RAWINPUT* input) override;
  void OnError() override;

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalInputMonitor::MouseMoveCallback on_mouse_move_;
  base::OnceClosure disconnect_callback_;

  // Tracks whether the instance is registered to receive raw input events.
  bool registered_ = false;

  DISALLOW_COPY_AND_ASSIGN(MouseRawInputHandlerWin);
};

MouseRawInputHandlerWin::MouseRawInputHandlerWin(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::MouseMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      on_mouse_move_(std::move(on_mouse_move)),
      disconnect_callback_(std::move(disconnect_callback)) {}

MouseRawInputHandlerWin::~MouseRawInputHandlerWin() {
  DCHECK(!registered_);
}

bool MouseRawInputHandlerWin::Register(HWND hwnd) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Register to receive raw keyboard input.
  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_INPUTSINK;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = kMouseUsage;
  device.hwndTarget = hwnd;
  if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed";
    return false;
  }

  registered_ = true;
  return true;
}

void MouseRawInputHandlerWin::Unregister() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  RAWINPUTDEVICE device = {0};
  device.dwFlags = RIDEV_REMOVE;
  device.usUsagePage = kGenericDesktopPage;
  device.usUsage = kMouseUsage;
  device.hwndTarget = nullptr;

  // The error is harmless, ignore it.
  RegisterRawInputDevices(&device, 1, sizeof(device));
  registered_ = false;
}

void MouseRawInputHandlerWin::OnInputEvent(const RAWINPUT* input) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Notify the observer about mouse events generated locally. Remote (injected)
  // mouse events do not specify a device handle (based on observed behavior).
  if (input->header.dwType == RIM_TYPEMOUSE &&
      input->header.hDevice != nullptr) {
    POINT position;
    if (!GetCursorPos(&position)) {
      position.x = 0;
      position.y = 0;
    }

    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_mouse_move_, webrtc::DesktopVector(
                                                      position.x, position.y)));
  }
}

void MouseRawInputHandlerWin::OnError() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (disconnect_callback_) {
    caller_task_runner_->PostTask(FROM_HERE, std::move(disconnect_callback_));
  }
}

class LocalMouseInputMonitorWin : public LocalMouseInputMonitor {
 public:
  explicit LocalMouseInputMonitorWin(
      std::unique_ptr<LocalInputMonitorWin> local_input_monitor);
  ~LocalMouseInputMonitorWin() override;

 private:
  std::unique_ptr<LocalInputMonitorWin> local_input_monitor_;
};

LocalMouseInputMonitorWin::LocalMouseInputMonitorWin(
    std::unique_ptr<LocalInputMonitorWin> local_input_monitor)
    : local_input_monitor_(std::move(local_input_monitor)) {}

LocalMouseInputMonitorWin::~LocalMouseInputMonitorWin() = default;

}  // namespace

std::unique_ptr<LocalMouseInputMonitor> LocalMouseInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::MouseMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
  auto raw_input_handler = std::make_unique<MouseRawInputHandlerWin>(
      caller_task_runner, ui_task_runner, std::move(on_mouse_move),
      std::move(disconnect_callback));

  return std::make_unique<LocalMouseInputMonitorWin>(
      LocalInputMonitorWin::Create(caller_task_runner, ui_task_runner,
                                   std::move(raw_input_handler)));
}

}  // namespace remoting
