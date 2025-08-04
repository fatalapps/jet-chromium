// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

// Executes a script tool in the renderer.
class ScriptToolRequest : public PageToolRequest {
 public:
  ScriptToolRequest(tabs::TabHandle tab_handle,
                    const PageTarget& target,
                    const std::string& name,
                    const std::string& input_arguments);
  ~ScriptToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;
  void Apply(ToolRequestVisitorFunctor&) const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  std::string name_;
  std::string input_arguments_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_REQUEST_H_
