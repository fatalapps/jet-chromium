// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_executor.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/renderer/actor/click_tool.h"
#include "chrome/renderer/actor/drag_and_release_tool.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/mouse_move_tool.h"
#include "chrome/renderer/actor/script_tool.h"
#include "chrome/renderer/actor/scroll_tool.h"
#include "chrome/renderer/actor/select_tool.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "chrome/renderer/actor/type_tool.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

using blink::WebLocalFrame;
using content::RenderFrame;

namespace actor {

ToolExecutor::ToolExecutor(RenderFrame* frame, Journal& journal)
    : frame_(*frame), journal_(journal) {}

ToolExecutor::~ToolExecutor() {
  if (completion_callback_) {
    std::move(completion_callback_)
        .Run(MakeResult(mojom::ActionResultCode::kExecutorDestroyed,
                        "The tool executor was destroyed before invocation "
                        "could complete."));
  }
}

void ToolExecutor::InvokeTool(mojom::ToolInvocationPtr invocation,
                              ToolExecutorCallback callback) {
  if (tool_) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kExecutorBusy,
                   "Another tool invocation is still running."));
    return;
  }

  CHECK(!completion_callback_);
  completion_callback_ = std::move(callback);
  invoke_journal_entry_ =
      journal_->CreatePendingAsyncEntry(invocation->task_id, "InvokeTool", "");

  WebLocalFrame* web_frame = frame_->GetWebFrame();

  // Tool calls should only be routed to local root frames.
  CHECK(!web_frame || web_frame->LocalRoot() == web_frame);

  // Check LocalRoot in case the frame is a subframe.
  if (!web_frame || !web_frame->FrameWidget()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ToolExecutor::PageStabilized,
                       weak_ptr_factory_.GetWeakPtr(),
                       MakeResult(mojom::ActionResultCode::kFrameWentAway)));
    return;
  }

  switch (invocation->action->which()) {
    case actor::mojom::ToolAction::Tag::kClick: {
      tool_ = std::make_unique<ClickTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_click()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kMouseMove: {
      tool_ = std::make_unique<MouseMoveTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_mouse_move()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kType: {
      tool_ = std::make_unique<TypeTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_type()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScroll: {
      tool_ = std::make_unique<ScrollTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_scroll()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kSelect: {
      tool_ = std::make_unique<SelectTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_select()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kDragAndRelease: {
      tool_ = std::make_unique<DragAndReleaseTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->action->get_drag_and_release()),
          std::move(invocation->target),
          std::move(invocation->observed_target));
      break;
    }
    case actor::mojom::ToolAction::Tag::kScriptTool: {
      // We could consider not waiting for stabilization since the API has an
      // explicit async hook to know when the tool is done. Or having the
      // stabilization only delay until a new frame is produced.
      tool_ = std::make_unique<ScriptTool>(
          frame_.get(), invocation->task_id, journal_.get(),
          std::move(invocation->target), std::move(invocation->observed_target),
          std::move(invocation->action->get_script_tool()));
      break;
    }
    default:
      NOTREACHED();
  }

  page_stability_monitor_ = std::make_unique<PageStabilityMonitor>(*frame_);

  execute_journal_entry_ = journal_->CreatePendingAsyncEntry(
      invocation->task_id, "ExecuteTool", tool_->DebugString());
  tool_->Execute(base::BindOnce(&ToolExecutor::ToolFinished,
                                weak_ptr_factory_.GetWeakPtr(),
                                invocation->task_id));
}

void ToolExecutor::ToolFinished(int32_t task_id,
                                mojom::ActionResultPtr result) {
  execute_journal_entry_.reset();
  page_stability_monitor_->WaitForStable(
      *tool_, task_id, *journal_,
      base::BindOnce(&ToolExecutor::PageStabilized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result)));
}

void ToolExecutor::PageStabilized(mojom::ActionResultPtr result) {
  CHECK(completion_callback_);
  page_stability_monitor_.reset();

  CHECK(tool_);
  // Release current tool so we can accept a new tool invocation.
  tool_.reset();

  invoke_journal_entry_.reset();
  std::move(completion_callback_).Run(std::move(result));
}

}  // namespace actor
