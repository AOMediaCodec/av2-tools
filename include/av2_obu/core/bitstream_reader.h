/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License and the Alliance for
 * Open Media Patent License 1.0. If the BSD 3-Clause Clear License was not distributed with this
 * source code in the LICENSE file, you can obtain it at
 * aomedia.org/license/software-license/bsd-3-c-c/. If the Alliance for Open Media Patent
 * License 1.0 was not distributed with this source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace av2_obu {

// Bitstream reader for parsing bit-level syntax
class BitstreamReader {
public:
  // Construct from file stream at current position
  explicit BitstreamReader(std::ifstream& ifs, size_t byte_count);

  // Construct from raw data
  explicit BitstreamReader(const std::vector<uint8_t>& data);

  // f(n) - Unsigned n-bit number
  uint64_t read_bits(uint32_t n);

  // f(1) - Read a single bit
  uint32_t read_bit() { return static_cast<uint32_t>(read_bits(1)); }

  // uvlc() - Read variable length unsigned number
  uint64_t read_uvlc();

  // leb128() - Read unsigned integer represented by a variable number of little-endian bytes.
  uint32_t read_leb128();

  // ns(n) - Read unsigned encoded integer with maximum number of values n
  uint32_t read_ns(uint32_t n);

  // Byte align (skip to next byte boundary)
  void byte_align();

  // Get current bit position
  size_t bits_read() const { return bit_pos_; }

  // Get current byte position
  size_t bytes_read() const { return bit_pos_ / 8; }

  // Check if we have bits remaining
  bool has_bits(size_t n) const;

  // Get remaining bits
  size_t bits_remaining() const;

private:
  std::vector<uint8_t> data_;
  size_t bit_pos_ = 0;  // Current bit position
};

// Helper function: floor(log2(x))
uint32_t floor_log2(uint32_t x);

}  // namespace av2_obu
