// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "components/tabs/public/tab_interface.h"
#include "string.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tab_groups {
class CollaborationMessagingTabData;
}  // namespace tab_groups

class CollaborationMessagingPageActionController {
 public:
  CollaborationMessagingPageActionController(
      page_actions::PageActionController& page_action_controller,
      tab_groups::CollaborationMessagingTabData&
          collaboration_messaging_tab_data);

  ~CollaborationMessagingPageActionController();

  CollaborationMessagingPageActionController(
      const CollaborationMessagingPageActionController&) = delete;

  CollaborationMessagingPageActionController& operator=(
      const CollaborationMessagingPageActionController&) = delete;

  // TODO(crbug.com/430536113): Move this to private.
  // Handle update callback from TabInterface. Need this to
  // be public for passing unit tests. Will be moved to private once receiving
  // callback from TabInterface.
  void HandleUpdate(tabs::TabInterface* tab);

 private:
  // Show the page action with label and avatar from collaboration message.
  void Show(const std::u16string& label_text, const ui::ImageModel& avatar);

  // Hide the page action.
  void Hide();

  const raw_ref<page_actions::PageActionController> page_actions_controller_;
  const raw_ref<tab_groups::CollaborationMessagingTabData>
      collaboration_messaging_tab_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_PAGE_ACTION_CONTROLLER_H_
