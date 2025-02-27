// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_BLOOM_FILTER_H_
#define COMPONENTS_PREVIEWS_CORE_BLOOM_FILTER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"

namespace previews {

// A vector of bytes (or 8-bit integers).
typedef std::vector<uint8_t> ByteVector;

// BloomFilter is a simple Bloom filter for keeping track of a set of strings.
// The implementation is specifically defined to be compatible with data
// and details provided from a server using the OptimizationGuide hints.proto.
class BloomFilter {
 public:
  // Constructs a Bloom filter of |num_bits| size with zero-ed data and using
  //|num_hash_functions| per entry.
  BloomFilter(uint32_t num_hash_functions, uint32_t num_bits);

  // Constructs a Bloom filter of |num_bits| size with data initialized from
  // the |filter_data| string (which is the C++ type for protobuffer |bytes|
  // type) and using |num_hash_functions| per entry.
  BloomFilter(uint32_t num_hash_functions,
              uint32_t num_bits,
              std::string filter_data);

  ~BloomFilter();

  // Returns whether this Bloom filter contains |str|.
  bool Contains(const std::string& str) const;

  // Adds |str| to this Bloom filter.
  void Add(const std::string& str);

  // Returns the bit array data of this Bloom filter as vector of bytes.
  const ByteVector& bytes() const { return bytes_; };

 private:
  // Number of bits to set for each added string.
  uint32_t num_hash_functions_;

  // Number of bits in the filter.
  uint32_t num_bits_;

  // Byte data for the filter.
  ByteVector bytes_;

  DISALLOW_COPY_AND_ASSIGN(BloomFilter);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_BLOOM_FILTER_H_
