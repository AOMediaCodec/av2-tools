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

#include "channel_simulator.h"

namespace av2_obu {

// Bit error injection (random bit flips)
class BitCorruptor : public ChannelSimulator {
public:
  BitCorruptor(double ber, const Options& opts = {});

  // Corrupt input file and write to output
  bool corrupt(const std::string& input, const std::string& output);

  // Statistics
  size_t bits_corrupted() const { return bits_corrupted_; }
  size_t bytes_affected() const { return bytes_affected_; }
  size_t total_bytes() const { return total_bytes_; }
  size_t unprotected_bytes() const { return total_bytes_ - protected_bytes(); }

private:
  double ber_;  // Bit error rate (0.0 to 1.0)
  std::mt19937 rng_;
  std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};

  size_t bits_corrupted_ = 0;
  size_t bytes_affected_ = 0;
  size_t total_bytes_ = 0;
};

}  // namespace av2_obu
