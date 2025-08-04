// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/cookie_craving.h"

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

// Default values for tests.
constexpr char kUrlString[] = "https://www.example.test/foo";
constexpr char kName[] = "name";
const base::Time kCreationTime = base::Time::Now();

// Helper to Create() and unwrap a CookieCraving, expecting it to be valid.
CookieCraving CreateValidCookieCraving(
    const GURL& url,
    const std::string& name,
    const std::string& attributes,
    base::Time creation_time = kCreationTime) {
  std::optional<CookieCraving> maybe_cc =
      CookieCraving::Create(url, name, attributes, creation_time);
  EXPECT_TRUE(maybe_cc);
  EXPECT_TRUE(maybe_cc->IsValid());
  return std::move(*maybe_cc);
}

// Helper to create and unwrap a CanonicalCookie.
CanonicalCookie CreateCanonicalCookie(
    const GURL& url,
    const std::string& cookie_line,
    base::Time creation_time = kCreationTime) {
  std::unique_ptr<CanonicalCookie> canonical_cookie =
      CanonicalCookie::CreateForTesting(url, cookie_line, creation_time,
                                        /*server_time=*/std::nullopt);
  EXPECT_TRUE(canonical_cookie);
  EXPECT_TRUE(canonical_cookie->IsCanonical());
  return *canonical_cookie;
}

TEST(CookieCravingTest, CreateBasic) {
  // Default cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), kName, "");
  EXPECT_EQ(cc.Name(), kName);
  EXPECT_EQ(cc.Domain(), "www.example.test");
  EXPECT_EQ(cc.Path(), "/");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_FALSE(cc.SecureAttribute());
  EXPECT_FALSE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::UNSPECIFIED);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);

  // Non-default attributes.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");
  EXPECT_EQ(cc.Name(), kName);
  EXPECT_EQ(cc.Domain(), ".example.test");
  EXPECT_EQ(cc.Path(), "/foo");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::LAX_MODE);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);

  // Normalize whitespace.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), "     name    ",
      "  Secure;HttpOnly;Path = /foo;   Domain= example.test; SameSite =Lax  ");
  EXPECT_EQ(cc.Name(), "name");
  EXPECT_EQ(cc.Domain(), ".example.test");
  EXPECT_EQ(cc.Path(), "/foo");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::LAX_MODE);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);
}

TEST(CookieCravingTest, CreateWithPrefix) {
  // Valid __Host- cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), "__Host-blah",
                                              "Secure; Path=/");
  EXPECT_EQ(cc.Domain(), "www.example.test");
  EXPECT_EQ(cc.Path(), "/");
  EXPECT_TRUE(cc.SecureAttribute());

  // Valid __Secure- cookie.
  cc = CreateValidCookieCraving(GURL(kUrlString), "__Secure-blah",
                                "Secure; Path=/foo; Domain=example.test");
  EXPECT_TRUE(cc.SecureAttribute());
}

// Test various strange inputs that should still be valid.
TEST(CookieCravingTest, CreateStrange) {
  const char* kStrangeNames[] = {
      // Empty name is permitted.
      "",
      // Leading and trailing whitespace should get trimmed.
      "   name     ",
      // Internal whitespace is allowed.
      "n a m e",
      // Trim leading and trailing whitespace while preserving internal
      // whitespace.
      "   n a m e   ",
  };
  for (const char* name : kStrangeNames) {
    CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), name, "");
    EXPECT_EQ(cc.Name(), base::TrimWhitespaceASCII(name, base::TRIM_ALL));
  }

  const char* kStrangeAttributesLines[] = {
      // Capitalization.
      "SECURE; PATH=/; SAMESITE=LAX",
      // Extra whitespace.
      "     Secure;     Path=/;     SameSite=Lax     ",
      // No whitespace.
      "Secure;Path=/;SameSite=Lax",
      // Domain attribute with leading dot.
      "Domain=.example.test",
      // Different path from the URL is allowed.
      "Path=/different",
      // Path not beginning with '/' is allowed. (It's just ignored.)
      "Path=noslash",
      // Attributes with extraneous values.
      "Secure=true; HttpOnly=yes; SameSite=absolutely",
      // Unknown attribute values.
      "SameSite=SuperStrict",
  };
  for (const char* attributes : kStrangeAttributesLines) {
    CreateValidCookieCraving(GURL(kUrlString), kName, attributes);
  }
}

// Another strange/maybe unexpected case is that Create() does not check the
// secureness of the URL against the cookie's Secure attribute. (This is
// documented in the method comment.)
TEST(CookieCravingTest, CreateSecureFromInsecureUrl) {
  CookieCraving cc =
      CreateValidCookieCraving(GURL("http://insecure.test"), kName, "Secure");
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kNonSecure);
}

// Test inputs that should result in a failure to parse the cookie line.
TEST(CookieCravingTest, CreateFailParse) {
  const struct {
    const char* name;
    const char* attributes;
  } kParseFailInputs[] = {
      // Invalid characters in name.
      {"blah\nsomething", "Secure; Path=/"},
      {"blah=something", "Secure; Path=/"},
      {"blah;something", "Secure; Path=/"},
      // Truncated lines are blocked.
      {"name", "Secure;\n Path=/"},
  };
  for (const auto& input : kParseFailInputs) {
    std::optional<CookieCraving> cc = CookieCraving::Create(
        GURL(kUrlString), input.name, input.attributes, kCreationTime);
    EXPECT_FALSE(cc);
  }
}

// Test cases where the Create() params are not valid.
TEST(CookieCravingTest, CreateFailInvalidParams) {
  // Invalid URL.
  std::optional<CookieCraving> cc =
      CookieCraving::Create(GURL(), kName, "", kCreationTime);
  EXPECT_FALSE(cc);

  // Null creation time.
  cc = CookieCraving::Create(GURL(kUrlString), kName, "", base::Time());
  EXPECT_FALSE(cc);
}

TEST(CookieCravingTest, CreateFailBadDomain) {
  // URL does not match domain.
  std::optional<CookieCraving> cc = CookieCraving::Create(
      GURL(kUrlString), kName, "Domain=other.test", kCreationTime);
  EXPECT_FALSE(cc);

  // Public suffix is not allowed to be Domain attribute.
  cc = CookieCraving::Create(GURL(kUrlString), kName, "Domain=test",
                             kCreationTime);
  EXPECT_FALSE(cc);

  // IP addresses cannot set suffixes as the Domain attribute.
  cc = CookieCraving::Create(GURL("http://1.2.3.4"), kName, "Domain=2.3.4",
                             kCreationTime);
  EXPECT_FALSE(cc);

  // Forbidden attributes even if the attribute is in the name field too.
  cc = CookieCraving::Create(GURL(kUrlString), "partitioned", "partitioned",
                             kCreationTime);
  EXPECT_FALSE(cc);

}

TEST(CookieCravingTest, CreateFailInvalidPrefix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kPrefixCookieHttp, features::kPrefixCookieHostHttp}, {});

  // __Host- with insecure URL.
  std::optional<CookieCraving> cc =
      CookieCraving::Create(GURL("http://insecure.test"), "__Host-blah",
                            "Secure; Path=/", kCreationTime);
  EXPECT_FALSE(cc);

  // __Host- with non-Secure cookie.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah", "Path=/",
                             kCreationTime);
  EXPECT_FALSE(cc);

  // __Host- with Domain attribute value.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah",
                             "Secure; Path=/; Domain=example.test",
                             kCreationTime);
  EXPECT_FALSE(cc);

  // __Host- with non-root path.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah",
                             "Secure; Path=/foo", kCreationTime);
  EXPECT_FALSE(cc);

  // __Secure- with non-Secure cookie.
  cc = CookieCraving::Create(GURL(kUrlString), "__Secure-blah", "",
                             kCreationTime);
  EXPECT_FALSE(cc);

  // Prefixes are checked case-insensitively, so these CookieCravings are also
  // invalid for not satisfying the prefix requirements.
  // Missing Secure.
  cc = CookieCraving::Create(GURL(kUrlString), "__host-blah", "Path=/",
                             kCreationTime);
  EXPECT_FALSE(cc);
  // Specifies Domain.
  cc = CookieCraving::Create(GURL(kUrlString), "__HOST-blah",
                             "Secure; Path=/; Domain=example.test",
                             kCreationTime);
  EXPECT_FALSE(cc);
  // Missing Secure.
  cc = CookieCraving::Create(GURL(kUrlString), "__SeCuRe-blah", "",
                             kCreationTime);
  EXPECT_FALSE(cc);

  cc = CookieCraving::Create(GURL(kUrlString), "__http-blah", "Path=/",
                             kCreationTime);
  EXPECT_FALSE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__http-blah", "secure;Path=/",
                             kCreationTime);
  EXPECT_FALSE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__http-blah",
                             "secure;Path=/;httpOnly", kCreationTime);
  EXPECT_TRUE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__hosthttp-blah", "Path=/",
                             kCreationTime);
  EXPECT_FALSE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__hosthttp-blah",
                             "secure;Path=/", kCreationTime);
  EXPECT_FALSE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__hosthttp-blah",
                             "secure;Path=/;httpOnly", kCreationTime);
  EXPECT_TRUE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__hosthttp-blah",
                             "secure;Path=/cookies/;httpOnly", kCreationTime);
  EXPECT_FALSE(cc);
  cc = CookieCraving::Create(GURL(kUrlString), "__hosthttp-blah",
                             "secure;Path=/;httpOnly;Domain=example.test",
                             kCreationTime);
  EXPECT_FALSE(cc);
}

// Valid cases were tested as part of the successful Create() tests above, so
// this only tests the invalid cases.
TEST(CookieCravingTest, IsNotValid) {
  const struct {
    const char* name;
    const char* domain;
    const char* path;
    bool secure;
    base::Time creation = kCreationTime;
  } kTestCases[] = {
      // Invalid name.
      {" name", "www.example.test", "/", true},
      {";", "www.example.test", "/", true},
      {"=", "www.example.test", "/", true},
      {"na\nme", "www.example.test", "/", true},
      // Empty domain.
      {"name", "", "/", true},
      // Non-canonical domain.
      {"name", "ExAmPlE.test", "/", true},
      // Empty path.
      {"name", "www.example.test", "", true},
      // Path not beginning with slash.
      {"name", "www.example.test", "noslash", true},
      // Invalid __Host- prefix.
      {"__Host-name", ".example.test", "/", true},
      {"__Host-name", "www.example.test", "/", false},
      {"__Host-name", "www.example.test", "/foo", false},
      // Invalid __Secure- prefix.
      {"__Secure-name", "www.example.test", "/", false},
      // Invalid __Host- prefix (case insensitive).
      {"__HOST-name", ".example.test", "/", true},
      {"__HoSt-name", "www.example.test", "/", false},
      {"__host-name", "www.example.test", "/foo", false},
      // Invalid __Secure- prefix (case insensitive).
      {"__secure-name", "www.example.test", "/", false},
      // Null creation date.
      {"name", "www.example.test", "/", true, base::Time()},
  };

  for (const auto& test_case : kTestCases) {
    CookieCraving cc = CookieCraving::CreateUnsafeForTesting(
        test_case.name, test_case.domain, test_case.path, test_case.creation,
        test_case.secure,
        /*httponly=*/false, CookieSameSite::LAX_MODE,
        CookieSourceScheme::kSecure, 443);
    SCOPED_TRACE(cc.DebugString());
    EXPECT_FALSE(cc.IsValid());
  }
}

TEST(CookieCravingTest, IsSatisfiedBy) {
  // Default case with no attributes.
  CanonicalCookie canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue");
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // With attributes.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString),
                            "name=somevalue; Domain=example.test; Path=/; "
                            "Secure; HttpOnly; SameSite=Lax");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name",
      "Domain=example.test; Path=/; Secure; HttpOnly; SameSite=Lax");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // The URL may differ as long as the cookie attributes match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving = CreateValidCookieCraving(
      GURL("https://subdomain.example.test"), "name", "Domain=example.test");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Creation time is not required to match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test", kCreationTime);
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Domain=example.test",
                               kCreationTime + base::Hours(1));
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Source scheme and port (and indeed source host) are not required to match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving =
      CreateValidCookieCraving(GURL("http://subdomain.example.test:8080"),
                               "name", "Domain=example.test");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, IsNotSatisfiedBy) {
  // Name does not match.
  CanonicalCookie canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "realname=somevalue");
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "fakename", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Domain does not match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=www.example.test");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Host cookie vs domain cookie.
  canonical_cookie = CreateCanonicalCookie(GURL(kUrlString), "name=somevalue");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=www.example.test");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Domain cookie vs host cookie.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=www.example.test");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Path does not match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Secure vs non-Secure.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Non-Secure vs Secure.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name", "Secure; Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // HttpOnly vs non-HttpOnly.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString),
      "name=somevalue; HttpOnly; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Non-HttpOnly vs HttpOnly.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name", "HttpOnly; Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // SameSite does not match.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; SameSite=Lax");
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "SameSite=Strict");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // SameSite vs unspecified SameSite. (Note that the SameSite attribute value
  // is compared, not the effective SameSite enforcement mode.)
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; SameSite=Lax");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, BasicCookieToFromProto) {
  // Default cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), kName, "");

  proto::CookieCraving proto = cc.ToProto();
  EXPECT_EQ(proto.name(), kName);
  EXPECT_EQ(proto.domain(), "www.example.test");
  EXPECT_EQ(proto.path(), "/");
  EXPECT_EQ(proto.creation_time(),
            kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_FALSE(proto.secure());
  EXPECT_FALSE(proto.httponly());
  EXPECT_EQ(proto.same_site(),
            proto::CookieSameSite::COOKIE_SAME_SITE_UNSPECIFIED);
  EXPECT_EQ(proto.source_scheme(), proto::CookieSourceScheme::SECURE);
  EXPECT_EQ(proto.source_port(), 443);

  std::optional<CookieCraving> restored_cc =
      CookieCraving::CreateFromProto(proto);
  ASSERT_TRUE(restored_cc.has_value());
  EXPECT_TRUE(restored_cc->IsEqualForTesting(cc));

  // Non-default attributes.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");

  proto = cc.ToProto();
  EXPECT_EQ(proto.name(), kName);
  EXPECT_EQ(proto.domain(), ".example.test");
  EXPECT_EQ(proto.path(), "/foo");
  EXPECT_EQ(proto.creation_time(),
            kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(proto.secure());
  EXPECT_TRUE(proto.httponly());
  EXPECT_EQ(proto.same_site(), proto::CookieSameSite::LAX_MODE);
  EXPECT_EQ(proto.source_scheme(), proto::CookieSourceScheme::SECURE);
  EXPECT_EQ(proto.source_port(), 443);

  restored_cc = CookieCraving::CreateFromProto(proto);
  ASSERT_TRUE(restored_cc.has_value());
  EXPECT_TRUE(restored_cc->IsEqualForTesting(cc));
}

TEST(CookieCravingTest, FailCreateFromInvalidProto) {
  // Empty proto.
  proto::CookieCraving proto;
  std::optional<CookieCraving> cc = CookieCraving::CreateFromProto(proto);
  EXPECT_FALSE(cc.has_value());

  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");
  proto = cc->ToProto();

  // Missing parameters.
  {
    proto::CookieCraving p(proto);
    p.clear_name();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_domain();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_path();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_secure();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_httponly();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_source_port();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_creation_time();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_same_site();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_source_scheme();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
}

}  // namespace net::device_bound_sessions
