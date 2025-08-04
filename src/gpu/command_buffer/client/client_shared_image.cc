// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include <optional>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/dawn/include/dawn/wire/client/webgpu_cpp.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"

#if BUILDFLAG(IS_APPLE)
#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#endif

namespace gpu {

namespace {

class ScopedMappingForTests : public ClientSharedImage::ScopedMapping {
 public:
  ScopedMappingForTests(const gfx::Size& size, gfx::BufferFormat format)
      : size_(size), format_(format) {
    int num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format_);
    size_t allocation_size = 0;
    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      size_t height_in_pixels;
      CHECK(gfx::PlaneHeightForBufferFormatChecked(
          Size().height(), format_, plane_index, &height_in_pixels));
      allocation_size += Stride(plane_index) * height_in_pixels;
    }

    data_ = std::vector<uint8_t>(allocation_size);
  }

  ~ScopedMappingForTests() override = default;

  // ClientSharedImage::ScopedMapping:
  base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index) override {
    size_t height_in_pixels;
    size_t row_size_in_bytes;

    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        Size().height(), format_, plane_index, &height_in_pixels));
    CHECK(gfx::RowSizeForBufferFormatChecked(Size().width(), format_,
                                             plane_index, &row_size_in_bytes));
    size_t span_length =
        Stride(plane_index) * (height_in_pixels - 1) + row_size_in_bytes;

    DCHECK_LT(plane_index, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    auto* data_ptr = data_.data();
    data_ptr += gfx::BufferOffsetForBufferFormat(Size(), format_, plane_index);

    // SAFETY: `data_` has been allocated to have the necessary size.
    return UNSAFE_BUFFERS(
        base::span<uint8_t>(reinterpret_cast<uint8_t*>(data_ptr), span_length));
  }
  size_t Stride(const uint32_t plane_index) override {
    DCHECK_LT(plane_index, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return gfx::RowSizeForBufferFormat(Size().width(), format_, plane_index);
  }
  gfx::Size Size() override { return size_; }
  bool IsSharedMemory() override { return true; }

 private:
  gfx::Size size_;
  gfx::BufferFormat format_;
  std::vector<uint8_t> data_;
};

class ScopedMappingSharedMemoryMapping
    : public ClientSharedImage::ScopedMapping {
 public:
  ScopedMappingSharedMemoryMapping(SharedImageMetadata metadata,
                                   base::WritableSharedMemoryMapping* mapping)
      : metadata_(metadata), mapping_(mapping) {}
  ~ScopedMappingSharedMemoryMapping() override = default;

  // ClientSharedImage::ScopedMapping:
  base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index) override {
    CHECK(mapping_->IsValid());
    CHECK_LT(plane_index,
             gfx::NumberOfPlanesForLinearBufferFormat(BufferFormat()));

    size_t height_in_pixels;
    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        Size().height(), BufferFormat(), plane_index, &height_in_pixels));
    size_t span_length = Stride(plane_index) * height_in_pixels;

    // SAFETY: The validity of the mapping combined with the construction of
    // that mapping guarantee that it contains at least `span_length` bytes
    // beyond the start of the plane.
    return UNSAFE_BUFFERS(
        base::span<uint8_t>(static_cast<uint8_t*>(mapping_->memory()) +
                                gfx::BufferOffsetForBufferFormat(
                                    Size(), BufferFormat(), plane_index),
                            span_length));
  }
  size_t Stride(const uint32_t plane_index) override {
    CHECK_LT(plane_index,
             gfx::NumberOfPlanesForLinearBufferFormat(BufferFormat()));
    return gfx::RowSizeForBufferFormat(Size().width(), BufferFormat(),
                                       plane_index);
  }
  gfx::Size Size() override { return metadata_.size; }
  bool IsSharedMemory() override { return true; }

 private:
  gfx::BufferFormat BufferFormat() {
    return viz::SinglePlaneSharedImageFormatToBufferFormat(metadata_.format);
  }

  SharedImageMetadata metadata_;
  raw_ptr<base::WritableSharedMemoryMapping> mapping_;
};

class ScopedMappingGpuMemoryBuffer : public ClientSharedImage::ScopedMapping {
 public:
  ScopedMappingGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format)
      : size_(size), format_(format) {}
  ~ScopedMappingGpuMemoryBuffer() override {
    if (buffer_) {
      buffer_->Unmap();
    }
  }

  // ClientSharedImage::ScopedMapping:
  base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index) override {
    CHECK(buffer_);

    size_t height_in_pixels;
    size_t row_size_in_bytes;

    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        Size().height(), format_, plane_index, &height_in_pixels));
    CHECK(gfx::RowSizeForBufferFormatChecked(Size().width(), format_,
                                             plane_index, &row_size_in_bytes));

    // Note that the stride might be larger than the row size due to padding.
    // For all rows other than the last, this is legal data for the client to
    // access as it's part of the buffer.  However, the final row is not
    // guaranteed to have padding (it's a system-dependent internal detail).
    // Thus, the data that is legal for the client to access should *not*
    // include any bytes beyond the actual end of the final row.
    size_t span_length =
        Stride(plane_index) * (height_in_pixels - 1) + row_size_in_bytes;

    // SAFETY: The underlying platform-specific buffer generation mechanisms
    // guarantee that the buffer contains at least `span_length` bytes following
    // the start of the plane, as that region is by definition the memory
    // storing the data of the plane.
    return UNSAFE_BUFFERS(base::span<uint8_t>(
        reinterpret_cast<uint8_t*>(buffer_->memory(plane_index)), span_length));
  }
  size_t Stride(const uint32_t plane_index) override {
    CHECK(buffer_);
    return buffer_->stride(plane_index);
  }
  gfx::Size Size() override { return size_; }
  bool IsSharedMemory() override {
    CHECK(buffer_);
    return buffer_->GetType() == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
  }
  bool Init(GpuMemoryBufferImpl* gpu_memory_buffer, bool is_already_mapped) {
    if (!gpu_memory_buffer) {
      LOG(ERROR) << "No GpuMemoryBuffer.";
      return false;
    }

    if (!is_already_mapped && !gpu_memory_buffer->Map()) {
      LOG(ERROR) << "Failed to map the buffer.";
      return false;
    }
    buffer_ = gpu_memory_buffer;
    return true;
  }

 private:
  // ScopedMappingGpuMemoryBuffer is essentially a wrapper around
  // GpuMemoryBuffer for now for simplicity and will be removed later.
  // TODO(crbug.com/40279377): Refactor/Rename GpuMemoryBuffer and its
  // implementations  as the end goal after all clients using GMB are
  // converted to use the ScopedMapping and notion of GpuMemoryBuffer is being
  // removed.
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
  RAW_PTR_EXCLUSION GpuMemoryBufferImpl* buffer_ = nullptr;
  gfx::Size size_;
  gfx::BufferFormat format_;
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE)
bool GMBIsNative(gfx::GpuMemoryBufferType gmb_type) {
  return gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::SHARED_MEMORY_BUFFER;
}
#endif

// Computes the texture target to use for a SharedImage that was created with
// `metadata` and the given type of GpuMemoryBuffer(Handle) supplied by the
// client (which will be gfx::EmptyBuffer if the client did not supply a
// GMB/GMBHandle). Conceptually:
// * On Mac the native buffer target is required if either (1) the client
//   gave a native buffer or (2) the usages require a native buffer. And this
//   matters only when running on ANGLE OpenGL/CGL - in all other cases we use
//   GL_TEXTURE_2D including with Graphite and on iOS (EAGL instead of CGL).
// * On Ozone the native buffer target is required iff external sampling is
//   being used, which is dictated by the format of the SharedImage. Note
//   * Fuchsia does not support import of external images to GL for usage with
//     external sampling.  The ClientSharedImage's texture target must be 0 in
//     the case where external sampling would be used to signal this lack of
//     support to the //media code, which detects the lack of support *based on*
//     on the texture target being 0.
// * On all other platforms GL_TEXTURE_2D is always used (external sampling is
//   supported in Chromium only on Ozone).
uint32_t ComputeTextureTargetForSharedImage(
    SharedImageMetadata metadata,
    gfx::GpuMemoryBufferType client_gmb_type,
    scoped_refptr<SharedImageInterface> sii) {
  CHECK(sii);
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
  return GL_TEXTURE_2D;
#elif BUILDFLAG(IS_MAC)
  // Check for IOSurfaces being used. We infer IOSurface based on scanout or
  // WebGPU usage, but that's not strictly correct e.g. with Graphite, WebGL
  // canvas back buffers will also use IOSurfaces always regardless of scanout.
  // However, in those cases we would be using GL_TEXTURE_2D anyway due to ANGLE
  // Metal (or Swiftshader for tests) being used.
  // Note that iOS uses GL_TEXTURE_2D even though it uses IOSurfaces -
  // GL_TEXTURE_RECTANGLE_ARB is in CGL which is Mac only.
  constexpr gpu::SharedImageUsageSet kUsagesRequiringNativeBuffer =
      SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU_READ |
      SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  const bool uses_native_buffer =
      GMBIsNative(client_gmb_type) ||
      metadata.usage.HasAny(kUsagesRequiringNativeBuffer);
  return uses_native_buffer
             ? sii->GetCapabilities().texture_target_for_io_surfaces
             : GL_TEXTURE_2D;
#else  // Ozone
  // Check for external sampling being used.
  if (!metadata.format.PrefersExternalSampler()) {
    return GL_TEXTURE_2D;
  }
  // The client should configure an SI to use external sampling only if they
  // have provided a native buffer to back that SI.
  CHECK(GMBIsNative(client_gmb_type));
  // See the note at the top of this function wrt Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
  return 0;
#else
  return GL_TEXTURE_EXTERNAL_OES;
#endif  // BUILDFLAG(IS_FUCHSIA)
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
}

}  // namespace

// static
std::unique_ptr<GpuMemoryBufferImpl>
ClientSharedImage::CreateGpuMemoryBufferImplFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SharedImageUsageSet si_usage,
    GpuMemoryBufferImpl::CopyNativeBufferToShMemCallback
        copy_native_buffer_to_shmem_callback,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          std::move(handle), size, format, usage);
#if BUILDFLAG(IS_APPLE)
    case gfx::IO_SURFACE_BUFFER: {
      bool is_read_only_cpu_usage =
          si_usage.Has(SHARED_IMAGE_USAGE_CPU_READ) &&
          !si_usage.Has(SHARED_IMAGE_USAGE_CPU_WRITE_ONLY);
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          std::move(handle), size, format, is_read_only_cpu_usage);
    }
#endif
#if BUILDFLAG(IS_OZONE)
    case gfx::NATIVE_PIXMAP: {
      // NOTE: This is not used beyond the lifetime of CreateFromHandle().
      auto client_native_pixmap_factory =
          ui::CreateClientNativePixmapFactoryOzone();
      return GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory.get(), std::move(handle), size, format,
          usage);
    }
#endif
#if BUILDFLAG(IS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return GpuMemoryBufferImplDXGI::CreateFromHandle(
          std::move(handle), size, format,
          std::move(copy_native_buffer_to_shmem_callback), std::move(pool));
#endif
#if BUILDFLAG(IS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return nullptr;
#endif
    default:
      // TODO(dcheng): Remove default case (https://crbug.com/676224).
      NOTREACHED() << gfx::BufferFormatToString(format) << ", "
                   << gfx::BufferUsageToString(usage);
  }
}

// static
std::unique_ptr<ClientSharedImage::ScopedMapping>
ClientSharedImage::ScopedMapping::Create(
    SharedImageMetadata metadata,
    base::WritableSharedMemoryMapping* mapping) {
  return std::make_unique<ScopedMappingSharedMemoryMapping>(metadata, mapping);
}

// static
std::unique_ptr<ClientSharedImage::ScopedMapping>
ClientSharedImage::ScopedMapping::Create(SharedImageMetadata metadata,
                                         GpuMemoryBufferImpl* gpu_memory_buffer,
                                         bool is_already_mapped) {
  auto scoped_mapping = base::WrapUnique(new ScopedMappingGpuMemoryBuffer(
      metadata.size,
      viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
          metadata.format)));
  if (!scoped_mapping->Init(gpu_memory_buffer, is_already_mapped)) {
    LOG(ERROR) << "ScopedMapping init failed.";
    return nullptr;
  }
  return scoped_mapping;
}

// static
void ClientSharedImage::ScopedMapping::StartCreateAsync(
    SharedImageMetadata metadata,
    GpuMemoryBufferImpl* gpu_memory_buffer,
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb) {
  gpu_memory_buffer->MapAsync(
      base::BindOnce(&ClientSharedImage::ScopedMapping::FinishCreateAsync,
                     metadata, gpu_memory_buffer, std::move(result_cb)));
}

// static
void ClientSharedImage::ScopedMapping::FinishCreateAsync(
    SharedImageMetadata metadata,
    GpuMemoryBufferImpl* gpu_memory_buffer,
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
    bool success) {
  std::unique_ptr<ClientSharedImage::ScopedMapping> mapping;
  if (success) {
    mapping = ClientSharedImage::ScopedMapping::Create(
        metadata, gpu_memory_buffer, /*is_already_mapped=*/true);
  }
  std::move(result_cb).Run(std::move(mapping));
}

SkPixmap ClientSharedImage::ScopedMapping::GetSkPixmapForPlane(
    const uint32_t plane_index,
    SkImageInfo sk_image_info) {
  return SkPixmap(sk_image_info, GetMemoryForPlane(plane_index).data(),
                  Stride(plane_index));
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& info,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    gfx::GpuMemoryBufferType gmb_type)
    : mailbox_(mailbox),
      metadata_(info.meta),
      debug_label_(info.debug_label),
      creation_sync_token_(sync_token),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
  texture_target_ = ComputeTextureTargetForSharedImage(metadata_, gmb_type,
                                                       sii_holder_->Get());
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& info,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    base::WritableSharedMemoryMapping mapping)
    : ClientSharedImage(mailbox,
                        info,
                        sync_token,
                        sii_holder,
                        gfx::SHARED_MEMORY_BUFFER) {
  shared_memory_mapping_ = std::move(mapping);
  is_software_ = true;
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& info,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(info.meta),
      debug_label_(info.debug_label),
      creation_sync_token_(sync_token),
      sii_holder_(std::move(sii_holder)),
      texture_target_(texture_target) {
  // TODO(crbug.com/391788839): Create GpuMemoryBuffer from handle.
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target);
#endif
}

ClientSharedImage::ClientSharedImage(
    ExportedSharedImage exported_si,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder)
    : mailbox_(exported_si.mailbox_),
      metadata_(exported_si.metadata_),
      debug_label_(exported_si.debug_label_),
      creation_sync_token_(exported_si.creation_sync_token_),
      buffer_usage_(exported_si.buffer_usage_),
      sii_holder_(std::move(sii_holder)),
      texture_target_(exported_si.texture_target_) {
  if (exported_si.buffer_handle_) {
    gpu_memory_buffer_ = CreateGpuMemoryBufferImplFromHandle(
        std::move(exported_si.buffer_handle_.value()), metadata_.size,
        viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
            metadata_.format),
        exported_si.buffer_usage_.value(), metadata_.usage,
        base::BindRepeating(
            &ClientSharedImage::CopyNativeGmbToSharedMemoryAsync,
            base::Unretained(this)));
  }
  CHECK(!mailbox_.IsZero());
  CHECK(sii_holder_);
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target_);
#endif
}

ClientSharedImage::ClientSharedImage(ExportedSharedImage exported_si)
    : mailbox_(exported_si.mailbox_),
      metadata_(exported_si.metadata_),
      debug_label_(exported_si.debug_label_),
      creation_sync_token_(exported_si.creation_sync_token_),
      buffer_usage_(exported_si.buffer_usage_),
      texture_target_(exported_si.texture_target_) {
  if (exported_si.buffer_handle_) {
    gpu_memory_buffer_ = CreateGpuMemoryBufferImplFromHandle(
        std::move(exported_si.buffer_handle_.value()), metadata_.size,
        viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
            metadata_.format),
        exported_si.buffer_usage_.value(), metadata_.usage,
        base::BindRepeating(
            &ClientSharedImage::CopyNativeGmbToSharedMemoryAsync,
            base::Unretained(this)));
  }
  CHECK(!mailbox_.IsZero());
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target_);
#endif
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageInfo& info,
    const SyncToken& sync_token,
    GpuMemoryBufferHandleInfo handle_info,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool)
    : mailbox_(mailbox),
      metadata_(info.meta),
      debug_label_(info.debug_label),
      creation_sync_token_(sync_token),
      gpu_memory_buffer_(CreateGpuMemoryBufferImplFromHandle(
          std::move(handle_info.handle),
          metadata_.size,
          viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
              metadata_.format),
          handle_info.buffer_usage,
          info.meta.usage,
          base::BindRepeating(
              &ClientSharedImage::CopyNativeGmbToSharedMemoryAsync,
              base::Unretained(this)),
          std::move(shared_memory_pool))),
      buffer_usage_(handle_info.buffer_usage),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
  CHECK(gpu_memory_buffer_);
  texture_target_ = ComputeTextureTargetForSharedImage(
      metadata_, gpu_memory_buffer_->GetType(), sii_holder_->Get());
}

ClientSharedImage::ClientSharedImage(const Mailbox& mailbox,
                                     const SharedImageInfo& info)
    : mailbox_(mailbox), metadata_(info.meta), debug_label_(info.debug_label) {
  CHECK(!mailbox.IsZero());
  texture_target_ = GL_TEXTURE_2D;
}

ClientSharedImage::~ClientSharedImage() {
  if (!HasHolder()) {
    return;
  }

  auto sii = sii_holder_->Get();
  if (sii) {
    sii->DestroySharedImage(destruction_sync_token_, mailbox_);
  }
}

size_t ClientSharedImage::GetStrideForVideoFrame(uint32_t plane_index) const {
  if (async_map_invoked_callback_for_testing_) {
    return gfx::RowSizeForBufferFormat(
        size().width(),
        viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
            format()),
        plane_index);
  }
  CHECK(gpu_memory_buffer_);
  return gpu_memory_buffer_->stride(plane_index);
}

// Returns whether the underlying resource is shared memory without needing to
// Map() the shared image. This method is supposed to be used by VideoFrame
// temporarily as mentioned above in ::GetStrideForVideoFrame().
bool ClientSharedImage::IsSharedMemoryForVideoFrame() const {
  if (async_map_invoked_callback_for_testing_) {
    return true;
  }
  CHECK(gpu_memory_buffer_);
  return gpu_memory_buffer_->GetType() ==
         gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
}

bool ClientSharedImage::AsyncMappingIsNonBlocking() const {
  if (async_map_invoked_callback_for_testing_) {
    return true;
  }
  CHECK(gpu_memory_buffer_);
  return gpu_memory_buffer_->AsyncMappingIsNonBlocking();
}

std::unique_ptr<ClientSharedImage::ScopedMapping> ClientSharedImage::Map() {
  std::unique_ptr<ClientSharedImage::ScopedMapping> scoped_mapping;
  if (shared_memory_mapping_.IsValid()) {
    scoped_mapping = ScopedMapping::Create(metadata_, &shared_memory_mapping_);
  } else {
    scoped_mapping = ScopedMapping::Create(metadata_, gpu_memory_buffer_.get(),
                                           /*is_already_mapped=*/false);
  }

  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping";
  }
  return scoped_mapping;
}

void ClientSharedImage::FinishMapAsyncForTests(
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
    bool success) {
  std::unique_ptr<ScopedMapping> mapping;
  if (success) {
    mapping = std::make_unique<ScopedMappingForTests>(
        size(),
        viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
            format()));
  }
  std::move(result_cb).Run(std::move(mapping));
}

void ClientSharedImage::MapAsync(
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb) {
  if (async_map_invoked_callback_for_testing_) {
    if (premapped_for_testing_) {
      FinishMapAsyncForTests(std::move(result_cb), true);
    } else {
      async_map_invoked_callback_for_testing_.Run(
          base::BindOnce(&ClientSharedImage::FinishMapAsyncForTests,
                         base::Unretained(this), std::move(result_cb)));
    }
    return;
  }

  ScopedMapping::StartCreateAsync(metadata_, gpu_memory_buffer_.get(),
                                  std::move(result_cb));
}

gfx::GpuMemoryBufferHandle ClientSharedImage::CloneGpuMemoryBufferHandle()
    const {
  CHECK(gpu_memory_buffer_);
  return gpu_memory_buffer_->CloneHandle();
}

uint32_t ClientSharedImage::GetTextureTarget() {
#if !BUILDFLAG(IS_FUCHSIA)
  // Check that `texture_target_` has been initialized (note that on Fuchsia it
  // is possible for `texture_target_` to be initialized to 0: Fuchsia does not
  // support import of external images to GL for usage with external sampling.
  // SetTextureTarget() sets the texture target to 0 in the case where external
  // sampling would be used to signal this lack of support to the //media code,
  // which detects the lack of support *based on* on the texture target being
  // 0).
  CHECK(texture_target_);
#endif
  return texture_target_;
}

scoped_refptr<ClientSharedImage> ClientSharedImage::MakeUnowned() {
  return ClientSharedImage::ImportUnowned(Export());
}

ExportedSharedImage ClientSharedImage::Export(bool with_buffer_handle) {
  if (creation_sync_token_.HasData() &&
      !creation_sync_token_.verified_flush()) {
    sii_holder_->Get()->VerifySyncToken(creation_sync_token_);
  }
  std::optional<gfx::GpuMemoryBufferHandle> buffer_handle;
  std::optional<gfx::BufferUsage> buffer_usage;
  if (with_buffer_handle && gpu_memory_buffer_) {
    buffer_handle = gpu_memory_buffer_->CloneHandle();
    buffer_usage = buffer_usage_.value();
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_,
                             debug_label_, std::move(buffer_handle),
                             buffer_usage, texture_target_);
}

scoped_refptr<ClientSharedImage> ClientSharedImage::ImportUnowned(
    ExportedSharedImage exported_shared_image) {
  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(std::move(exported_shared_image)));
}

gpu::SyncToken ClientSharedImage::BackingWasExternallyUpdated(
    const gpu::SyncToken& sync_token) {
  CHECK(sii_holder_);
  auto sii = sii_holder_->Get();
  if (!sii) {
    return gpu::SyncToken();
  }

  sii->UpdateSharedImage(sync_token, mailbox());
  return sii->GenUnverifiedSyncToken();
}

void ClientSharedImage::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    int importance) {
  auto tracing_guid = GetGUIDForTracing();
  pmd->CreateSharedGlobalAllocatorDump(tracing_guid);
  pmd->AddOwnershipEdge(buffer_dump_guid, tracing_guid, importance);
}

void ClientSharedImage::BeginAccess(bool readonly) {
  base::AutoLock lock(lock_);
  if (readonly) {
    CHECK(!has_writer_ ||
          usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
    num_readers_++;
  } else {
    CHECK(!has_writer_);
    CHECK(num_readers_ == 0 ||
          usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
    has_writer_ = true;
  }
}

void ClientSharedImage::EndAccess(bool readonly) {
  base::AutoLock lock(lock_);
  if (readonly) {
    CHECK(num_readers_ > 0);
    num_readers_--;
  } else {
    CHECK(has_writer_);
    has_writer_ = false;
  }
}

std::unique_ptr<SharedImageTexture> ClientSharedImage::CreateGLTexture(
    gles2::GLES2Interface* gl) {
  SCOPED_CRASH_KEY_STRING32("ClientSharedImage", "DebugLabel", debug_label_);
  SCOPED_CRASH_KEY_NUMBER("ClientSharedImage", "Usage", metadata_.usage);
  DUMP_WILL_BE_CHECK(metadata_.usage.Has(SHARED_IMAGE_USAGE_GLES2_READ) ||
                     metadata_.usage.Has(SHARED_IMAGE_USAGE_GLES2_WRITE));
  return base::WrapUnique(new SharedImageTexture(gl, this));
}

std::unique_ptr<RasterScopedAccess> ClientSharedImage::BeginRasterAccess(
    InterfaceBase* raster_interface,
    const SyncToken& sync_token,
    bool readonly) {
  bool has_raster_usage =
      metadata_.usage.Has(SHARED_IMAGE_USAGE_RASTER_READ) ||
      metadata_.usage.Has(SHARED_IMAGE_USAGE_RASTER_WRITE) ||
      metadata_.usage.Has(SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE);
  if (!has_raster_usage) {
    SCOPED_CRASH_KEY_STRING32("ClientSharedImage", "DebugLabel", debug_label_);
    SCOPED_CRASH_KEY_NUMBER("ClientSharedImage", "Usage", metadata_.usage);
    DUMP_WILL_BE_CHECK(has_raster_usage);
  }
  return base::WrapUnique(
      new RasterScopedAccess(raster_interface, this, sync_token, readonly));
}

std::unique_ptr<RasterScopedAccess>
ClientSharedImage::BeginGLAccessForCopySharedImage(InterfaceBase* gl_interface,
                                                   const SyncToken& sync_token,
                                                   bool readonly) {
  return BeginRasterAccess(gl_interface, sync_token, readonly);
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImage::SetUsePreMappedMemory(bool use_premapped_memory) {
  CHECK(gpu_memory_buffer_);
  gpu_memory_buffer_->SetUsePreMappedMemory(use_premapped_memory);
}
#endif

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting() {
  return CreateForTesting(viz::SinglePlaneFormat::kRGBA_8888, GL_TEXTURE_2D);
}

scoped_refptr<ClientSharedImage> ClientSharedImage::CreateSoftwareForTesting() {
  auto shared_image = CreateForTesting();  // IN-TEST
  shared_image->is_software_ = true;
  return shared_image;
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    const SharedImageMetadata& metadata) {
  return CreateForTesting(metadata, GL_TEXTURE_2D);  // IN-TEST
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    viz::SharedImageFormat format,
    uint32_t texture_target) {
  SharedImageMetadata metadata;
  metadata.format = format;
  metadata.size = gfx::Size(64, 64);
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();

  return CreateForTesting(metadata, texture_target);
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    SharedImageUsageSet usage) {
  SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.size = gfx::Size(64, 64);
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = usage;

  return CreateForTesting(metadata, GL_TEXTURE_2D);
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    const SharedImageMetadata& metadata,
    uint32_t texture_target) {
  return ImportUnowned(ExportedSharedImage(
      Mailbox::Generate(), metadata, SyncToken(), "CSICreateForTesting",
      std::nullopt, std::nullopt, texture_target));
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    gfx::BufferUsage buffer_usage,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder) {
  SharedImageInfo info(metadata, "CSICreateForTesting");

  auto gpu_memory_buffer = GpuMemoryBufferImplSharedMemory::CreateForTesting(
      info.meta.size,
      viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
          info.meta.format),
      buffer_usage);

  // Since the |gpu_memory_buffer| here is always a shared memory, clear the
  // external sampler prefs if it is already set by client.
  // https://issues.chromium.org/339546249.
  if (info.meta.format.PrefersExternalSampler()) {
    info.meta.format.ClearPrefersExternalSampler();
  }

  auto client_si = base::MakeRefCounted<ClientSharedImage>(
      mailbox, info, sync_token, sii_holder, gfx::SHARED_MEMORY_BUFFER);
  client_si->gpu_memory_buffer_ = std::move(gpu_memory_buffer);
  client_si->buffer_usage_ = buffer_usage;
  return client_si;
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    bool premapped,
    const AsyncMapInvokedCallback& callback,
    gfx::BufferUsage buffer_usage,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder) {
  SharedImageInfo info(metadata, "CSICreateForTesting");

  auto client_si = base::MakeRefCounted<ClientSharedImage>(
      mailbox, info, sync_token, sii_holder, gfx::SHARED_MEMORY_BUFFER);
  client_si->async_map_invoked_callback_for_testing_ = callback;
  client_si->premapped_for_testing_ = premapped;
  client_si->buffer_usage_ = buffer_usage;
  return client_si;
}

void ClientSharedImage::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  // Lazily create the |task_runner_|.
  if (!copy_native_buffer_to_shmem_task_runner_) {
    copy_native_buffer_to_shmem_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
    CHECK(copy_native_buffer_to_shmem_task_runner_);
  }

  if (!copy_native_buffer_to_shmem_task_runner_->BelongsToCurrentThread()) {
    copy_native_buffer_to_shmem_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ClientSharedImage::CopyNativeGmbToSharedMemoryAsync,
                       base::Unretained(this), std::move(buffer_handle),
                       std::move(memory_region), std::move(callback)));
    return;
  }

  auto sii = sii_holder_->Get();
  if (!sii) {
    DLOG(WARNING) << "No SharedImageInterface.";
    std::move(callback).Run(false);
    return;
  }
  sii->CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  /*result=*/false));
}

std::unique_ptr<WebGPUTextureScopedAccess>
ClientSharedImage::BeginWebGPUTextureAccess(
    webgpu::WebGPUInterface* webgpu,
    const SyncToken& sync_token,
    const wgpu::dawn::wire::client::Device& device,
    const wgpu::dawn::wire::client::TextureDescriptor& desc,
    uint64_t usage,
    webgpu::MailboxFlags mailbox_flags) {
  return base::WrapUnique(new WebGPUTextureScopedAccess(
      webgpu, this, sync_token, device, desc, usage, mailbox_flags));
}

ExportedSharedImage::ExportedSharedImage() = default;
ExportedSharedImage::~ExportedSharedImage() = default;

ExportedSharedImage::ExportedSharedImage(ExportedSharedImage&& other) = default;
ExportedSharedImage& ExportedSharedImage::operator=(
    ExportedSharedImage&& other) = default;

ExportedSharedImage::ExportedSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    std::string debug_label,
    std::optional<gfx::GpuMemoryBufferHandle> buffer_handle,
    std::optional<gfx::BufferUsage> buffer_usage,
    uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      debug_label_(debug_label),
      buffer_handle_(std::move(buffer_handle)),
      buffer_usage_(buffer_usage),
      texture_target_(texture_target) {}

ExportedSharedImage ExportedSharedImage::Clone() const {
  std::optional<gfx::GpuMemoryBufferHandle> handle = std::nullopt;
  if (buffer_handle_.has_value()) {
    handle = buffer_handle_->Clone();
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_,
                             debug_label_, std::move(handle), buffer_usage_,
                             texture_target_);
}

SharedImageTexture::ScopedAccess::ScopedAccess(SharedImageTexture* texture,
                                               const SyncToken& sync_token,
                                               bool readonly)
    : texture_(texture), readonly_(readonly) {
  texture_->gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  texture_->gl_->BeginSharedImageAccessDirectCHROMIUM(
      texture->id(), (readonly_)
                         ? GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM
                         : GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

SharedImageTexture::ScopedAccess::~ScopedAccess() {
  CHECK(is_access_ended_);
}

void SharedImageTexture::ScopedAccess::DidEndAccess() {
  is_access_ended_ = true;
  texture_->DidEndAccess(readonly_);
}

// static
SyncToken SharedImageTexture::ScopedAccess::EndAccess(
    std::unique_ptr<SharedImageTexture::ScopedAccess> scoped_shared_image) {
  gles2::GLES2Interface* gl = scoped_shared_image->texture_->gl_;
  gl->EndSharedImageAccessDirectCHROMIUM(scoped_shared_image->texture_->id());
  scoped_shared_image->DidEndAccess();
  SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

SharedImageTexture::SharedImageTexture(gles2::GLES2Interface* gl,
                                       ClientSharedImage* shared_image)
    : gl_(gl), shared_image_(shared_image) {
  CHECK(gl_);
  CHECK(shared_image_);
  gl_->WaitSyncTokenCHROMIUM(
      shared_image_->creation_sync_token().GetConstData());
  id_ = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(
      shared_image_->mailbox().name);
}

SharedImageTexture::~SharedImageTexture() {
  CHECK(!has_active_access_);
  gl_->DeleteTextures(1, &id_);
}

std::unique_ptr<SharedImageTexture::ScopedAccess>
SharedImageTexture::BeginAccess(const SyncToken& sync_token, bool readonly) {
  CHECK(!has_active_access_);
  SCOPED_CRASH_KEY_STRING32("ClientSharedImage", "DebugLabel",
                            shared_image_->debug_label());
  SCOPED_CRASH_KEY_NUMBER("ClientSharedImage", "Usage", shared_image_->usage());
  if (readonly) {
    DUMP_WILL_BE_CHECK(
        shared_image_->usage().Has(SHARED_IMAGE_USAGE_GLES2_READ));
  } else {
    DUMP_WILL_BE_CHECK(
        shared_image_->usage().Has(SHARED_IMAGE_USAGE_GLES2_WRITE));
  }
  has_active_access_ = true;
  shared_image_->BeginAccess(readonly);
  return base::WrapUnique(
      new SharedImageTexture::ScopedAccess(this, sync_token, readonly));
}

void SharedImageTexture::DidEndAccess(bool readonly) {
  has_active_access_ = false;
  shared_image_->EndAccess(readonly);
}

RasterScopedAccess::RasterScopedAccess(InterfaceBase* raster_interface,
                                       ClientSharedImage* shared_image,
                                       const SyncToken& sync_token,
                                       bool readonly)
    : raster_interface_(raster_interface),
      shared_image_(shared_image),
      readonly_(readonly) {
  CHECK(raster_interface_);
  shared_image_->BeginAccess(readonly);
  raster_interface_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  bool has_read_usage =
      shared_image_->usage().Has(SHARED_IMAGE_USAGE_RASTER_READ) ||
      shared_image_->usage().Has(SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE);
  bool has_write_usage =
      shared_image_->usage().Has(SHARED_IMAGE_USAGE_RASTER_WRITE);
  if (readonly && !has_read_usage) {
    SCOPED_CRASH_KEY_STRING32("ClientSharedImage", "DebugLabel",
                              shared_image_->debug_label());
    SCOPED_CRASH_KEY_NUMBER("ClientSharedImage", "Usage",
                            shared_image_->usage());
    DUMP_WILL_BE_CHECK(has_read_usage);
  } else if (!readonly && !has_write_usage) {
    SCOPED_CRASH_KEY_STRING32("ClientSharedImage", "DebugLabel",
                              shared_image_->debug_label());
    SCOPED_CRASH_KEY_NUMBER("ClientSharedImage", "Usage",
                            shared_image_->usage());
    DUMP_WILL_BE_CHECK(has_write_usage);
  }
}

// static
SyncToken RasterScopedAccess::EndAccess(
    std::unique_ptr<RasterScopedAccess> scoped_access) {
  InterfaceBase* raster_interface = scoped_access->raster_interface_;
  SyncToken sync_token;
  scoped_access->shared_image_->EndAccess(scoped_access->readonly_);
  raster_interface->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

WebGPUTextureScopedAccess::WebGPUTextureScopedAccess(
    webgpu::WebGPUInterface* webgpu,
    ClientSharedImage* shared_image,
    const SyncToken& sync_token,
    const wgpu::dawn::wire::client::Device& device,
    const wgpu::dawn::wire::client::TextureDescriptor& desc,
    uint64_t usage,
    webgpu::MailboxFlags mailbox_flags)
    : webgpu_(webgpu), shared_image_(shared_image) {
  // Wait on any work using the image.
  webgpu_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Produce and inject image to WebGPU texture
  webgpu::ReservedTexture reservation = webgpu_->ReserveTexture(
      device.Get(), &static_cast<const WGPUTextureDescriptor&>(desc));
  DCHECK(reservation.texture);

  // If either |desc.usage| or |usage| contains the following flags, the access
  // is not read-only.
  const wgpu::TextureUsage write_flags = wgpu::TextureUsage::CopyDst |
                                         wgpu::TextureUsage::RenderAttachment |
                                         wgpu::TextureUsage::StorageBinding;
  readonly_ = !((desc.usage | wgpu::TextureUsage{usage}) & write_flags);
  shared_image_->BeginAccess(readonly_);
  texture_ = base::WrapUnique(
      new wgpu::Texture(wgpu::Texture::Acquire(reservation.texture)));
  device_id_ = reservation.deviceId;
  device_generation_ = reservation.deviceGeneration;
  texture_id_ = reservation.id;
  texture_generation_ = reservation.generation;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu_->AssociateMailbox(
      device_id_, device_generation_, texture_id_, texture_generation_,
      static_cast<uint64_t>(desc.usage), static_cast<uint64_t>(usage),
      reinterpret_cast<const WGPUTextureFormat*>(desc.viewFormats),
      base::checked_cast<GLuint>(desc.viewFormatCount), mailbox_flags,
      shared_image->mailbox());
}

WebGPUTextureScopedAccess::~WebGPUTextureScopedAccess() = default;

SyncToken WebGPUTextureScopedAccess::EndAccess(
    std::unique_ptr<WebGPUTextureScopedAccess> scoped_access) {
  webgpu::WebGPUInterface* webgpu = scoped_access->webgpu_;
  SyncToken finished_access_token;
  if (scoped_access->needs_present_) {
    webgpu->DissociateMailboxForPresent(
        scoped_access->device_id_, scoped_access->device_generation_,
        scoped_access->texture_id_, scoped_access->texture_generation_);
  } else {
    webgpu->DissociateMailbox(scoped_access->texture_id_,
                              scoped_access->texture_generation_);
  }

  scoped_access->shared_image_->EndAccess(scoped_access->readonly_);
  webgpu->GenUnverifiedSyncTokenCHROMIUM(finished_access_token.GetData());
  return finished_access_token;
}

const wgpu::dawn::wire::client::Texture& WebGPUTextureScopedAccess::texture() {
  return *texture_.get();
}

void WebGPUTextureScopedAccess::SetNeedsPresent(bool needs_present) {
  needs_present_ = needs_present;
}

}  // namespace gpu
