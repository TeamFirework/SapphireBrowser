// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace extensions {

// static
AutomationEventRouter* AutomationEventRouter::GetInstance() {
  return base::Singleton<
      AutomationEventRouter,
      base::LeakySingletonTraits<AutomationEventRouter>>::get();
}

AutomationEventRouter::AutomationEventRouter()
    : active_profile_(ProfileManager::GetActiveUserProfile()) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

AutomationEventRouter::~AutomationEventRouter() {
}

void AutomationEventRouter::RegisterListenerForOneTree(
    const ExtensionId& extension_id,
    int listener_process_id,
    int source_ax_tree_id) {
  Register(extension_id,
           listener_process_id,
           source_ax_tree_id,
           false);
}

void AutomationEventRouter::RegisterListenerWithDesktopPermission(
    const ExtensionId& extension_id,
    int listener_process_id) {
  Register(extension_id,
           listener_process_id,
           api::automation::kDesktopTreeID,
           true);
}

void AutomationEventRouter::DispatchAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle) {
  if (active_profile_ != ProfileManager::GetActiveUserProfile()) {
    active_profile_ = ProfileManager::GetActiveUserProfile();
    UpdateActiveProfile();
  }

  for (const auto& listener : listeners_) {
    // Skip listeners that don't want to listen to this tree.
    if (!listener.desktop && listener.tree_ids.find(event_bundle.tree_id) ==
                                 listener.tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);
    rph->Send(new ExtensionMsg_AccessibilityEventBundle(
        event_bundle, listener.is_active_profile));
  }
}

void AutomationEventRouter::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  for (const auto& listener : listeners_) {
    // Skip listeners that don't want to listen to this tree.
    if (!listener.desktop &&
        listener.tree_ids.find(params.tree_id) == listener.tree_ids.end()) {
      continue;
    }

    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);
    rph->Send(new ExtensionMsg_AccessibilityLocationChange(params));
  }
}

void AutomationEventRouter::DispatchTreeDestroyedEvent(
    int tree_id,
    content::BrowserContext* browser_context) {
  if (listeners_.empty())
    return;

  browser_context = browser_context ? browser_context : active_profile_;
  std::unique_ptr<base::ListValue> args(
      api::automation_internal::OnAccessibilityTreeDestroyed::Create(tree_id));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACCESSIBILITY_TREE_DESTROYED,
      api::automation_internal::OnAccessibilityTreeDestroyed::kEventName,
      std::move(args), browser_context);
  EventRouter::Get(browser_context)->BroadcastEvent(std::move(event));

  if (tree_destroyed_callback_for_test_)
    tree_destroyed_callback_for_test_.Run(tree_id);
}

void AutomationEventRouter::DispatchActionResult(const ui::AXActionData& data,
                                                 bool result) {
  CHECK(!data.source_extension_id.empty());

  if (listeners_.empty())
    return;

  std::unique_ptr<base::ListValue> args(
      api::automation_internal::OnActionResult::Create(
          data.target_tree_id, data.request_id, result));
  auto event = std::make_unique<Event>(
      events::AUTOMATION_INTERNAL_ON_ACTION_RESULT,
      api::automation_internal::OnActionResult::kEventName, std::move(args),
      active_profile_);
  EventRouter::Get(active_profile_)
      ->DispatchEventToExtension(data.source_extension_id, std::move(event));
}

void AutomationEventRouter::SetTreeDestroyedCallbackForTest(
    base::RepeatingCallback<void(int)> cb) {
  tree_destroyed_callback_for_test_ = cb;
}

AutomationEventRouter::AutomationListener::AutomationListener() {
}

AutomationEventRouter::AutomationListener::AutomationListener(
    const AutomationListener& other) = default;

AutomationEventRouter::AutomationListener::~AutomationListener() {
}

void AutomationEventRouter::Register(
    const ExtensionId& extension_id,
    int listener_process_id,
    int ax_tree_id,
    bool desktop) {
  auto iter =
      std::find_if(listeners_.begin(), listeners_.end(),
                   [listener_process_id](const AutomationListener& item) {
                     return item.process_id == listener_process_id;
                   });

  // Add a new entry if we don't have one with that process.
  if (iter == listeners_.end()) {
    AutomationListener listener;
    listener.extension_id = extension_id;
    listener.process_id = listener_process_id;
    listener.desktop = desktop;
    listener.tree_ids.insert(ax_tree_id);
    listeners_.push_back(listener);
    UpdateActiveProfile();
    return;
  }

  // We have an entry with that process so update the set of tree ids it wants
  // to listen to, and update its desktop permission.
  iter->tree_ids.insert(ax_tree_id);
  if (desktop)
    iter->desktop = true;
}

void AutomationEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != content::NOTIFICATION_RENDERER_PROCESS_TERMINATED &&
      type != content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    NOTREACHED();
    return;
  }

  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  int process_id = rph->GetID();
  base::EraseIf(listeners_, [process_id](const AutomationListener& item) {
    return item.process_id == process_id;
  });
  UpdateActiveProfile();
}

void AutomationEventRouter::UpdateActiveProfile() {
  for (auto& listener : listeners_) {
#if defined(OS_CHROMEOS)
    int extension_id_count = 0;
    for (const auto& listener2 : listeners_) {
      if (listener2.extension_id == listener.extension_id)
        extension_id_count++;
    }
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(listener.process_id);

    // The purpose of is_active_profile is to ensure different instances of
    // the same extension running in different profiles don't interfere with
    // one another. If an automation extension is only running in one profile,
    // always mark it as active. If it's running in two or more profiles,
    // only mark one as active.
    listener.is_active_profile = (extension_id_count == 1 ||
                                  rph->GetBrowserContext() == active_profile_);
#else
    listener.is_active_profile = true;
#endif
  }
}

}  // namespace extensions
