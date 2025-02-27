// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/bloom_filter.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "third_party/smhasher/src/MurmurHash3.h"

namespace previews {

namespace {

uint64_t MurmurHash3(const std::string& str, uint32_t seed) {
  // Uses MurmurHash3 in coordination with server as it is a fast hashing
  // function with compatible public client and private server implementations.
  // DO NOT CHANGE this hashing function without coordination and migration
  // plan with the server providing the OptimizationGuide proto.
  uint64_t output[2];
  MurmurHash3_x64_128(str.data(), str.size(), seed, &output);
  // Drop the last 64 bits.
  return output[0];
}

}  // namespace

BloomFilter::BloomFilter(uint32_t num_hash_functions, uint32_t num_bits)

    : num_hash_functions_(num_hash_functions),
      num_bits_(num_bits),
      bytes_(((num_bits + 7) / 8), 0) {}

BloomFilter::BloomFilter(uint32_t num_hash_functions,
                         uint32_t num_bits,
                         std::string filter_data)

    : num_hash_functions_(num_hash_functions),
      num_bits_(num_bits),
      bytes_(filter_data.size()) {
  CHECK_GE(filter_data.size() * 8, num_bits);
  memcpy(&bytes_[0], filter_data.data(), filter_data.size());
}

BloomFilter::~BloomFilter() {}

bool BloomFilter::Contains(const std::string& str) const {
  for (size_t i = 0; i < num_hash_functions_; ++i) {
    uint64_t n = MurmurHash3(str, i) % num_bits_;
    uint32_t byte_index = (n / 8);
    uint32_t bit_index = n % 8;
    if ((bytes_[byte_index] & (1 << bit_index)) == 0)
      return false;
  }
  return true;
}

void BloomFilter::Add(const std::string& str) {
  for (size_t i = 0; i < num_hash_functions_; ++i) {
    uint64_t n = MurmurHash3(str, i) % num_bits_;
    uint32_t byte_index = (n / 8);
    uint32_t bit_index = n % 8;
    bytes_[byte_index] |= 1 << bit_index;
  }
}

}  // namespace previews
