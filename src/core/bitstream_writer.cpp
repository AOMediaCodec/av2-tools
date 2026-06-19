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

#include <av2_obu/core/bitstream_writer.h>

#include <algorithm>

namespace av2_obu {

void BitstreamWriter::write_bits(uint32_t value, uint32_t n) {
  while (n > 0) {
    if ((bit_pos_ & 7) == 0) buf_.push_back(0);
    uint32_t bit_in_byte = static_cast<uint32_t>(bit_pos_ & 7);
    uint32_t bits_left_in_byte = 8 - bit_in_byte;
    uint32_t bits_to_write = std::min(n, bits_left_in_byte);
    uint32_t shift = n - bits_to_write;
    uint8_t bits = static_cast<uint8_t>((value >> shift) & ((1u << bits_to_write) - 1));
    buf_.back() |= static_cast<uint8_t>(bits << (bits_left_in_byte - bits_to_write));
    bit_pos_ += bits_to_write;
    n -= bits_to_write;
  }
}

void BitstreamWriter::byte_align() {
  if ((bit_pos_ & 7) != 0) {
    write_bits(0, 8 - static_cast<uint32_t>(bit_pos_ & 7));
  }
}

}  // namespace av2_obu
