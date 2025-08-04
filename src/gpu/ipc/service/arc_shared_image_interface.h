// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_ARC_SHARED_IMAGE_INTERFACE_H_
#define GPU_IPC_SERVICE_ARC_SHARED_IMAGE_INTERFACE_H_

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"

namespace gpu {

class GpuChannelManager;

// Used by ArcVideoEncodeAccelerator to create mappable SharedImages from
// GpuMemoryBufferHandles passed over from ARC.
class GPU_IPC_SERVICE_EXPORT ArcSharedImageInterface
    : public SharedImageInterface {
 public:
  static scoped_refptr<ArcSharedImageInterface> Create(
      GpuChannelManager* gpu_channel_manager);

  explicit ArcSharedImageInterface(
      std::unique_ptr<SharedImageFactory> shared_image_factory);

  ArcSharedImageInterface(const ArcSharedImageInterface&) = delete;
  ArcSharedImageInterface& operator=(const ArcSharedImageInterface&) = delete;

  // SharedImageInterface:
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      std::optional<SharedImagePoolId> pool_id = std::nullopt) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      std::optional<SharedImagePoolId> pool_id = std::nullopt) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) override;
  scoped_refptr<ClientSharedImage> CreateSharedImageForMLTensor(
      std::string debug_label,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      gpu::SharedImageUsageSet usage) override;
  scoped_refptr<ClientSharedImage> CreateSharedImageForSoftwareCompositor(
      const SharedImageInfo& si_info) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) override;
  scoped_refptr<ClientSharedImage> ImportSharedImage(
      ExportedSharedImage exported_shared_image) override;
  SwapChainSharedImages CreateSwapChain(viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        SharedImageUsageSet usage,
                                        std::string_view debug_label) override;
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;
  SyncToken GenUnverifiedSyncToken() override;
  SyncToken GenVerifiedSyncToken() override;
  void VerifySyncToken(SyncToken& sync_token) override;
  void WaitSyncToken(const SyncToken& sync_token) override;

  const SharedImageCapabilities& GetCapabilities() override;

 private:
  ~ArcSharedImageInterface() override;

  bool MakeContextCurrent(bool needs_gl = false);

  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_ARC_SHARED_IMAGE_INTERFACE_H_
