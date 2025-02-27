// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script.h"

namespace autofill_assistant {

ScriptHandle::ScriptHandle() : autostart(false) {}

ScriptHandle::~ScriptHandle() = default;

Script::Script() : priority(0) {}

Script::~Script() = default;

}  // namespace autofill_assistant.
