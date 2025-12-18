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

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

#include <av2_obu/core/bitstream_reader.h>

namespace av2_obu {

BitstreamReader::BitstreamReader(std::ifstream& ifs, size_t byte_count) {
  data_.resize(byte_count);
  if (!ifs.read(reinterpret_cast<char*>(data_.data()), byte_count)) {
    spdlog::error("Failed to read {} bytes for bitstream reader", byte_count);
    throw std::runtime_error("Failed to read bitstream data");
  }
  bit_pos_ = 0;
}

BitstreamReader::BitstreamReader(const std::vector<uint8_t>& data) : data_(data), bit_pos_(0) {}

uint64_t BitstreamReader::read_bits(uint32_t n) {
  if (n == 0)
    return 0;
  if (n > 64) {
    spdlog::error("Cannot read more than 64 bits at once (requested: {})", n);
    throw std::runtime_error("Invalid bit read size");
  }

  if (!has_bits(n)) {
    spdlog::error("Not enough bits remaining (need {}, have {})", n, bits_remaining());
    throw std::runtime_error("Bitstream underflow");
  }

  uint64_t result = 0;

  for (uint32_t i = 0; i < n; ++i) {
    size_t byte_pos = bit_pos_ / 8;
    size_t bit_in_byte = 7 - (bit_pos_ % 8);  // MSB first

    uint8_t bit = (data_[byte_pos] >> bit_in_byte) & 1;
    result = (result << 1) | bit;

    bit_pos_++;
  }

  spdlog::debug("read_bits({}) = {}", n, result);
  return result;
}

uint64_t BitstreamReader::read_uvlc() {
  uint32_t leading_zeros = 0;

  // Count leading zeros
  while (true) {
    uint32_t done = read_bit();
    if (done)
      break;
    leading_zeros++;

    if (leading_zeros >= 32) {
      spdlog::debug("read_uvlc() = {} (max value)", (1ULL << 32) - 1);
      return (1ULL << 32) - 1;
    }
  }

  if (leading_zeros == 0) {
    spdlog::debug("read_uvlc() = 0");
    return 0;
  }

  uint64_t value = read_bits(leading_zeros);
  uint64_t result = value + (1ULL << leading_zeros) - 1;

  spdlog::debug("read_uvlc() = {} (leadingZeros={}, value={})", result, leading_zeros, value);
  return result;
}

uint32_t BitstreamReader::read_leb128() {
  // LEB128 decoding: read bytes until we find one without MSB set
  uint32_t value = 0;
  uint32_t byte_count = 0;

  // Make sure we're byte-aligned for LEB128
  byte_align();

  for (uint32_t i = 0; i < 8; ++i) {  // Max 8 bytes for uint32_t
    if (!has_bits(8)) {
      spdlog::error("Not enough bytes for LEB128 decoding");
      throw std::runtime_error("Bitstream underflow during LEB128 read");
    }

    uint8_t byte = static_cast<uint8_t>(read_bits(8));
    value |= (static_cast<uint32_t>(byte & 0x7F) << (i * 7));
    ++byte_count;

    if (!(byte & 0x80)) {
      // MSB not set, this is the last byte
      break;
    }
  }

  spdlog::debug("read_leb128() = {} ({} bytes)", value, byte_count);
  return value;
}

uint32_t BitstreamReader::read_ns(uint32_t n) {
  uint32_t w = floor_log2(n) + 1;
  uint32_t m = (1 << w) - n;

  uint32_t v = static_cast<uint32_t>(read_bits(w - 1));

  if (v < m) {
    spdlog::debug("read_ns({}) = {} (early exit)", n, v);
    return v;
  }

  uint32_t extra_bit = read_bit();
  uint32_t result = (v << 1) - m + extra_bit;

  spdlog::debug("read_ns({}) = {} (w={}, m={}, v={}, extra={})", n, result, w, m, v, extra_bit);
  return result;
}

uint32_t BitstreamReader::read_rg(uint32_t n) {
  for (uint32_t q = 0; q < 32; q++) {
    uint32_t rg_bit = read_bit();
    if (rg_bit == 0) {
      uint32_t remainder = static_cast<uint32_t>(read_bits(n));
      uint32_t result = (q << n) + remainder;
      spdlog::debug("read_rg({}) = {} (q={}, remainder={})", n, result, q, remainder);
      return result;
    }
  }

  // Overflow case - spec returns -1, but we throw to match error handling pattern
  spdlog::error("Rice-Golomb overflow: no zero bit found in 32 attempts");
  throw std::runtime_error("Rice-Golomb decoding overflow");
}

uint32_t BitstreamReader::read_tu(uint32_t mx) {
  for (uint32_t idx = 0; idx < mx; idx++) {
    uint32_t tu_bit = read_bit();
    if (tu_bit == 0) {
      spdlog::debug("read_tu({}) = {} (found 0 at idx={})", mx, idx, idx);
      return idx;
    }
  }

  // Reached maximum - final 0 is omitted
  spdlog::debug("read_tu({}) = {} (max reached)", mx, mx);
  return mx;
}

void BitstreamReader::byte_align() {
  if (bit_pos_ % 8 != 0) {
    bit_pos_ = ((bit_pos_ / 8) + 1) * 8;
    spdlog::debug("byte_align() -> bit_pos={}", bit_pos_);
  }
}

bool BitstreamReader::has_bits(size_t n) const {
  return bit_pos_ + n <= data_.size() * 8;
}

size_t BitstreamReader::bits_remaining() const {
  size_t total_bits = data_.size() * 8;
  return (bit_pos_ < total_bits) ? (total_bits - bit_pos_) : 0;
}

uint32_t floor_log2(uint32_t x) {
  if (x == 0)
    return 0;

  uint32_t s = 0;
  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

}  // namespace av2_obu
