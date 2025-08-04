// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

bool CanUseGPU() {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         !context_provider_wrapper->ContextProvider().IsContextLost();
}

}  // namespace

CanvasRenderingContextHost::CanvasRenderingContextHost(HostType host_type,
                                                       const gfx::Size& size)
    : host_type_(host_type), size_(size) {}

void CanvasRenderingContextHost::Trace(Visitor* visitor) const {
  visitor->Trace(plain_text_painter_);
  visitor->Trace(unique_font_selector_);
}

void CanvasRenderingContextHost::RecordCanvasSizeToUMA() {
  if (did_record_canvas_size_to_uma_)
    return;
  did_record_canvas_size_to_uma_ = true;

  switch (host_type_) {
    case HostType::kNone:
      NOTREACHED();
    case HostType::kCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                  std::sqrt(Size().Area64()), 1, 5000, 100);
      break;
    case HostType::kOffscreenCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                  std::sqrt(Size().Area64()), 1, 5000, 100);
      break;
  }
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage() const {
  if (!IsValidImageSize()) {
    return nullptr;
  }
  SkImageInfo info = SkImageInfo::Make(
      gfx::SizeToSkISize(Size()),
      viz::ToClosestSkColorType(GetRenderingContextFormat()),
      kPremul_SkAlphaType, GetRenderingContextColorSpace().ToSkColorSpace());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(info, info.minRowBytes(), nullptr);
  if (!surface) {
    return nullptr;
  }
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

bool CanvasRenderingContextHost::IsValidImageSize() const {
  const gfx::Size size = Size();
  if (size.IsEmpty()) {
    return false;
  }
  base::CheckedNumeric<int> area = size.GetCheckedArea();
  // Firefox limits width/height to 32767 pixels, but slows down dramatically
  // before it reaches that limit. We limit by area instead, giving us larger
  // maximum dimensions, in exchange for a smaller maximum canvas size.
  static constexpr int kMaxCanvasArea =
      32768 * 8192;  // Maximum canvas area in CSS pixels
  if (!area.IsValid() || area.ValueOrDie() > kMaxCanvasArea) {
    return false;
  }
  // In Skia, we will also limit width/height to 65535.
  static constexpr int kMaxSkiaDim =
      65535;  // Maximum width/height in CSS pixels.
  if (size.width() > kMaxSkiaDim || size.height() > kMaxSkiaDim) {
    return false;
  }
  return true;
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize();
}

void CanvasRenderingContextHost::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (RenderingContext())
    RenderingContext()->RestoreCanvasMatrixClipStack(canvas);
}

bool CanvasRenderingContextHost::IsWebGL() const {
  return RenderingContext() && RenderingContext()->IsWebGL();
}

bool CanvasRenderingContextHost::IsWebGPU() const {
  return RenderingContext() && RenderingContext()->IsWebGPU();
}

bool CanvasRenderingContextHost::IsRenderingContext2D() const {
  return RenderingContext() && RenderingContext()->IsRenderingContext2D();
}

bool CanvasRenderingContextHost::IsImageBitmapRenderingContext() const {
  return RenderingContext() &&
         RenderingContext()->IsImageBitmapRenderingContext();
}

SkAlphaType CanvasRenderingContextHost::GetRenderingContextAlphaType() const {
  return RenderingContext() ? RenderingContext()->GetAlphaType()
                            : kPremul_SkAlphaType;
}

viz::SharedImageFormat CanvasRenderingContextHost::GetRenderingContextFormat()
    const {
  return RenderingContext() ? RenderingContext()->GetSharedImageFormat()
                            : GetN32FormatForCanvas();
}

gfx::ColorSpace CanvasRenderingContextHost::GetRenderingContextColorSpace()
    const {
  return RenderingContext() ? RenderingContext()->GetColorSpace()
                            : gfx::ColorSpace::CreateSRGB();
}

PlainTextPainter& CanvasRenderingContextHost::GetPlainTextPainter() {
  if (!plain_text_painter_) {
    plain_text_painter_ =
        MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
    UseCounter::Count(GetTopExecutionContext(), WebFeature::kCanvasTextNg);
  }
  return *plain_text_painter_;
}

RasterMode CanvasRenderingContextHost::GetRasterModeForCanvas2D() const {
  CHECK(IsRenderingContext2D());
  return IsAccelerated() ? RasterMode::kGPU : RasterMode::kCPU;
}

bool CanvasRenderingContextHost::IsOffscreenCanvas() const {
  return host_type_ == HostType::kOffscreenCanvasHost;
}

bool CanvasRenderingContextHost::IsAccelerated() const {
  if (RenderingContext()) {
    // This method is supported only on 2D contexts.
    CHECK(IsRenderingContext2D());
    return RenderingContext()->IsHibernating()
               ? false
               : RenderingContext()->Is2DCanvasAccelerated();
  }

  // Whether or not to accelerate is not yet resolved, the canvas cannot be
  // accelerated if the gpu context is lost.
  return ShouldTryToUseGpuRaster();
}

ImageBitmapSourceStatus CanvasRenderingContextHost::CheckUsability() const {
  const gfx::Size size = Size();
  if (size.IsEmpty()) {
    return base::unexpected(size.width() == 0
                                ? ImageBitmapSourceError::kZeroWidth
                                : ImageBitmapSourceError::kZeroHeight);
  }
  return base::ok();
}

IdentifiableToken CanvasRenderingContextHost::IdentifiabilityInputDigest(
    const CanvasRenderingContext* const context) const {
  const uint64_t context_digest =
      context ? context->IdentifiableTextToken().ToUkmMetricValue() : 0;
  const uint64_t context_type = static_cast<uint64_t>(
      context ? context->GetRenderingAPI()
              : CanvasRenderingContext::CanvasRenderingAPI::kUnknown);
  const bool encountered_skipped_ops =
      context && context->IdentifiabilityEncounteredSkippedOps();
  const bool encountered_sensitive_ops =
      context && context->IdentifiabilityEncounteredSensitiveOps();
  const bool encountered_partially_digested_image =
      context && context->IdentifiabilityEncounteredPartiallyDigestedImage();
  // Bits [0-3] are the context type, bits [4-6] are skipped ops, sensitive
  // ops, and partial image ops bits, respectively. The remaining bits are
  // for the canvas digest.
  uint64_t final_digest = (context_digest << 7) | context_type;
  if (encountered_skipped_ops)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kSkipped;
  if (encountered_sensitive_ops)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kSensitive;
  if (encountered_partially_digested_image)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kPartiallyDigested;
  return final_digest;
}

void CanvasRenderingContextHost::PageVisibilityChanged() {
  bool page_visible = IsPageVisible();
  if (RenderingContext()) {
    RenderingContext()->PageVisibilityChanged();
    if (page_visible) {
      RenderingContext()->SendContextLostEventIfNeeded();
    }
  }
  if (!page_visible && (IsWebGL() || IsWebGPU())) {
    DiscardResources();
  }
}

bool CanvasRenderingContextHost::ContextHasOpenLayers(
    const CanvasRenderingContext* context) const {
  return context != nullptr && context->IsRenderingContext2D() &&
         context->LayerCount() != 0;
}

void CanvasRenderingContextHost::SetPreferred2DRasterMode(RasterModeHint hint) {
  // TODO(junov): move code that switches between CPU and GPU rasterization
  // to here.
  preferred_2d_raster_mode_ = hint;
}

bool CanvasRenderingContextHost::ShouldTryToUseGpuRaster() const {
  return preferred_2d_raster_mode_ == RasterModeHint::kPreferGPU && CanUseGPU();
}

}  // namespace blink
