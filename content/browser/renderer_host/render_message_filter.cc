// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>
#include <string.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media_log_event.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "content/public/common/font_cache_dispatcher_win.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif
#if defined(OS_LINUX)
#include "base/linux_util.h"
#include "base/threading/platform_thread.h"
#endif

using blink::mojom::CacheStorageError;

namespace content {
namespace {

const uint32_t kRenderFilteredMessageClasses[] = {ViewMsgStart};

void NoOpCacheStorageErrorCallback(CacheStorageCacheHandle cache_handle,
                                   CacheStorageError error) {}

base::Optional<url::Origin> GetRendererOrigin(const GURL& url,
                                              int render_process_id) {
  GURL requesting_url =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(
          render_process_id);

  if (!requesting_url.is_valid() || !url.is_valid())
    return base::nullopt;

  url::Origin origin = url::Origin::Create(requesting_url);

  // Don't cache the code corresponding to unique origins. The same-origin
  // checks should always fail for unique origins but the serialized value of
  // unique origins does not ensure this.
  if (origin.unique())
    return base::nullopt;

  return origin;
}

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper,
    MediaInternals* media_internals,
    CacheStorageContextImpl* cache_storage_context,
    GeneratedCodeCacheContext* generated_code_cache_context)
    : BrowserMessageFilter(kRenderFilteredMessageClasses,
                           arraysize(kRenderFilteredMessageClasses)),
      BrowserAssociatedInterface<mojom::RenderMessageFilter>(this, this),
      resource_dispatcher_host_(ResourceDispatcherHostImpl::Get()),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      render_process_id_(render_process_id),
      media_internals_(media_internals),
      cache_storage_context_(cache_storage_context),
      generated_code_cache_context_(generated_code_cache_context),
      weak_ptr_factory_(this) {
  DCHECK(request_context_.get());

  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaLogEvents, OnMediaLogEvents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderMessageFilter::OnDestruct() const {
  const_cast<RenderMessageFilter*>(this)->resource_context_ = nullptr;
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                   BrowserThread::ID* thread) {
  if (message.type() == ViewHostMsg_MediaLogEvents::ID)
    *thread = BrowserThread::UI;
}

void RenderMessageFilter::GenerateRoutingID(
    GenerateRoutingIDCallback callback) {
  std::move(callback).Run(render_widget_helper_->GetNextRoutingID());
}

void RenderMessageFilter::CreateNewWidget(int32_t opener_id,
                                          mojom::WidgetPtr widget,
                                          CreateNewWidgetCallback callback) {
  int route_id = MSG_ROUTING_NONE;
  render_widget_helper_->CreateNewWidget(opener_id, std::move(widget),
                                         &route_id);
  std::move(callback).Run(route_id);
}

void RenderMessageFilter::CreateFullscreenWidget(
    int opener_id,
    mojom::WidgetPtr widget,
    CreateFullscreenWidgetCallback callback) {
  int route_id = 0;
  render_widget_helper_->CreateNewFullscreenWidget(opener_id, std::move(widget),
                                                   &route_id);
  std::move(callback).Run(route_id);
}

#if defined(OS_LINUX)
void RenderMessageFilter::SetThreadPriorityOnFileThread(
    base::PlatformThreadId ns_tid,
    base::ThreadPriority priority) {
  bool ns_pid_supported = false;
  pid_t peer_tid = base::FindThreadID(peer_pid(), ns_tid, &ns_pid_supported);
  if (peer_tid == -1) {
    if (ns_pid_supported)
      DLOG(WARNING) << "Could not find tid";
    return;
  }

  if (peer_tid == peer_pid()) {
    DLOG(WARNING) << "Changing priority of main thread is not allowed";
    return;
  }

  base::PlatformThread::SetThreadPriority(peer_tid, priority);
}
#endif

#if defined(OS_LINUX)
void RenderMessageFilter::SetThreadPriority(int32_t ns_tid,
                                            base::ThreadPriority priority) {
  constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_BLOCKING,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  base::PostTaskWithTraits(
      FROM_HERE, kTraits,
      base::BindOnce(&RenderMessageFilter::SetThreadPriorityOnFileThread, this,
                     static_cast<base::PlatformThreadId>(ns_tid), priority));
}
#endif

void RenderMessageFilter::DidGenerateCacheableMetadata(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<uint8_t>& data) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::RMF_BAD_URL_CACHEABLE_METADATA);
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(features::kIsolatedCodeCache)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&RenderMessageFilter::DidGenerateCacheableMetadataOnUI,
                       this, url, expected_response_time, data));
  } else {
    if (!generated_code_cache_context_ ||
        !generated_code_cache_context_->generated_code_cache())
      return;

    base::Optional<url::Origin> requesting_origin =
        GetRendererOrigin(url, render_process_id_);
    if (!requesting_origin)
      return;

    generated_code_cache_context_->generated_code_cache()->WriteData(
        url, *requesting_origin, expected_response_time, data);
  }
}

void RenderMessageFilter::FetchCachedCode(const GURL& url,
                                          FetchCachedCodeCallback callback) {
  if (!generated_code_cache_context_ ||
      !generated_code_cache_context_->generated_code_cache()) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  base::Optional<url::Origin> requesting_origin =
      GetRendererOrigin(url, render_process_id_);
  if (!requesting_origin) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  base::RepeatingCallback<void(const base::Time&, const std::vector<uint8_t>&)>
      read_callback = base::BindRepeating(
          &RenderMessageFilter::OnReceiveCachedCode,
          weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback));
  generated_code_cache_context_->generated_code_cache()->FetchEntry(
      url, *requesting_origin, read_callback);
}

void RenderMessageFilter::OnReceiveCachedCode(
    FetchCachedCodeCallback callback,
    const base::Time& response_time,
    const std::vector<uint8_t>& data) {
  // TODO(crbug.com/867848): Pass the data as a mojo data pipe instead
  // of vector<uint8>
  std::move(callback).Run(response_time, data);
}

void RenderMessageFilter::ClearCodeCacheEntry(const GURL& url) {
  if (!generated_code_cache_context_ ||
      !generated_code_cache_context_->generated_code_cache())
    return;

  base::Optional<url::Origin> requesting_origin =
      GetRendererOrigin(url, render_process_id_);
  if (!requesting_origin)
    return;

  generated_code_cache_context_->generated_code_cache()->DeleteEntry(
      url, *requesting_origin);
}

void RenderMessageFilter::DidGenerateCacheableMetadataInCacheStorage(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<uint8_t>& data,
    const url::Origin& cache_storage_origin,
    const std::string& cache_storage_cache_name) {
  scoped_refptr<net::IOBuffer> buf =
      base::MakeRefCounted<net::IOBuffer>(data.size());
  if (!data.empty())
    memcpy(buf->data(), &data.front(), data.size());

  cache_storage_context_->cache_manager()->OpenCache(
      cache_storage_origin, CacheStorageOwner::kCacheAPI,
      cache_storage_cache_name,
      base::BindOnce(&RenderMessageFilter::OnCacheStorageOpenCallback,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     expected_response_time, buf, data.size()));
}

void RenderMessageFilter::OnCacheStorageOpenCallback(
    const GURL& url,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buf,
    int buf_len,
    CacheStorageCacheHandle cache_handle,
    CacheStorageError error) {
  if (error != CacheStorageError::kSuccess || !cache_handle.value())
    return;
  CacheStorageCache* cache = cache_handle.value();
  cache->WriteSideData(
      base::BindOnce(&NoOpCacheStorageErrorCallback, std::move(cache_handle)),
      url, expected_response_time, buf, buf_len);
}

void RenderMessageFilter::OnMediaLogEvents(
    const std::vector<media::MediaLogEvent>& events) {
  // OnMediaLogEvents() is always dispatched to the UI thread for handling.
  // See OverrideThreadForMessage().
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (media_internals_)
    media_internals_->OnMediaEvents(render_process_id_, events);
}

void RenderMessageFilter::HasGpuProcess(HasGpuProcessCallback callback) {
  GpuProcessHost::GetHasGpuProcess(std::move(callback));
}

void RenderMessageFilter::DidGenerateCacheableMetadataOnUI(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<uint8_t>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (!host)
    return;

  // Use the same priority for the metadata write as for script
  // resources (see defaultPriorityForResourceType() in WebKit's
  // CachedResource.cpp). Note that WebURLRequest::PriorityMedium
  // corresponds to net::LOW (see ConvertWebKitPriorityToNetPriority()
  // in weburlloader_impl.cc).
  const net::RequestPriority kPriority = net::LOW;
  host->GetStoragePartition()->GetNetworkContext()->WriteCacheMetadata(
      url, kPriority, expected_response_time, data);
}

}  // namespace content
