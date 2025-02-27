// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_target_client_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"

namespace content {

InputTargetClientImpl::InputTargetClientImpl(RenderFrameImpl* render_frame)
    : render_frame_(render_frame), binding_(this) {}

InputTargetClientImpl::~InputTargetClientImpl() {}

void InputTargetClientImpl::BindToRequest(
    viz::mojom::InputTargetClientRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request), render_frame_->GetTaskRunner(
                                        blink::TaskType::kInternalDefault));
}

void InputTargetClientImpl::FrameSinkIdAt(const gfx::Point& point,
                                          FrameSinkIdAtCallback callback) {
  gfx::PointF local_point;
  viz::FrameSinkId id = render_frame_->GetRenderWidget()->GetFrameSinkIdAtPoint(
      point, &local_point);
  std::move(callback).Run(id, local_point);
}

}  // namespace content
