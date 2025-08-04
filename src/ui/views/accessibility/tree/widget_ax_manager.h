// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source.h"
#include "ui/views/views_export.h"

namespace ui {
class BrowserAccessibilityManager;
}  // namespace ui

namespace views {

class ViewAccessibility;
class Widget;

using ViewAccessibilityAXTreeSerializer = ui::AXTreeSerializer<
    ViewAccessibility*,
    std::vector<raw_ptr<ViewAccessibility, VectorExperimental>>,
    ui::AXTreeUpdate*,
    ui::AXTreeData*,
    ui::AXNodeData>;

// This class owns and manages the accessibility tree for a Widget. It is owned
// by the `widget_` and must never outlive its owner. This is currently under
// construction.
class VIEWS_EXPORT WidgetAXManager : public ui::AXModeObserver,
                                     public ui::AXNodeIdDelegate,
                                     public ui::AXPlatformTreeManagerDelegate {
 public:
  explicit WidgetAXManager(Widget* widget);
  WidgetAXManager(const WidgetAXManager&) = delete;
  WidgetAXManager& operator=(const WidgetAXManager&) = delete;
  WidgetAXManager(WidgetAXManager&&) = delete;
  WidgetAXManager& operator=(WidgetAXManager&&) = delete;
  ~WidgetAXManager() override;

  void Enable();

  bool is_enabled() const { return is_enabled_; }

  void OnEvent(ViewAccessibility& view_ax, ax::mojom::Event event_type);
  void OnDataChanged(ViewAccessibility& view_ax);

  void OnChildAdded(WidgetAXManager* child_manager);
  void OnChildRemoved(WidgetAXManager* child_manager);

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

  // ui::AXNodeIdDelegate:
  ui::AXPlatformNodeId GetOrCreateAXNodeUniqueId(
      ui::AXNodeID ax_node_id) override;
  void OnAXNodeDeleted(ui::AXNodeID ax_node_id) override;

  // ui::AXPlatformTreeManagerDelegate:
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void UnrecoverableAccessibilityError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_view_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback) override;
  gfx::NativeWindow GetTopLevelNativeWindow() override;
  bool CanFireAccessibilityEvents() const override;
  bool AccessibilityIsRootFrame() const override;
  bool ShouldSuppressAXLoadComplete() override;
  content::WebContentsAccessibility* AccessibilityGetWebContentsAccessibility()
      override;
  bool AccessibilityIsWebContentSource() override;

 private:
  friend class WidgetAXManagerTestApi;

  void SchedulePendingUpdate();
  void SendPendingUpdate();

  // The widget this manager is owned by.
  raw_ptr<Widget> widget_;

  // The AXTreeID for this widget's accessibility tree. Must be unique.
  const ui::AXTreeID ax_tree_id_;

  // The AXTreeID of the parent widget's accessibility tree, if any.
  ui::AXTreeID parent_ax_tree_id_;

  std::unique_ptr<WidgetViewAXCache> cache_;

  // Holds the active views-based tree. A tree consists of all the views in the
  // widget.
  std::unique_ptr<ViewAccessibilityAXTreeSource> tree_source_;

  // Serializes incremental updates on the currently active `tree_source_`.
  std::unique_ptr<ViewAccessibilityAXTreeSerializer> tree_serializer_;

  // Holds the generated AXTree of AXNodes for the views-based tree.
  std::unique_ptr<ui::BrowserAccessibilityManager> ax_tree_manager_;

  // Indicates whether we're actively serializing widget accessibility data.
  bool is_enabled_ = false;

  // Indicates whether we have already posted an event or data changed task to
  // SendPendingUpdate().
  bool processing_update_posted_ = false;

  struct Event {
    int id;
    ax::mojom::Event event_type;
    // TODO(accessibility): Implement action request tracking.
  };
  std::vector<Event> pending_events_;
  absl::flat_hash_set<ui::AXNodeID> pending_data_updates_;

  // Ensure posted tasks don’t run after we’re destroyed.
  base::WeakPtrFactory<WidgetAXManager> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
