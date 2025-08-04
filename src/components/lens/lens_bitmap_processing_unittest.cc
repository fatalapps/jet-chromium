// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_bitmap_processing.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_client_logs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_phase_latencies_metadata.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace lens {
constexpr int kImageCompressionQuality = 30;

class LensBitmapProcessingTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  const SkBitmap CreateOpaqueBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    bitmap.setAlphaType(kOpaque_SkAlphaType);
    return bitmap;
  }

  std::string GetJpegBytesForBitmap(const SkBitmap& bitmap) {
    std::optional<std::vector<uint8_t>> data =
        gfx::JPEGCodec::Encode(bitmap, kImageCompressionQuality);
    return std::string(base::as_string_view(data.value()));
  }

  std::string GetWebpBytesForBitmap(const SkBitmap& bitmap) {
    std::optional<std::vector<uint8_t>> data =
        gfx::WebpCodec::Encode(bitmap, kImageCompressionQuality);
    return std::string(base::as_string_view(data.value()));
  }
};

TEST_F(LensBitmapProcessingTest, ShouldDownscaleSize) {
  gfx::Size size(10, 10);
  EXPECT_TRUE(lens::ShouldDownscaleSize(size, /*max_area=*/10,
                                        /*max_width=*/100, /*max_height=*/5));
  EXPECT_TRUE(lens::ShouldDownscaleSize(size, /*max_area=*/10, /*max_width=*/5,
                                        /*max_height=*/100));

  // Area is too great, but width and height are less than max_width and
  // max_height respectively.
  EXPECT_FALSE(lens::ShouldDownscaleSize(
      size, /*max_area=*/10, /*max_width=*/100, /*max_height=*/100));

  // Width and height are too great, but area is less than max_area.
  EXPECT_FALSE(lens::ShouldDownscaleSize(size, /*max_area=*/1000,
                                         /*max_width=*/5, /*max_height=*/5));
}

TEST_F(LensBitmapProcessingTest, DownscaleImage_TooLarge) {
  const SkBitmap bitmap = CreateOpaqueBitmap(100, 100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  SkBitmap downscaled_bitmap = DownscaleImage(bitmap, 50, 50, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(50, downscaled_bitmap.width());
  EXPECT_EQ(50, downscaled_bitmap.height());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(100 * 100, ref_counted_logs->client_logs()
                           .phase_latencies_metadata()
                           .phase(0)
                           .image_downscale_data()
                           .original_image_size());
  ASSERT_EQ(50 * 50, ref_counted_logs->client_logs()
                         .phase_latencies_metadata()
                         .phase(0)
                         .image_downscale_data()
                         .downscaled_image_size());
}

TEST_F(LensBitmapProcessingTest, DownscaleImage_TooWide) {
  const SkBitmap bitmap = CreateOpaqueBitmap(200, 100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  SkBitmap downscaled_bitmap = DownscaleImage(bitmap, 50, 50, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(50, downscaled_bitmap.width());
  EXPECT_EQ(25, downscaled_bitmap.height());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(200 * 100, ref_counted_logs->client_logs()
                           .phase_latencies_metadata()
                           .phase(0)
                           .image_downscale_data()
                           .original_image_size());
  ASSERT_EQ(50 * 25, ref_counted_logs->client_logs()
                         .phase_latencies_metadata()
                         .phase(0)
                         .image_downscale_data()
                         .downscaled_image_size());
}

TEST_F(LensBitmapProcessingTest, DownscaleImage_TooTall) {
  const SkBitmap bitmap = CreateOpaqueBitmap(100, 200);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  SkBitmap downscaled_bitmap = DownscaleImage(bitmap, 50, 50, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(25, downscaled_bitmap.width());
  EXPECT_EQ(50, downscaled_bitmap.height());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(100 * 200, ref_counted_logs->client_logs()
                           .phase_latencies_metadata()
                           .phase(0)
                           .image_downscale_data()
                           .original_image_size());
  ASSERT_EQ(25 * 50, ref_counted_logs->client_logs()
                         .phase_latencies_metadata()
                         .phase(0)
                         .image_downscale_data()
                         .downscaled_image_size());
}

TEST_F(LensBitmapProcessingTest, EncodeImage_Opaque) {
  const SkBitmap bitmap = CreateOpaqueBitmap(100, 100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();

  scoped_refptr<base::RefCountedBytes> output =
      base::MakeRefCounted<base::RefCountedBytes>();
  bool success = EncodeImageMaybeWithTransparency(
      bitmap, kImageCompressionQuality, output, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_TRUE(success);
  EXPECT_EQ(expected_output,
            std::string(base::as_string_view(output->as_vector())));
  ASSERT_EQ(359, ref_counted_logs->client_logs()
                   .phase_latencies_metadata()
                   .phase(0)
                   .image_encode_data()
                   .encoded_image_size_bytes());
}

TEST_F(LensBitmapProcessingTest, EncodeImage_Transparent) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorGREEN);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();

  scoped_refptr<base::RefCountedBytes> output =
      base::MakeRefCounted<base::RefCountedBytes>();
  bool success = EncodeImageMaybeWithTransparency(
      bitmap, kImageCompressionQuality, output, ref_counted_logs);
  std::string expected_output = GetWebpBytesForBitmap(bitmap);

  EXPECT_TRUE(success);
  EXPECT_EQ(expected_output,
            std::string(base::as_string_view(output->as_vector())));
  ASSERT_EQ(116, ref_counted_logs->client_logs()
                   .phase_latencies_metadata()
                   .phase(0)
                   .image_encode_data()
                   .encoded_image_size_bytes());
}

}  // namespace lens
