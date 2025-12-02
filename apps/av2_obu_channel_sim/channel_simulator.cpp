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

#include "channel_simulator.h"

#include <spdlog/spdlog.h>

namespace av2_obu {

bool ChannelSimulator::analyze_bitstream(const std::string& input_file) {
  spdlog::info("Analyzing bitstream: {}", input_file);

  if (!parser_.parse_file(input_file)) {
    spdlog::error("Failed to parse bitstream");
    return false;
  }

  protected_ranges_.clear();
  protected_obu_count_ = 0;
  protected_bytes_ = 0;

  if (options_.protect_config_obus) {
    for (const auto& obu : parser_.obus()) {
      if (is_config_obu(obu->type())) {
        // Protect entire OBU (size field + header + payload)
        size_t start = obu->position().start_pos;
        size_t end = obu->position().end_pos;
        size_t obu_total_bytes = end - start;

        protected_ranges_.push_back({start, end});
        protected_obu_count_++;
        protected_bytes_ += obu_total_bytes;

        if (options_.verbose) {
          spdlog::info("  Protecting {} at offset {} ({} bytes)", obu->type_name(), start,
                       obu_total_bytes);
        }
      }
    }

    spdlog::info("Protected {} config OBUs ({} bytes total)", protected_obu_count_,
                 protected_bytes_);
  } else {
    spdlog::info("Config OBU protection disabled");
  }

  return true;
}

bool ChannelSimulator::is_protected(size_t byte_offset) const {
  for (const auto& [start, end] : protected_ranges_) {
    if (byte_offset >= start && byte_offset < end) {
      return true;
    }
  }
  return false;
}

bool ChannelSimulator::overlaps_protected(size_t start, size_t end) const {
  for (const auto& [prot_start, prot_end] : protected_ranges_) {
    // Check if ranges overlap: !(end <= prot_start || start >= prot_end)
    if (end > prot_start && start < prot_end) {
      return true;
    }
  }
  return false;
}

bool ChannelSimulator::is_config_obu(OBUType type) const {
  return type == OBUType::SEQUENCE_HEADER || type == OBUType::LAYER_CONFIGURATION_RECORD ||
         type == OBUType::OPERATING_POINT_SET || type == OBUType::MULTI_FRAME_HEADER;
}

}  // namespace av2_obu
