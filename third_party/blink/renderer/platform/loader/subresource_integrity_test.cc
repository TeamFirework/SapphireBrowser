// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/crypto_testing_platform_support.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/dtoa/utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <algorithm>

namespace blink {

static const char kBasicScript[] = "alert('test');";
static unsigned char kSha256Hash[] = {
    0x18, 0x01, 0x78, 0xf1, 0x03, 0xa8, 0xc5, 0x1b, 0xee, 0xd2, 0x06,
    0x40, 0x99, 0x08, 0xaf, 0x51, 0xd2, 0x4f, 0xc8, 0x16, 0x9c, 0xab,
    0x39, 0xc1, 0x01, 0x7c, 0x27, 0x91, 0xfa, 0x66, 0x41, 0x7e};
static unsigned char kSha384Hash[] = {
    0x9d, 0xea, 0x77, 0x5e, 0x9b, 0xe1, 0x53, 0x1a, 0x42, 0x30, 0xe5, 0x57,
    0x20, 0x53, 0xde, 0x71, 0x38, 0x40, 0xa9, 0xd6, 0x3f, 0xb9, 0x57, 0xa2,
    0x0f, 0x89, 0x17, 0x4a, 0xa5, 0xe9, 0xc7, 0x46, 0x09, 0x51, 0x65, 0x38,
    0x7d, 0x34, 0xda, 0x16, 0x07, 0x22, 0x4e, 0xe6, 0x64, 0xed, 0xf9, 0x84};
static unsigned char kSha512Hash[] = {
    0x4d, 0x79, 0x09, 0xc3, 0x5f, 0x0f, 0xaa, 0x55, 0x65, 0x11, 0x45,
    0xd7, 0x8d, 0xe5, 0xdb, 0x19, 0xeb, 0x68, 0xa7, 0x54, 0xca, 0x07,
    0x7c, 0x18, 0x40, 0x8a, 0x75, 0xfe, 0x28, 0x71, 0x08, 0xe1, 0x46,
    0x51, 0xf1, 0xbd, 0x4d, 0x83, 0x9a, 0x03, 0x53, 0x25, 0x92, 0x94,
    0xc0, 0xa9, 0x25, 0x7a, 0xc9, 0xa7, 0xaf, 0x2c, 0xef, 0x13, 0x8f,
    0x9a, 0x60, 0x1f, 0x52, 0x66, 0x67, 0xef, 0x88, 0xb4};
static const char kSha256Integrity[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityLenientSyntax[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityWithEmptyOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?";
static const char kSha256IntegrityWithOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar";
static const char kSha256IntegrityWithOptions[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar?baz=foz";
static const char kSha256IntegrityWithMimeOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?ct=application/"
    "javascript";
static const char kSha384Integrity[] =
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha512Integrity[] =
    "sha512-TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1_"
    "ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ--ItA==";
static const char kSha384IntegrityLabeledAs256[] =
    "sha256-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha256AndSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kBadSha256AndGoodSha384Integrities[] =
    "sha256-deadbeef "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kGoodSha256AndBadSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= sha384-deadbeef";
static const char kBadSha256AndBadSha384Integrities[] =
    "sha256-deadbeef sha384-deadbeef";
static const char kUnsupportedHashFunctionIntegrity[] =
    "sha1-JfLW308qMPKfb4DaHpUBEESwuPc=";

class SubresourceIntegrityTest : public testing::Test {
 public:
  SubresourceIntegrityTest()
      : sec_url("https://example.test:443"),
        insec_url("http://example.test:80") {}

 protected:
  void SetUp() override {
    context =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
  }

  SubresourceIntegrity::IntegrityFeatures Features() const {
    return RuntimeEnabledFeatures::SignatureBasedIntegrityEnabledByRuntimeFlag()
               ? SubresourceIntegrity::IntegrityFeatures::kSignatures
               : SubresourceIntegrity::IntegrityFeatures::kDefault;
  }

  void ExpectAlgorithm(const String& text,
                       IntegrityAlgorithm expected_algorithm) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    IntegrityAlgorithm algorithm;

    EXPECT_EQ(SubresourceIntegrity::kAlgorithmValid,
              SubresourceIntegrity::ParseAttributeAlgorithm(
                  position, end, Features(), algorithm));
    EXPECT_EQ(expected_algorithm, algorithm);
    EXPECT_EQ(end, position);
  }

  void ExpectAlgorithmFailure(
      const String& text,
      SubresourceIntegrity::AlgorithmParseResult expected_result) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* begin = characters.data();
    const UChar* end = characters.end();
    IntegrityAlgorithm algorithm;

    EXPECT_EQ(expected_result, SubresourceIntegrity::ParseAttributeAlgorithm(
                                   position, end, Features(), algorithm));
    EXPECT_EQ(begin, position);
  }

  void ExpectDigest(const String& text, const char* expected_digest) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    String digest;

    EXPECT_TRUE(SubresourceIntegrity::ParseDigest(position, end, digest));
    EXPECT_EQ(expected_digest, digest);
  }

  void ExpectDigestFailure(const String& text) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    String digest;

    EXPECT_FALSE(SubresourceIntegrity::ParseDigest(position, end, digest));
    EXPECT_TRUE(digest.IsEmpty());
  }

  void ExpectParse(const char* integrity_attribute,
                   const char* expected_digest,
                   IntegrityAlgorithm expected_algorithm) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  integrity_attribute, Features(), metadata_set));
    EXPECT_EQ(1u, metadata_set.size());
    if (metadata_set.size() > 0) {
      IntegrityMetadata metadata = *metadata_set.begin();
      EXPECT_EQ(expected_digest, metadata.Digest());
      EXPECT_EQ(expected_algorithm, metadata.Algorithm());
    }
  }

  void ExpectParseMultipleHashes(
      const char* integrity_attribute,
      const IntegrityMetadata expected_metadata_array[],
      size_t expected_metadata_array_size) {
    IntegrityMetadataSet expected_metadata_set;
    for (size_t i = 0; i < expected_metadata_array_size; i++) {
      expected_metadata_set.insert(expected_metadata_array[i].ToPair());
    }
    IntegrityMetadataSet metadata_set;
    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  integrity_attribute, Features(), metadata_set));
    EXPECT_TRUE(
        IntegrityMetadata::SetsEqual(expected_metadata_set, metadata_set));
  }

  void ExpectParseFailure(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseNoValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  integrity_attribute, Features(), metadata_set));
  }

  void ExpectEmptyParseResult(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  integrity_attribute, Features(), metadata_set));
    EXPECT_EQ(0u, metadata_set.size());
  }

  enum ServiceWorkerMode {
    kNoServiceWorker,
    kSWOpaqueResponse,
    kSWClearResponse
  };

  enum Expectation { kIntegritySuccess, kIntegrityFailure };

  struct TestCase {
    const KURL url;
    network::mojom::FetchRequestMode request_mode;
    network::mojom::FetchResponseType response_type;
    const Expectation expectation;
  };

  void CheckExpectedIntegrity(const char* integrity, const TestCase& test) {
    CheckExpectedIntegrity(integrity, test, test.expectation);
  }

  // Allows to overwrite the test expectation for cases that are always expected
  // to fail:
  void CheckExpectedIntegrity(const char* integrity,
                              const TestCase& test,
                              Expectation expectation) {
    IntegrityMetadataSet metadata_set;
    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  String(integrity), Features(), metadata_set));

    SubresourceIntegrity::ReportInfo report_info;
    EXPECT_EQ(expectation == kIntegritySuccess,
              SubresourceIntegrity::CheckSubresourceIntegrity(
                  metadata_set, kBasicScript, strlen(kBasicScript), test.url,
                  *CreateTestResource(test.url, test.request_mode,
                                      test.response_type),
                  report_info));
  }

  Resource* CreateTestResource(
      const KURL& url,
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchResponseType response_type) {
    Resource* resource = RawResource::CreateForTest(url, ResourceType::kRaw);

    ResourceRequest request;
    request.SetURL(url);
    request.SetFetchRequestMode(request_mode);

    ResourceResponse response(url);
    response.SetHTTPStatusCode(200);
    response.SetType(response_type);

    resource->SetResourceRequest(request);
    resource->SetResponse(response);
    return resource;
  }

  KURL sec_url;
  KURL insec_url;

  ScopedTestingPlatformSupport<CryptoTestingPlatformSupport> platform_;
  Persistent<MockFetchContext> context;
};

// Test the prioritization (i.e. selecting the "strongest" algorithm.
// This effectively tests the definition of IntegrityAlgorithm in
// IntegrityMetadata. The test is here, because SubresourceIntegrity is the
// class that relies on this working as expected.)
TEST_F(SubresourceIntegrityTest, Prioritization) {
  // Check that each algorithm is it's own "strongest".
  EXPECT_EQ(
      IntegrityAlgorithm::kSha256,
      std::max({IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha256}));
  EXPECT_EQ(
      IntegrityAlgorithm::kSha384,
      std::max({IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha384}));

  EXPECT_EQ(
      IntegrityAlgorithm::kSha512,
      std::max({IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha512}));
  EXPECT_EQ(
      IntegrityAlgorithm::kEd25519,
      std::max({IntegrityAlgorithm::kEd25519, IntegrityAlgorithm::kEd25519}));

  // Check a mix of algorithms.
  EXPECT_EQ(IntegrityAlgorithm::kSha384,
            std::max({IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384,
                      IntegrityAlgorithm::kSha256}));
  EXPECT_EQ(IntegrityAlgorithm::kSha512,
            std::max({IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512,
                      IntegrityAlgorithm::kSha256}));
  EXPECT_EQ(
      IntegrityAlgorithm::kEd25519,
      std::max({IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512,
                IntegrityAlgorithm::kEd25519, IntegrityAlgorithm::kSha512,
                IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}));
}

TEST_F(SubresourceIntegrityTest, ParseAlgorithm) {
  ExpectAlgorithm("sha256-", IntegrityAlgorithm::kSha256);
  ExpectAlgorithm("sha384-", IntegrityAlgorithm::kSha384);
  ExpectAlgorithm("sha512-", IntegrityAlgorithm::kSha512);
  ExpectAlgorithm("sha-256-", IntegrityAlgorithm::kSha256);
  ExpectAlgorithm("sha-384-", IntegrityAlgorithm::kSha384);
  ExpectAlgorithm("sha-512-", IntegrityAlgorithm::kSha512);

  {
    ScopedSignatureBasedIntegrityForTest signature_based_integrity(true);
    ExpectAlgorithm("ed25519-", IntegrityAlgorithm::kEd25519);
  }
  ScopedSignatureBasedIntegrityForTest signature_based_integrity(false);
  ExpectAlgorithmFailure("ed25519-", SubresourceIntegrity::kAlgorithmUnknown);

  ExpectAlgorithmFailure("sha1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("sha-1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobarsha256-",
                         SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobar-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("ed-25519-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("ed25518-", SubresourceIntegrity::kAlgorithmUnknown);

  ExpectAlgorithmFailure("sha256", SubresourceIntegrity::kAlgorithmUnparsable);
  ExpectAlgorithmFailure("", SubresourceIntegrity::kAlgorithmUnparsable);
}

TEST_F(SubresourceIntegrityTest, ParseDigest) {
  ExpectDigest("abcdefg", "abcdefg");
  ExpectDigest("abcdefg?", "abcdefg");
  ExpectDigest("ab+de/g", "ab+de/g");
  ExpectDigest("ab-de_g", "ab+de/g");

  ExpectDigestFailure("?");
  ExpectDigestFailure("&&&foobar&&&");
  ExpectDigestFailure("\x01\x02\x03\x04");
}

//
// End-to-end parsing tests.
//

TEST_F(SubresourceIntegrityTest, Parsing) {
  ExpectParseFailure("not_really_a_valid_anything");
  ExpectParseFailure("sha256-&&&foobar&&&");
  ExpectParseFailure("sha256-\x01\x02\x03\x04");
  ExpectParseFailure("sha256-!!! sha256-!!!");

  ExpectEmptyParseResult("foobar:///sha256-abcdefg");
  ExpectEmptyParseResult("ni://sha256-abcdefg");
  ExpectEmptyParseResult("ni:///sha256-abcdefg");
  ExpectEmptyParseResult("notsha256atall-abcdefg");

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse("sha-256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse("     sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=     ",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J-Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);

  ExpectParse(
      "sha-384-XVVXBGoYw6AJOh9J_Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_"
      "tA1v5GPr",
      "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);

  ExpectParse(
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/javascript",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?foo=bar?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?baz=foz?ct=application/"
      "xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParseMultipleHashes("", nullptr, 0);
  ExpectParseMultipleHashes("    ", nullptr, 0);

  const IntegrityMetadata valid_sha384_and_sha512[] = {
      IntegrityMetadata(
          "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
          IntegrityAlgorithm::kSha384),
      IntegrityMetadata("tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
                        "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
                        IntegrityAlgorithm::kSha512),
  };
  ExpectParseMultipleHashes(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr "
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      valid_sha384_and_sha512, arraysize(valid_sha384_and_sha512));

  const IntegrityMetadata valid_sha256_and_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        IntegrityAlgorithm::kSha256),
      IntegrityMetadata("deadbeef", IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-deadbeef",
      valid_sha256_and_sha256, arraysize(valid_sha256_and_sha256));

  const IntegrityMetadata valid_sha256_and_invalid_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-!!!!",
      valid_sha256_and_invalid_sha256,
      arraysize(valid_sha256_and_invalid_sha256));

  const IntegrityMetadata invalid_sha256_and_valid_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-!!! sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      invalid_sha256_and_valid_sha256,
      arraysize(invalid_sha256_and_valid_sha256));

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      IntegrityAlgorithm::kSha256);

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo:bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  {
    ScopedSignatureBasedIntegrityForTest signature_based_integrity(false);
    ExpectEmptyParseResult("ed25519-xxxx");
    ExpectEmptyParseResult(
        "ed25519-qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=");
  }

  ScopedSignatureBasedIntegrityForTest signature_based_integrity(true);
  ExpectParse("ed25519-xxxx", "xxxx", IntegrityAlgorithm::kEd25519);
  ExpectParse("ed25519-qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=",
              "qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=",
              IntegrityAlgorithm::kEd25519);
  ExpectParse("ed25519-qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=?foo=bar",
              "qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=",
              IntegrityAlgorithm::kEd25519);
  ExpectEmptyParseResult("ed-25519-xxx");
  ExpectEmptyParseResult(
      "ed-25519-qGFmwTxlocg707D1cX4w60iTwtfwbMLf8ITDyfko7s0=");
}

TEST_F(SubresourceIntegrityTest, ParsingBase64) {
  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);
}

// Tests that SubresourceIntegrity::CheckSubresourceIntegrity behaves correctly
// when faced with secure or insecure origins, same origin and cross origin
// requests, successful and failing CORS checks as well as when the response was
// handled by a service worker.
TEST_F(SubresourceIntegrityTest, OriginIntegrity) {
  using network::mojom::FetchRequestMode;
  using network::mojom::FetchResponseType;
  constexpr auto kOk = kIntegritySuccess;
  constexpr auto kFail = kIntegrityFailure;
  const KURL& url = sec_url;

  const TestCase cases[] = {
      // FetchResponseType::kError never arrives because it is a loading error.
      {url, FetchRequestMode::kNoCORS, FetchResponseType::kBasic, kOk},
      {url, FetchRequestMode::kNoCORS, FetchResponseType::kCORS, kOk},
      {url, FetchRequestMode::kNoCORS, FetchResponseType::kDefault, kOk},
      {url, FetchRequestMode::kNoCORS, FetchResponseType::kOpaque, kFail},
      {url, FetchRequestMode::kNoCORS, FetchResponseType::kOpaqueRedirect,
       kFail},

      // FetchResponseType::kError never arrives because it is a loading error.
      // FetchResponseType::kOpaque and FetchResponseType::kOpaqueResponse
      // never arrives: even when service worker is involved, it's handled as
      // an error.
      {url, FetchRequestMode::kCORS, FetchResponseType::kBasic, kOk},
      {url, FetchRequestMode::kCORS, FetchResponseType::kCORS, kOk},
      {url, FetchRequestMode::kCORS, FetchResponseType::kDefault, kOk},
  };

  MockWebCryptoDigestorFactory factory_sha256(
      kBasicScript, strlen(kBasicScript), kSha256Hash, sizeof(kSha256Hash));
  MockWebCryptoDigestorFactory factory_sha384(
      kBasicScript, strlen(kBasicScript), kSha384Hash, sizeof(kSha384Hash));
  MockWebCryptoDigestorFactory factory_sha512(
      kBasicScript, strlen(kBasicScript), kSha512Hash, sizeof(kSha512Hash));

  CryptoTestingPlatformSupport::SetMockCryptoScope mock_crypto_scope(
      *platform_.GetTestingPlatformSupport());

  EXPECT_CALL(mock_crypto_scope.MockCrypto(),
              CreateDigestorProxy(kWebCryptoAlgorithmIdSha256))
      .WillRepeatedly(testing::InvokeWithoutArgs(
          &factory_sha256, &MockWebCryptoDigestorFactory::Create));
  EXPECT_CALL(mock_crypto_scope.MockCrypto(),
              CreateDigestorProxy(kWebCryptoAlgorithmIdSha384))
      .WillRepeatedly(testing::InvokeWithoutArgs(
          &factory_sha384, &MockWebCryptoDigestorFactory::Create));
  EXPECT_CALL(mock_crypto_scope.MockCrypto(),
              CreateDigestorProxy(kWebCryptoAlgorithmIdSha512))
      .WillRepeatedly(testing::InvokeWithoutArgs(
          &factory_sha512, &MockWebCryptoDigestorFactory::Create));

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << ", target: " << test.url.BaseAsString()
                 << ", request mode: " << test.request_mode
                 << ", response type: " << test.response_type
                 << ", expected result: "
                 << (test.expectation == kIntegritySuccess ? "integrity"
                                                           : "failure"));

    // Verify basic sha256, sha384, and sha512 integrity checks.
    CheckExpectedIntegrity(kSha256Integrity, test);
    CheckExpectedIntegrity(kSha256IntegrityLenientSyntax, test);
    CheckExpectedIntegrity(kSha384Integrity, test);
    CheckExpectedIntegrity(kSha512Integrity, test);

    // Verify multiple hashes in an attribute.
    CheckExpectedIntegrity(kSha256AndSha384Integrities, test);
    CheckExpectedIntegrity(kBadSha256AndGoodSha384Integrities, test);

    // Unsupported hash functions should succeed.
    CheckExpectedIntegrity(kUnsupportedHashFunctionIntegrity, test);

    // Options should be ignored
    CheckExpectedIntegrity(kSha256IntegrityWithEmptyOption, test);
    CheckExpectedIntegrity(kSha256IntegrityWithOption, test);
    CheckExpectedIntegrity(kSha256IntegrityWithOptions, test);
    CheckExpectedIntegrity(kSha256IntegrityWithMimeOption, test);

    // The following tests are expected to fail in every scenario:

    // The hash label must match the hash value.
    CheckExpectedIntegrity(kSha384IntegrityLabeledAs256, test,
                           Expectation::kIntegrityFailure);

    // With multiple values, at least one must match, and it must be the
    // strongest hash algorithm.
    CheckExpectedIntegrity(kGoodSha256AndBadSha384Integrities, test,
                           Expectation::kIntegrityFailure);
    CheckExpectedIntegrity(kBadSha256AndBadSha384Integrities, test,
                           Expectation::kIntegrityFailure);
  }
}

TEST_F(SubresourceIntegrityTest, FindBestAlgorithm) {
  // Each algorithm is its own best.
  EXPECT_EQ(IntegrityAlgorithm::kSha256,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha256}})));
  EXPECT_EQ(IntegrityAlgorithm::kSha384,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha384}})));
  EXPECT_EQ(IntegrityAlgorithm::kSha512,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha512}})));
  EXPECT_EQ(IntegrityAlgorithm::kEd25519,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kEd25519}})));

  // Test combinations of multiple algorithms.
  EXPECT_EQ(IntegrityAlgorithm::kSha384,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha256},
                                      {"", IntegrityAlgorithm::kSha384}})));
  EXPECT_EQ(IntegrityAlgorithm::kSha512,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha256},
                                      {"", IntegrityAlgorithm::kSha512},
                                      {"", IntegrityAlgorithm::kSha384}})));
  EXPECT_EQ(IntegrityAlgorithm::kEd25519,
            SubresourceIntegrity::FindBestAlgorithm(
                IntegrityMetadataSet({{"", IntegrityAlgorithm::kSha256},
                                      {"", IntegrityAlgorithm::kSha512},
                                      {"", IntegrityAlgorithm::kEd25519}})));
}

TEST_F(SubresourceIntegrityTest, GetCheckFunctionForAlgorithm) {
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrityDigest ==
              SubresourceIntegrity::GetCheckFunctionForAlgorithm(
                  IntegrityAlgorithm::kSha256));
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrityDigest ==
              SubresourceIntegrity::GetCheckFunctionForAlgorithm(
                  IntegrityAlgorithm::kSha384));
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrityDigest ==
              SubresourceIntegrity::GetCheckFunctionForAlgorithm(
                  IntegrityAlgorithm::kSha512));
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegritySignature ==
              SubresourceIntegrity::GetCheckFunctionForAlgorithm(
                  IntegrityAlgorithm::kEd25519));
}

}  // namespace blink
