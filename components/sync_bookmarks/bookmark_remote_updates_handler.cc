// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

namespace sync_bookmarks {

namespace {

// Id is created by concatenating the specifics field number and the server tag
// similar to LookbackServerEntity::CreateId() that uses
// GetSpecificsFieldNumberFromModelType() to compute the field number.
const char kBookmarksRootId[] = "32904_google_chrome_bookmarks";
const char kMobileBookmarksTag[] = "synced_bookmarks";

// Recursive method to traverse a forest created by ReorderUpdates() to to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::StringPiece& node_id,
    const std::unordered_map<base::StringPiece,
                             const syncer::UpdateResponseData*,
                             base::StringPieceHash>& id_to_updates,
    const std::unordered_map<base::StringPiece,
                             std::vector<base::StringPiece>,
                             base::StringPieceHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_id) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::StringPiece& child : node_to_children.at(node_id)) {
    DCHECK_NE(id_to_updates.count(child), 0U);
    ordered_updates->push_back(id_to_updates.at(child));
    TraverseAndAppendChildren(child, id_to_updates, node_to_children,
                              ordered_updates);
  }
}

int ComputeChildNodeIndex(const bookmarks::BookmarkNode* parent,
                          const sync_pb::UniquePosition& unique_position,
                          const SyncedBookmarkTracker* bookmark_tracker) {
  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);
  for (int i = 0; i < parent->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = parent->GetChild(i);
    const SyncedBookmarkTracker::Entity* child_entity =
        bookmark_tracker->GetEntityForBookmarkNode(child);
    DCHECK(child_entity);
    const syncer::UniquePosition child_position =
        syncer::UniquePosition::FromProto(
            child_entity->metadata()->unique_position());
    if (position.LessThan(child_position)) {
      return i;
    }
  }
  return parent->child_count();
}

void ApplyRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity,
    const SyncedBookmarkTracker::Entity* new_parent_tracked_entity,
    bookmarks::BookmarkModel* model,
    SyncedBookmarkTracker* tracker,
    favicon::FaviconService* favicon_service) {
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK(!update_entity.is_deleted());
  DCHECK(tracked_entity);
  DCHECK(new_parent_tracked_entity);
  DCHECK(model);
  DCHECK(tracker);
  DCHECK(favicon_service);
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();
  const bookmarks::BookmarkNode* new_parent =
      new_parent_tracked_entity->bookmark_node();

  if (update_entity.is_folder != node->is_folder()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.is_folder ? "folder" : "bookmark")
                << " while local node is a "
                << (node->is_folder() ? "folder" : "bookmark");
    return;
  }
  UpdateBookmarkNodeFromSpecifics(update_entity.specifics.bookmark(), node,
                                  model, favicon_service);
  // Compute index information before updating the |tracker|.
  const int old_index = old_parent->GetIndexOf(node);
  const int new_index =
      ComputeChildNodeIndex(new_parent, update_entity.unique_position, tracker);
  tracker->Update(update_entity.id, update.response_version,
                  update_entity.modification_time,
                  update_entity.unique_position, update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // BookmarkModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  model->Move(node, new_parent, new_index);
}

}  // namespace

BookmarkRemoteUpdatesHandler::BookmarkRemoteUpdatesHandler(
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker) {
  DCHECK(bookmark_model);
  DCHECK(bookmark_tracker);
  DCHECK(favicon_service);
}

void BookmarkRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates) {
  for (const syncer::UpdateResponseData* update : ReorderUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity.value();
    // Only non deletions and non premanent node should have valid specifics and
    // unique positions.
    if (!update_entity.is_deleted() &&
        update_entity.parent_id != kBookmarksRootId) {
      if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                    update_entity.is_folder)) {
        // Ignore updates with invalid specifics.
        DLOG(ERROR)
            << "Couldn't process an update bookmark with an invalid specifics.";
        continue;
      }
      if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
               .IsValid()) {
        // Ignore updates with invalid unique position.
        DLOG(ERROR) << "Couldn't process an update bookmark with an invalid "
                       "unique position.";
        continue;
      }
    }
    const SyncedBookmarkTracker::Entity* tracked_entity =
        bookmark_tracker_->GetEntityForSyncId(update_entity.id);
    if (tracked_entity && tracked_entity->metadata()->server_version() >=
                              update->response_version) {
      // Seen this update before; just ignore it.
      continue;
    }
    // TODO(crbug.com/516866): Handle the case of conflict as a result of
    // re-encryption request.
    if (tracked_entity && tracked_entity->IsUnsynced()) {
      ProcessConflict(*update, tracked_entity);
      continue;
    }
    if (update_entity.is_deleted()) {
      ProcessDelete(update_entity, tracked_entity);
      continue;
    }
    if (!tracked_entity) {
      ProcessCreate(*update);
      continue;
    }
    // Ignore changes to the permanent nodes (e.g. bookmarks bar). We only care
    // about their children.
    if (bookmark_model_->is_permanent_node(tracked_entity->bookmark_node())) {
      continue;
    }
    ProcessUpdate(*update, tracked_entity);
  }
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderUpdates(updates);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  std::unordered_map<base::StringPiece, const syncer::UpdateResponseData*,
                     base::StringPieceHash>
      id_to_updates;
  std::set<base::StringPiece> roots;
  std::unordered_map<base::StringPiece, std::vector<base::StringPiece>,
                     base::StringPieceHash>
      parent_to_children;

  // Add only non-deletions to |id_to_updates|.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = &update;
  }
  // Iterate over |id_to_updates| and construct |roots| and
  // |parent_to_children|.
  for (const std::pair<base::StringPiece, const syncer::UpdateResponseData*>&
           pair : id_to_updates) {
    const syncer::EntityData& update_entity = pair.second->entity.value();
    parent_to_children[update_entity.parent_id].push_back(update_entity.id);
    // If this entity's parent has no pending update, add it to |roots|.
    if (id_to_updates.count(update_entity.parent_id) == 0) {
      roots.insert(update_entity.parent_id);
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::StringPiece& root : roots) {
    TraverseAndAppendChildren(root, id_to_updates, parent_to_children,
                              &ordered_updates);
  }

  int root_node_updates_count = 0;
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      root_node_updates_count++;
      continue;
    }
    if (update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count);
  return ordered_updates;
}

void BookmarkRemoteUpdatesHandler::ProcessCreate(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK(!update_entity.is_deleted());
  // Because the Synced Bookmarks node can be created server side, it's possible
  // it'll arrive at the client as an update.
  if (update_entity.server_defined_unique_tag == kMobileBookmarksTag) {
    bookmark_tracker_->Add(update_entity.id, bookmark_model_->mobile_node(),
                           update.response_version, update_entity.creation_time,
                           update_entity.unique_position,
                           update_entity.specifics);
    return;
  }
  if (update_entity.parent_id == kBookmarksRootId) {
    DLOG(ERROR) << "Permanent nodes other than the Synced Bookmarks node "
                   "should have been merged during intial sync.";
    return;
  }

  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder));

  const bookmarks::BookmarkNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    DLOG(ERROR) << "Could not find parent of node being added."
                << " Node title: " << update_entity.specifics.bookmark().title()
                << ", parent id = " << update_entity.parent_id;
    return;
  }
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(
          update_entity.specifics.bookmark(), parent_node,
          ComputeChildNodeIndex(parent_node, update_entity.unique_position,
                                bookmark_tracker_),
          update_entity.is_folder, bookmark_model_, favicon_service_);
  if (!bookmark_node) {
    // We ignore bookmarks we can't add.
    DLOG(ERROR) << "Failed to create bookmark node with title "
                << update_entity.specifics.bookmark().title() << " and url "
                << update_entity.specifics.bookmark().url();
    return;
  }
  bookmark_tracker_->Add(update_entity.id, bookmark_node,
                         update.response_version, update_entity.creation_time,
                         update_entity.unique_position,
                         update_entity.specifics);
}

void BookmarkRemoteUpdatesHandler::ProcessUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity.value();
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());

  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder));
  DCHECK(!tracked_entity->IsUnsynced());

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();

  const SyncedBookmarkTracker::Entity* new_parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    bookmark_tracker_->Update(update_entity.id, update.response_version,
                              update_entity.modification_time,
                              update_entity.unique_position,
                              update_entity.specifics);
    return;
  }
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, bookmark_model_,
                    bookmark_tracker_, favicon_service_);
}

void BookmarkRemoteUpdatesHandler::ProcessDelete(
    const syncer::EntityData& update_entity,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  // Ignore changes to the permanent top-level nodes.  We only care about
  // their children.
  if (bookmark_model_->is_permanent_node(node)) {
    return;
  }
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  bookmark_model_->Remove(node);
}

void BookmarkRemoteUpdatesHandler::ProcessConflict(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity.value();
  // TODO(crbug.com/516866): Add basic unit test for this function.

  // Can only conflict with existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  if (tracked_entity->metadata()->is_deleted() && update_entity.is_deleted()) {
    // Both have been deleted, delete the corresponding entity from the tracker.
    bookmark_tracker_->Remove(update_entity.id);
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict",
                              syncer::ConflictResolution::CHANGES_MATCH,
                              syncer::ConflictResolution::TYPE_SIZE);
    return;
  }

  if (update_entity.is_deleted()) {
    // Only remote has been deleted. Local wins. Record that we received the
    // update from the server but leave the pending commit intact.
    bookmark_tracker_->UpdateServerVersion(update_entity.id,
                                           update.response_version);
    DLOG(WARNING) << "Conflict: USE_LOCAL";
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict",
                              syncer::ConflictResolution::USE_LOCAL,
                              syncer::ConflictResolution::TYPE_SIZE);
    return;
  }

  if (tracked_entity->metadata()->is_deleted()) {
    // Only local node has been deleted. It should be restored from the server
    // data as a remote creation.
    bookmark_tracker_->Remove(update_entity.id);
    ProcessCreate(update);
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict",
                              syncer::ConflictResolution::USE_REMOTE,
                              syncer::ConflictResolution::TYPE_SIZE);
    return;
  }

  // No deletions, there are potentially conflicting updates.
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();

  const SyncedBookmarkTracker::Entity* new_parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  // The |new_parent_entity| could be null in some racy conditions.  For
  // example, when a client A moves a node and deletes the old parent and
  // commits, and then updates the node again, and at the same time client B
  // updates before receiving the move updates. The client B update will arrive
  // at client A after the parent entity has been deleted already.
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  // |new_parent| would be null if the parent has been deleted locally and not
  // committed yet. Deletions are excuted recusively, so a parent deletions
  // entails child deletion, and if this child has been updated on another
  // client, this would cause conflict.
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    return;
  }
  // Either local and remote data match or server wins, and in both cases we
  // should squash any pending commits.
  bookmark_tracker_->AckSequenceNumber(update_entity.id);

  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    bookmark_tracker_->Update(update_entity.id, update.response_version,
                              update_entity.modification_time,
                              update_entity.unique_position,
                              update_entity.specifics);

    // The changes are identical so there isn't a real conflict.
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict",
                              syncer::ConflictResolution::CHANGES_MATCH,
                              syncer::ConflictResolution::TYPE_SIZE);
    return;
  }

  // Conflict where data don't match and no remote deletion, and hence server
  // wins. Update the model from server data.
  DLOG(WARNING) << "Conflict: USE_REMOTE";
  UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict",
                            syncer::ConflictResolution::USE_REMOTE,
                            syncer::ConflictResolution::TYPE_SIZE);
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, bookmark_model_,
                    bookmark_tracker_, favicon_service_);
}

void BookmarkRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const bookmarks::BookmarkNode* node) {
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);
  bookmark_tracker_->Remove(entity->metadata()->server_id());

  for (int i = 0; i < node->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = node->GetChild(i);
    RemoveEntityAndChildrenFromTracker(child);
  }
}

const bookmarks::BookmarkNode* BookmarkRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->bookmark_node();
}

}  // namespace sync_bookmarks
