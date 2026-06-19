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
#include <vector>

namespace av2_obu {

// MSB-first bit writer matching the ISOBMFF / AV2 f(N) descriptor convention.
// Writes are appended to an internal byte buffer.
class BitstreamWriter {
public:
  // f(n) — write the low n bits of value (0 < n <= 32).
  void write_bits(uint32_t value, uint32_t n);

  // f(1)
  void write_bit(uint32_t value) { write_bits(value, 1); }

  // Pad with zero bits to the next byte boundary. No-op if already aligned.
  void byte_align();

  // Number of bits / bytes written so far.
  size_t bit_count() const { return bit_pos_; }
  size_t byte_count() const { return buf_.size(); }
  bool byte_aligned() const { return (bit_pos_ & 7) == 0; }

  // Borrow / take the underlying buffer.
  const std::vector<uint8_t>& bytes() const { return buf_; }
  std::vector<uint8_t> take() { return std::move(buf_); }

private:
  std::vector<uint8_t> buf_;
  size_t bit_pos_ = 0;
};

}  // namespace av2_obu
