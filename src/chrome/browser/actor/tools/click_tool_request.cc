// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ClickToolRequest::ClickToolRequest(TabHandle tab_handle,
                                   const PageTarget& target,
                                   MouseClickType type,
                                   MouseClickCount count)
    : PageToolRequest(tab_handle, target),
      click_type_(type),
      click_count_(count) {}

ClickToolRequest::~ClickToolRequest() = default;

void ClickToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string ClickToolRequest::JournalEvent() const {
  return "Click";
}

mojom::ToolActionPtr ClickToolRequest::ToMojoToolAction() const {
  auto click = mojom::ClickAction::New();
  click->type = click_type_;
  click->count = click_count_;
  return mojom::ToolAction::NewClick(std::move(click));
}

std::unique_ptr<PageToolRequest> ClickToolRequest::Clone() const {
  return std::make_unique<ClickToolRequest>(*this);
}

}  // namespace actor
