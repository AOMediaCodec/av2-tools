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

#include "bit_corruptor.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <set>

namespace av2_obu {

BitCorruptor::BitCorruptor(double ber, const Options& opts)
  : ChannelSimulator(opts), ber_(ber), rng_(options_.seed) {
  if (ber_ < 0.0 || ber_ > 1.0) {
    spdlog::error("BER must be between 0.0 and 1.0");
    ber_ = 0.0;
  }
  spdlog::info("BitCorruptor initialized: BER={:.6f}, seed={}", ber_, options_.seed);
}

bool BitCorruptor::corrupt(const std::string& input, const std::string& output) {
  // Parse bitstream to identify protected regions
  if (!analyze_bitstream(input)) {
    return false;
  }

  // Read input file
  std::ifstream ifs(input, std::ios::binary);
  if (!ifs) {
    spdlog::error("Failed to open input file: {}", input);
    return false;
  }

  std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
  ifs.close();

  total_bytes_ = data.size();
  bits_corrupted_ = 0;
  bytes_affected_ = 0;

  spdlog::info("Corrupting bitstream ({} bytes, {} protected)...", total_bytes_, protected_bytes());

  // Track which bytes were affected
  std::set<size_t> affected_bytes;

  // Apply bit errors to unprotected bytes
  for (size_t byte_idx = 0; byte_idx < data.size(); ++byte_idx) {
    if (is_protected(byte_idx)) {
      continue;  // Skip protected bytes
    }

    // Check each bit in the byte
    for (int bit = 0; bit < 8; ++bit) {
      if (uniform_dist_(rng_) < ber_) {
        data[byte_idx] ^= (1 << bit);  // Flip bit
        bits_corrupted_++;
        affected_bytes.insert(byte_idx);
      }
    }
  }

  bytes_affected_ = affected_bytes.size();

  spdlog::info("Corruption complete: {} bits flipped in {} bytes", bits_corrupted_,
               bytes_affected_);

  // Write output file
  std::ofstream ofs(output, std::ios::binary);
  if (!ofs) {
    spdlog::error("Failed to open output file: {}", output);
    return false;
  }

  ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  ofs.close();

  spdlog::info("Output written to: {}", output);
  return true;
}

}  // namespace av2_obu
