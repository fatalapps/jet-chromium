// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/signature.h"

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "components/dbus/utils/variant.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_utils {
namespace {

// Helper to get a signature as a std::string.
template <typename T>
std::string GetSignature() {
  return internal::GetDBusTypeSignature<T>();
}

TEST(DBusSignatureTest, GetSignature) {
  using TestTuple = std::tuple<int32_t, std::string, std::vector<uint8_t>>;
  using TestTupleWithVector = std::tuple<std::string, std::vector<int32_t>>;
  using TestTupleWithMap = std::tuple<int32_t, std::map<std::string, bool>>;
  using TestMapWithTupleKey = std::map<std::tuple<int32_t, std::string>, bool>;

  EXPECT_EQ(GetSignature<bool>(), "b");
  EXPECT_EQ(GetSignature<int16_t>(), "n");
  EXPECT_EQ(GetSignature<uint16_t>(), "q");
  EXPECT_EQ(GetSignature<int32_t>(), "i");
  EXPECT_EQ(GetSignature<uint32_t>(), "u");
  EXPECT_EQ(GetSignature<int64_t>(), "x");
  EXPECT_EQ(GetSignature<uint64_t>(), "t");
  EXPECT_EQ(GetSignature<double>(), "d");
  EXPECT_EQ(GetSignature<uint8_t>(), "y");
  EXPECT_EQ(GetSignature<std::string>(), "s");
  EXPECT_EQ(GetSignature<dbus::ObjectPath>(), "o");
  EXPECT_EQ(GetSignature<Variant>(), "v");
  EXPECT_EQ(GetSignature<base::ScopedFD>(), "h");

  EXPECT_EQ(GetSignature<std::vector<int32_t>>(), "ai");
  EXPECT_EQ(GetSignature<std::vector<std::string>>(), "as");
  EXPECT_EQ(GetSignature<std::vector<bool>>(), "ab");
  EXPECT_EQ(GetSignature<std::vector<std::vector<int32_t>>>(), "aai");

  EXPECT_EQ((GetSignature<std::map<std::string, int32_t>>()), "a{si}");

  EXPECT_EQ(GetSignature<TestTuple>(), "(isay)");
  EXPECT_EQ(GetSignature<TestTupleWithVector>(), "(sai)");
  EXPECT_EQ(GetSignature<TestTupleWithMap>(), "(ia{sb})");
  EXPECT_EQ(GetSignature<TestMapWithTupleKey>(), "a{(is)b}");

  EXPECT_EQ((GetSignature<std::vector<TestTuple>>()), "a(isay)");
  EXPECT_EQ((GetSignature<std::map<std::string, TestTuple>>()), "a{s(isay)}");
  EXPECT_EQ((GetSignature<std::map<TestTuple, int32_t>>()), "a{(isay)i}");
}

}  // namespace
}  // namespace dbus_utils
