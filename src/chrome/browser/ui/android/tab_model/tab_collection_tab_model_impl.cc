// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "tab_collection_tab_model_impl.h"
#include "ui/gfx/range/range.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from TabCollectionTabModelImpl.java.
#include "chrome/android/chrome_jni_headers/TabCollectionTabModelImpl_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::TokenAndroid;
using tab_groups::TabGroupColorId;
using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

namespace tabs {

namespace {

constexpr int kInvalidTabIndex = -1;

// Converts the `tab_android` to a `unique_ptr<TabInterface>`. Under the hood we
// use a wrapper class `TabInterfaceAndroid` which takes a weak ptr to
// `TabAndroid` to avoid memory management issues.
std::unique_ptr<TabInterface> ToTabInterface(TabAndroid* tab_android) {
  return std::make_unique<TabInterfaceAndroid>(tab_android);
}

// Converts the wrapper class TabInterfaceAndroid* to a TabAndroid*. This will
// crash if the `tab_interface` has outlived the TabAndroid*.
TabAndroid* ToTabAndroid(TabInterface* tab_interface) {
  auto weak_tab_android =
      static_cast<TabInterfaceAndroid*>(tab_interface)->GetWeakPtr();
  CHECK(weak_tab_android);
  return static_cast<TabAndroid*>(weak_tab_android.get());
}

// When moving a tab from a lower index to a higher index a value of 1 less
// should be used to account for the tab being removed from the list before it
// is re-inserted.
size_t ClampIfMovingToHigherIndex(const std::optional<size_t>& current_index,
                                  size_t new_index) {
  return (new_index != 0 && current_index && *current_index < new_index)
             ? new_index - 1
             : new_index;
}

}  // namespace

TabCollectionTabModelImpl::TabCollectionTabModelImpl(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& java_object,
    Profile* profile)
    : java_object_(env, java_object),
      profile_(profile),
      tab_strip_collection_(std::make_unique<TabStripCollection>()) {}

TabCollectionTabModelImpl::~TabCollectionTabModelImpl() = default;

void TabCollectionTabModelImpl::Destroy(JNIEnv* env) {
  delete this;
}

int TabCollectionTabModelImpl::GetTabCountRecursive(JNIEnv* env) const {
  return base::checked_cast<int>(tab_strip_collection_->TabCountRecursive());
}

int TabCollectionTabModelImpl::GetIndexOfTabRecursive(
    JNIEnv* env,
    TabAndroid* tab_android) const {
  if (!tab_android) {
    return kInvalidTabIndex;
  }

  int current_index = 0;
  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    if (ToTabAndroid(tab_in_collection) == tab_android) {
      return current_index;
    }
    current_index++;
  }
  return kInvalidTabIndex;
}

TabAndroid* TabCollectionTabModelImpl::GetTabAtIndexRecursive(
    JNIEnv* env,
    size_t index) const {
  if (index >= tab_strip_collection_->TabCountRecursive()) {
    return nullptr;
  }
  TabInterface* tab = tab_strip_collection_->GetTabAtIndexRecursive(index);
  return ToTabAndroid(tab);
}

int TabCollectionTabModelImpl::MoveTabRecursive(
    JNIEnv* env,
    size_t current_index,
    size_t new_index,
    const std::optional<base::Token>& token,
    bool new_is_pinned) {
  std::optional<TabGroupId> new_tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);
  new_index = GetSafeIndex(/*is_tab_group=*/false, current_index, new_index,
                           new_tab_group_id, new_is_pinned);

  tab_strip_collection_->MoveTabRecursive(current_index, new_index,
                                          new_tab_group_id, new_is_pinned);
  return base::checked_cast<int>(new_index);
}

void TabCollectionTabModelImpl::AddTabRecursive(
    JNIEnv* env,
    TabAndroid* tab_android,
    size_t index,
    const std::optional<base::Token>& token,
    bool is_pinned) {
  CHECK(tab_android);

  std::optional<TabGroupId> tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);

  index = GetSafeIndex(/*is_tab_group=*/false, /*current_index=*/std::nullopt,
                       index, tab_group_id, is_pinned);

  auto tab_interface_android = ToTabInterface(tab_android);
  tab_strip_collection_->AddTabRecursive(std::move(tab_interface_android),
                                         index, tab_group_id, is_pinned);
}

void TabCollectionTabModelImpl::RemoveTabRecursive(JNIEnv* env,
                                                   TabAndroid* tab) {
  int index = GetIndexOfTabRecursive(env, tab);
  CHECK_NE(index, kInvalidTabIndex);
  tab_strip_collection_->RemoveTabAtIndexRecursive(index);
}

void TabCollectionTabModelImpl::CreateTabGroup(
    JNIEnv* env,
    const base::Token& tab_group_id,
    const std::u16string& tab_group_title,
    jint j_color_id,
    bool is_collapsed) {
  TabGroupAndroid::Factory factory(profile_);
  std::unique_ptr<TabGroupTabCollection> group_collection =
      std::make_unique<TabGroupTabCollection>(
          factory, TabGroupId::FromRawToken(tab_group_id),
          TabGroupVisualData(tab_group_title,
                             static_cast<TabGroupColorId>(j_color_id),
                             is_collapsed));
  tab_strip_collection_->CreateTabGroup(std::move(group_collection));
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetTabsInGroup(
    JNIEnv* env,
    const base::Token& token) {
  std::optional<TabGroupId> tab_group_id =
      tab_groups::TabGroupId::FromRawToken(token);
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(*tab_group_id);

  std::vector<TabAndroid*> tabs;
  if (!group_collection) {
    return tabs;
  }

  tabs.reserve(group_collection->TabCountRecursive());
  for (TabInterface* group_tab : *group_collection) {
    tabs.push_back(ToTabAndroid(group_tab));
  }
  return tabs;
}

int TabCollectionTabModelImpl::MoveTabGroupTo(JNIEnv* env,
                                              const base::Token& token,
                                              int to_index) {
  TabGroupId tab_group_id = TabGroupId::FromRawToken(token);
  TabGroup* group = GetTabGroupChecked(tab_group_id);
  gfx::Range range = group->ListTabs();
  to_index =
      GetSafeIndex(/*is_tab_group=*/true, range.start(), to_index, tab_group_id,
                   /*is_pinned=*/false);
  // When moving to a higher index the implementation will first remove the tab
  // group before adding the tab group. This means the destination index needs
  // to account for the size of the group. To do this we subtract the number of
  // tabs in the group from the `to_index`. Note that GetSafeIndex() already
  // subtracts one when moving to a higher index so we subtract 1 less.
  if (to_index >= base::checked_cast<int>(range.end())) {
    to_index -= range.length() - 1;
    CHECK_GE(to_index, 0);
  }
  tab_strip_collection_->MoveTabGroupTo(tab_group_id, to_index);
  return base::checked_cast<int>(to_index);
}

void TabCollectionTabModelImpl::UpdateTabGroupVisualData(
    JNIEnv* env,
    const base::Token& tab_group_id,
    const std::optional<std::u16string>& tab_group_title,
    const std::optional<jint>& j_color_id,
    const std::optional<bool>& is_collapsed) {
  TabGroup* group = GetTabGroupChecked(TabGroupId::FromRawToken(tab_group_id));
  const TabGroupVisualData* old_visual_data = group->visual_data();
  CHECK(old_visual_data);

  TabGroupVisualData new_visual_data(
      tab_group_title.value_or(old_visual_data->title()),
      j_color_id.has_value() ? static_cast<TabGroupColorId>(j_color_id.value())
                             : old_visual_data->color(),
      is_collapsed.value_or(old_visual_data->is_collapsed()));
  group->SetVisualData(new_visual_data);
}

std::u16string TabCollectionTabModelImpl::GetTabGroupTitle(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return visual_data->title();
}

jint TabCollectionTabModelImpl::GetTabGroupColor(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return static_cast<jint>(visual_data->color());
}

bool TabCollectionTabModelImpl::GetTabGroupCollapsed(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return visual_data->is_collapsed();
}

bool TabCollectionTabModelImpl::DetachedTabGroupExists(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  TabGroupId group_id = TabGroupId::FromRawToken(tab_group_id);
  TabGroupTabCollection* detached_group =
      tab_strip_collection_->GetDetachedTabGroup(group_id);
  return detached_group != nullptr;
}

void TabCollectionTabModelImpl::CloseDetachedTabGroup(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  TabGroupId group_id = TabGroupId::FromRawToken(tab_group_id);
  TabGroupTabCollection* detached_group =
      tab_strip_collection_->GetDetachedTabGroup(group_id);
  if (!detached_group) {
    CHECK(!tab_strip_collection_->GetTabGroupCollection(group_id))
        << "Tried to close an attached tab group.";
    LOG(WARNING) << "Detached tab group already closed.";
    return;
  }
  tab_strip_collection_->CloseDetachedTabGroup(group_id);
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetAllTabs(JNIEnv* env) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tab_strip_collection_->TabCountRecursive());

  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    tabs.push_back(ToTabAndroid(tab_in_collection));
  }
  return tabs;
}

std::vector<base::Token> TabCollectionTabModelImpl::GetAllTabGroupIds(
    JNIEnv* env) {
  std::vector<TabGroupId> group_ids =
      tab_strip_collection_->GetAllTabGroupIds();

  std::vector<base::Token> tokens;
  tokens.reserve(group_ids.size());
  for (const TabGroupId& group_id : group_ids) {
    tokens.push_back(group_id.token());
  }
  return tokens;
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetRepresentativeTabList(
    JNIEnv* env) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tab_strip_collection_->pinned_collection()->ChildCount() +
               tab_strip_collection_->unpinned_collection()->ChildCount());

  std::optional<TabGroupId> current_group_id = std::nullopt;
  for (TabInterface* tab : *tab_strip_collection_) {
    std::optional<TabGroupId> tab_group_id = tab->GetGroup();
    if (!tab_group_id) {
      current_group_id = std::nullopt;
      tabs.push_back(ToTabAndroid(tab));
    } else if (current_group_id != tab_group_id) {
      current_group_id = tab_group_id;
      TabGroupAndroid* group =
          static_cast<TabGroupAndroid*>(GetTabGroupChecked(*tab_group_id));

      std::optional<TabHandle> last_shown_tab = group->last_shown_tab();
      // By the time a tab group is used in GetRepresentativeTabList it should
      // have a valid `last_shown_tab`. The only time this should be empty is
      // either while the tab group is detached or during the synchronous
      // process of attaching the group. During neither of these times is this
      // method expected to be called.
      CHECK(last_shown_tab);
      TabAndroid* tab_android = TabAndroid::FromTabHandle(*last_shown_tab);
      CHECK(tab_android);
      tabs.push_back(tab_android);
    }
  }
  return tabs;
}

void TabCollectionTabModelImpl::SetLastShownTabForGroup(
    JNIEnv* env,
    const base::Token& group_id,
    TabAndroid* tab_android) {
  TabGroupAndroid* group = static_cast<TabGroupAndroid*>(GetTabGroupChecked(
      TabGroupId::FromRawToken(group_id), /*allow_detached=*/true));
  if (tab_android) {
    group->set_last_shown_tab(tab_android->GetHandle());
  } else {
    group->set_last_shown_tab(std::nullopt);
  }
}

TabAndroid* TabCollectionTabModelImpl::GetLastShownTabForGroup(
    JNIEnv* env,
    const base::Token& group_id) {
  TabGroupAndroid* group = static_cast<TabGroupAndroid*>(
      GetTabGroupChecked(TabGroupId::FromRawToken(group_id),
                         /*allow_detached=*/true));
  auto handle = group->last_shown_tab();
  if (!handle) {
    return nullptr;
  }
  return TabAndroid::FromTabHandle(*handle);
}

// Private methods:

size_t TabCollectionTabModelImpl::GetSafeIndex(
    bool is_tab_group,
    const std::optional<size_t>& current_index,
    size_t proposed_index,
    const std::optional<TabGroupId>& tab_group_id,
    bool is_pinned) const {
  size_t first_non_pinned_index = ClampIfMovingToHigherIndex(
      current_index, tab_strip_collection_->IndexOfFirstNonPinnedTab());
  if (is_pinned) {
    return std::min(proposed_index, first_non_pinned_index);
  }

  size_t total_tabs = ClampIfMovingToHigherIndex(
      current_index, tab_strip_collection_->TabCountRecursive());
  size_t clamped_index =
      std::clamp(proposed_index, first_non_pinned_index, total_tabs);
  if (!is_tab_group && tab_group_id) {
    TabGroupTabCollection* group_collection =
        tab_strip_collection_->GetTabGroupCollection(*tab_group_id);
    if (group_collection) {
      gfx::Range range = group_collection->GetTabGroup()->ListTabs();
      if (!range.is_empty()) {
        return std::clamp(
            proposed_index,
            ClampIfMovingToHigherIndex(current_index, range.start()),
            ClampIfMovingToHigherIndex(current_index, range.end()));
      }
    }
  }

  // Always safe since these are the edges.
  if (clamped_index == first_non_pinned_index || clamped_index == total_tabs) {
    return clamped_index;
  }

  std::optional<TabGroupId> group_at_index = GetGroupIdAt(clamped_index);
  if (group_at_index) {
    // Insertion will happen inside a tab group we need to push it out.
    TabGroupTabCollection* group_collection =
        tab_strip_collection_->GetTabGroupCollection(*group_at_index);
    gfx::Range range = group_collection->GetTabGroup()->ListTabs();

    // When moving a tab group to be within its own range this should no-op.
    if (is_tab_group && group_at_index == tab_group_id) {
      return range.start();
    }

    // Push to the nearest boundary.
    if (clamped_index - range.start() < range.end() - clamped_index) {
      return ClampIfMovingToHigherIndex(current_index, range.start());
    } else {
      return ClampIfMovingToHigherIndex(current_index, range.end());
    }
  }

  return clamped_index;
}

std::optional<TabGroupId> TabCollectionTabModelImpl::GetGroupIdAt(
    size_t index) const {
  if (index < tab_strip_collection_->TabCountRecursive()) {
    return tab_strip_collection_->GetTabAtIndexRecursive(index)->GetGroup();
  } else {
    return std::nullopt;
  }
}

TabGroupTabCollection* TabCollectionTabModelImpl::GetTabGroupCollectionChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(tab_group_id);
  if (!group_collection && allow_detached) {
    group_collection = tab_strip_collection_->GetDetachedTabGroup(tab_group_id);
  }
  CHECK(group_collection);
  return group_collection;
}

TabGroup* TabCollectionTabModelImpl::GetTabGroupChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroupTabCollection* group_collection =
      GetTabGroupCollectionChecked(tab_group_id, allow_detached);
  TabGroup* group = group_collection->GetTabGroup();
  CHECK(group);
  return group;
}

const TabGroupVisualData*
TabCollectionTabModelImpl::GetTabGroupVisualDataChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroup* group = GetTabGroupChecked(tab_group_id, allow_detached);
  const TabGroupVisualData* visual_data = group->visual_data();
  CHECK(visual_data);
  return visual_data;
}

static jlong JNI_TabCollectionTabModelImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_java_object,
    Profile* profile) {
  TabCollectionTabModelImpl* tab_collection_tab_model_impl =
      new TabCollectionTabModelImpl(env, j_java_object, profile);
  return reinterpret_cast<intptr_t>(tab_collection_tab_model_impl);
}

}  // namespace tabs
