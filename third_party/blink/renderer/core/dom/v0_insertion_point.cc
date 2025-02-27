/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

using namespace HTMLNames;

V0InsertionPoint::V0InsertionPoint(const QualifiedName& tag_name,
                                   Document& document)
    : HTMLElement(tag_name, document, kCreateV0InsertionPoint),
      registered_with_shadow_root_(false) {
  SetHasCustomStyleCallbacks();
}

V0InsertionPoint::~V0InsertionPoint() = default;

void V0InsertionPoint::SetDistributedNodes(
    DistributedNodes& distributed_nodes) {
  // Attempt not to reattach nodes that would be distributed to the exact same
  // location by comparing the old and new distributions.

  wtf_size_t i = 0;
  wtf_size_t j = 0;

  for (; i < distributed_nodes_.size() && j < distributed_nodes.size();
       ++i, ++j) {
    if (distributed_nodes_.size() < distributed_nodes.size()) {
      // If the new distribution is larger than the old one, reattach all nodes
      // in the new distribution that were inserted.
      for (; j < distributed_nodes.size() &&
             distributed_nodes_.at(i) != distributed_nodes.at(j);
           ++j)
        distributed_nodes.at(j)->LazyReattachIfAttached();
      if (j == distributed_nodes.size())
        break;
    } else if (distributed_nodes_.size() > distributed_nodes.size()) {
      // If the old distribution is larger than the new one, reattach all nodes
      // in the old distribution that were removed.
      for (; i < distributed_nodes_.size() &&
             distributed_nodes_.at(i) != distributed_nodes.at(j);
           ++i)
        distributed_nodes_.at(i)->LazyReattachIfAttached();
      if (i == distributed_nodes_.size())
        break;
    } else if (distributed_nodes_.at(i) != distributed_nodes.at(j)) {
      // If both distributions are the same length reattach both old and new.
      distributed_nodes_.at(i)->LazyReattachIfAttached();
      distributed_nodes.at(j)->LazyReattachIfAttached();
    }
  }

  // If we hit the end of either list above we need to reattach all remaining
  // nodes.

  for (; i < distributed_nodes_.size(); ++i)
    distributed_nodes_.at(i)->LazyReattachIfAttached();

  for (; j < distributed_nodes.size(); ++j)
    distributed_nodes.at(j)->LazyReattachIfAttached();

  distributed_nodes_.Swap(distributed_nodes);
  // Deallocate a Vector and a HashMap explicitly so that
  // Oilpan can recycle them without an intervening GC.
  distributed_nodes.Clear();
  distributed_nodes_.ShrinkToFit();
}

void V0InsertionPoint::AttachLayoutTree(AttachContext& context) {
  // We need to attach the distribution here so that they're inserted in the
  // right order otherwise the n^2 protection inside LayoutTreeBuilder will
  // cause them to be inserted in the wrong place later. This also lets
  // distributed nodes benefit from the n^2 protection.
  AttachContext children_context(context);

  for (wtf_size_t i = 0; i < distributed_nodes_.size(); ++i) {
    Node* child = distributed_nodes_.at(i);
    if (child->NeedsAttach())
      child->AttachLayoutTree(children_context);
  }
  if (children_context.previous_in_flow)
    context.previous_in_flow = children_context.previous_in_flow;

  HTMLElement::AttachLayoutTree(context);
}

void V0InsertionPoint::DetachLayoutTree(const AttachContext& context) {
  for (wtf_size_t i = 0; i < distributed_nodes_.size(); ++i)
    distributed_nodes_.at(i)->LazyReattachIfAttached();

  HTMLElement::DetachLayoutTree(context);
}

void V0InsertionPoint::RebuildDistributedChildrenLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (wtf_size_t i = distributed_nodes_.size(); i > 0; --i) {
    RebuildLayoutTreeForChild(distributed_nodes_.at(i - 1),
                              whitespace_attacher);
  }
}

void V0InsertionPoint::DidRecalcStyle(StyleRecalcChange change) {
  if (!HasDistribution() || DistributedNodeAt(0)->parentNode() == this) {
    // We either do not have distributed children or the distributed children
    // are the fallback children. Fallback children have already been
    // recalculated in ContainerNode::RecalcDescendantStyles().
    return;
  }

  StyleChangeType style_change_type =
      change == kForce ? kSubtreeStyleChange : kLocalStyleChange;

  for (wtf_size_t i = 0; i < distributed_nodes_.size(); ++i) {
    Node* node = distributed_nodes_.at(i);
    if (change == kReattach && node->IsElementNode()) {
      if (node->ShouldCallRecalcStyle(kReattach))
        ToElement(node)->RecalcStyle(kReattach);
      continue;
    }
    node->SetNeedsStyleRecalc(
        style_change_type,
        StyleChangeReasonForTracing::Create(
            StyleChangeReason::kPropagateInheritChangeToDistributedNodes));
  }
}

bool V0InsertionPoint::CanBeActive() const {
  ShadowRoot* shadow_root = ContainingShadowRoot();
  if (!shadow_root)
    return false;
  if (shadow_root->IsV1())
    return false;
  return !Traversal<V0InsertionPoint>::FirstAncestor(*this);
}

bool V0InsertionPoint::IsActive() const {
  if (!CanBeActive())
    return false;
  ShadowRoot* shadow_root = ContainingShadowRoot();
  DCHECK(shadow_root);
  if (!IsHTMLShadowElement(*this) ||
      shadow_root->V0().DescendantShadowElementCount() <= 1)
    return true;

  // Slow path only when there are more than one shadow elements in a shadow
  // tree. That should be a rare case.
  for (const auto& point : shadow_root->V0().DescendantInsertionPoints()) {
    if (IsHTMLShadowElement(*point))
      return point == this;
  }
  return true;
}

bool V0InsertionPoint::IsContentInsertionPoint() const {
  return IsHTMLContentElement(*this) && IsActive();
}

StaticNodeList* V0InsertionPoint::getDistributedNodes() {
  UpdateDistributionForLegacyDistributedNodes();

  HeapVector<Member<Node>> nodes;
  nodes.ReserveInitialCapacity(distributed_nodes_.size());
  for (wtf_size_t i = 0; i < distributed_nodes_.size(); ++i)
    nodes.UncheckedAppend(distributed_nodes_.at(i));

  return StaticNodeList::Adopt(nodes);
}

bool V0InsertionPoint::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return !IsActive() && HTMLElement::LayoutObjectIsNeeded(style);
}

void V0InsertionPoint::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (!(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled() &&
          root->IsV1()))
      root->SetNeedsDistributionRecalc();
  }
}

Node::InsertionNotificationRequest V0InsertionPoint::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (!root->IsV1()) {
      if (!(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled() &&
            root->IsV1()))
        root->SetNeedsDistributionRecalc();
      if (CanBeActive() && !registered_with_shadow_root_ &&
          insertion_point.GetTreeScope().RootNode() == root) {
        registered_with_shadow_root_ = true;
        root->V0().DidAddInsertionPoint(this);
        if (CanAffectSelector())
          root->V0().WillAffectSelector();
      }
    }
  }

  // We could have been distributed into in a detached subtree, make sure to
  // clear the distribution when inserted again to avoid cycles.
  ClearDistribution();

  return kInsertionDone;
}

void V0InsertionPoint::RemovedFrom(ContainerNode& insertion_point) {
  ShadowRoot* root = ContainingShadowRoot();
  if (!root)
    root = insertion_point.ContainingShadowRoot();

  if (root &&
      !(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled() && root->IsV1()))
    root->SetNeedsDistributionRecalc();

  // Since this insertion point is no longer visible from the shadow subtree, it
  // need to clean itself up.
  ClearDistribution();

  if (registered_with_shadow_root_ &&
      insertion_point.GetTreeScope().RootNode() == root) {
    DCHECK(root);
    registered_with_shadow_root_ = false;
    root->V0().DidRemoveInsertionPoint(this);
    if (!root->IsV1() && CanAffectSelector())
      root->V0().WillAffectSelector();
  }

  HTMLElement::RemovedFrom(insertion_point);
}

void V0InsertionPoint::Trace(blink::Visitor* visitor) {
  visitor->Trace(distributed_nodes_);
  HTMLElement::Trace(visitor);
}

const V0InsertionPoint* ResolveReprojection(const Node* projected_node) {
  DCHECK(projected_node);
  const V0InsertionPoint* insertion_point = nullptr;
  const Node* current = projected_node;
  ShadowRoot* last_shadow_root = nullptr;
  while (true) {
    ShadowRoot* shadow_root =
        ShadowRootWhereNodeCanBeDistributedForV0(*current);
    if (!shadow_root || shadow_root->IsV1() || shadow_root == last_shadow_root)
      break;
    last_shadow_root = shadow_root;
    const V0InsertionPoint* inserted_to =
        shadow_root->V0().FinalDestinationInsertionPointFor(projected_node);
    if (!inserted_to)
      break;
    DCHECK_NE(current, inserted_to);
    current = inserted_to;
    insertion_point = inserted_to;
  }
  return insertion_point;
}

void CollectDestinationInsertionPoints(
    const Node& node,
    HeapVector<Member<V0InsertionPoint>, 8>& results) {
  const Node* current = &node;
  ShadowRoot* last_shadow_root = nullptr;
  while (true) {
    ShadowRoot* shadow_root =
        ShadowRootWhereNodeCanBeDistributedForV0(*current);
    if (!shadow_root || shadow_root->IsV1() || shadow_root == last_shadow_root)
      return;
    last_shadow_root = shadow_root;
    const DestinationInsertionPoints* insertion_points =
        shadow_root->V0().DestinationInsertionPointsFor(&node);
    if (!insertion_points)
      return;
    for (wtf_size_t i = 0; i < insertion_points->size(); ++i)
      results.push_back(insertion_points->at(i).Get());
    DCHECK_NE(current, insertion_points->back().Get());
    current = insertion_points->back().Get();
  }
}

}  // namespace blink
