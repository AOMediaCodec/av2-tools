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

// AV2 Spec Section 4.8 — Mathematical functions

namespace av2_obu {

constexpr inline int32_t Abs(int32_t x) {
  return x >= 0 ? x : -x;
}

constexpr inline int32_t Clip3(int32_t x, int32_t y, int32_t z) {
  if (z < x) return x;
  if (z > y) return y;
  return z;
}

// Clip1(x) = Clip3(0, 2^BitDepth - 1, x)
constexpr inline int32_t Clip1(int32_t x, uint32_t bitDepth) {
  return Clip3(0, (1 << bitDepth) - 1, x);
}

constexpr inline int32_t Min(int32_t x, int32_t y) {
  return x <= y ? x : y;
}

constexpr inline uint32_t Min(uint32_t x, uint32_t y) {
  return x <= y ? x : y;
}

constexpr inline int32_t Max(int32_t x, int32_t y) {
  return x >= y ? x : y;
}

constexpr inline uint32_t Max(uint32_t x, uint32_t y) {
  return x >= y ? x : y;
}

constexpr inline int32_t Round2(int32_t x, uint32_t n) {
  if (n == 0) return x;
  return (x + (1 << (n - 1))) >> n;
}

constexpr inline int32_t Round2Signed(int32_t x, uint32_t n) {
  if (x >= 0) return Round2(x, n);
  return -Round2(-x, n);
}

// FloorLog2(x): floor of base-2 logarithm. Input x must be >= 1.
constexpr inline uint32_t FloorLog2(uint32_t x) {
  uint32_t s = 0;
  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

// GetMsb(x): same as FloorLog2 but also accepts x == 0.
constexpr inline uint32_t GetMsb(uint32_t x) {
  if (x == 0) return 0;
  return FloorLog2(x);
}

// CeilLog2(x): ceiling of base-2 logarithm.
// Returns the number of bits needed to code a value in the range 0 to x-1.
constexpr inline uint32_t CeilLog2(uint32_t x) {
  if (x < 2) return 0;
  uint32_t i = 1;
  uint32_t p = 2;
  while (p < x) {
    i++;
    p = p << 1;
  }
  return i;
}

// tile_log2(blkSize, target): smallest k such that blkSize << k >= target
constexpr inline uint32_t tile_log2(uint32_t blkSize, uint32_t target) {
  uint32_t k = 0;
  while ((blkSize << k) < target) {
    k++;
  }
  return k;
}

}  // namespace av2_obu
