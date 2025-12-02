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

#include <av2_obu/av2_obu.h>

#include <random>
#include <string>
#include <utility>
#include <vector>

namespace av2_obu {

// Base class for channel simulators with OBU-aware protection
class ChannelSimulator {
public:
  struct Options {
    bool protect_config_obus;
    uint32_t seed;
    bool verbose;

    Options() : protect_config_obus(true), seed(0), verbose(false) {}
  };

  explicit ChannelSimulator(const Options& opts = {}) : options_(opts) {
    if (options_.seed == 0) {
      options_.seed = std::random_device{}();
    }
  }

  virtual ~ChannelSimulator() = default;

  // Parse bitstream and identify protected regions
  bool analyze_bitstream(const std::string& input_file);

  // Check if a byte offset is in a protected region
  bool is_protected(size_t byte_offset) const;

  // Check if byte range overlaps with any protected region
  bool overlaps_protected(size_t start, size_t end) const;

  // Get statistics
  size_t protected_obu_count() const { return protected_obu_count_; }
  size_t protected_bytes() const { return protected_bytes_; }
  const std::vector<std::pair<size_t, size_t>>& protected_ranges() const {
    return protected_ranges_;
  }

protected:
  Options options_;
  OBUParser parser_;
  std::vector<std::pair<size_t, size_t>> protected_ranges_;  // [start, end) byte ranges
  size_t protected_obu_count_ = 0;
  size_t protected_bytes_ = 0;

  // Check if OBU type should be protected
  bool is_config_obu(OBUType type) const;
};

}  // namespace av2_obu
