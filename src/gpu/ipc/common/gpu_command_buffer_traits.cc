// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_command_buffer_traits.h"

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/buffer_format_util.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<gpu::SyncToken>::Write(base::Pickle* m, const param_type& p) {
  DCHECK(!p.HasData() || p.verified_flush());

  WriteParam(m, p.verified_flush());
  WriteParam(m, p.namespace_id());
  WriteParam(m, p.command_buffer_id());
  WriteParam(m, p.release_count());
}

bool ParamTraits<gpu::SyncToken>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  bool verified_flush = false;
  gpu::CommandBufferNamespace namespace_id =
      gpu::CommandBufferNamespace::INVALID;
  gpu::CommandBufferId command_buffer_id;
  uint64_t release_count = 0;

  if (!ReadParam(m, iter, &verified_flush) ||
      !ReadParam(m, iter, &namespace_id) ||
      !ReadParam(m, iter, &command_buffer_id) ||
      !ReadParam(m, iter, &release_count)) {
    return false;
  }

  p->Set(namespace_id, command_buffer_id, release_count);
  if (p->HasData()) {
    if (!verified_flush)
      return false;
    p->SetVerifyFlush();
  }

  return true;
}

void ParamTraits<gpu::SyncToken>::Log(const param_type& p, std::string* l) {
  *l += base::StringPrintf(
      "[%" PRId8 ":%" PRIX64 "] %" PRIu64, p.namespace_id(),
      p.command_buffer_id().GetUnsafeValue(), p.release_count());
}

void ParamTraits<gpu::Mailbox>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(p.name, sizeof(p.name));
}

bool ParamTraits<gpu::Mailbox>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* p) {
  const char* bytes = nullptr;
  if (!iter->ReadBytes(&bytes, sizeof(p->name)))
    return false;
  DCHECK(bytes);
  UNSAFE_TODO(memcpy(p->name, bytes, sizeof(p->name)));
  return true;
}

void ParamTraits<gpu::Mailbox>::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < sizeof(p.name); ++i)
    *l += base::StringPrintf("%02x", UNSAFE_TODO(p.name[i]));
}

void ParamTraits<gpu::MailboxHolder>::Write(base::Pickle* m,
                                            const param_type& p) {
  WriteParam(m, p.mailbox);
  WriteParam(m, p.sync_token);
  WriteParam(m, p.texture_target);
}

bool ParamTraits<gpu::MailboxHolder>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* p) {
  if (!ReadParam(m, iter, &p->mailbox) || !ReadParam(m, iter, &p->sync_token) ||
      !ReadParam(m, iter, &p->texture_target))
    return false;
  return true;
}

void ParamTraits<gpu::MailboxHolder>::Log(const param_type& p, std::string* l) {
  LogParam(p.mailbox, l);
  LogParam(p.sync_token, l);
  *l += base::StringPrintf(":%04x@", p.texture_target);
}

void ParamTraits<gpu::GpuMemoryBufferFormatSet>::Write(base::Pickle* m,
                                                       const param_type& p) {
  WriteParam(m, p.ToEnumBitmask());
}

bool ParamTraits<gpu::GpuMemoryBufferFormatSet>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  uint64_t bitmask = 0;
  if (!ReadParam(m, iter, &bitmask)) {
    return false;
  }
  // Check deserialized bitmask contains only bits GpuMemoryBufferFormatSet
  // expects to be set based on largest enum it expects.
  if (bitmask & ~gpu::GpuMemoryBufferFormatSet::All().ToEnumBitmask()) {
    return false;
  }
  *p = gpu::GpuMemoryBufferFormatSet::FromEnumBitmask(bitmask);
  return true;
}

void ParamTraits<gpu::GpuMemoryBufferFormatSet>::Log(const param_type& p,
                                                     std::string* l) {
  std::string str;
  for (gfx::BufferFormat format : p) {
    if (!str.empty()) {
      str += "|";
    }
    str += gfx::BufferFormatToString(format);
  }
  *l += str;
}

}  // namespace IPC
