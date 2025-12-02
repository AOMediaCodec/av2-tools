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

#include "packaging_strategy.h"

#include <spdlog/spdlog.h>

namespace av2_obu {

PackagingStrategy determine_strategy(const OBUParser::Statistics& stats,
                                     const UserOptions& user_opts) {
  PackagingStrategy strategy;

  // Timing parameters
  strategy.frame_rate = user_opts.frame_rate;
  strategy.timescale = static_cast<uint32_t>(user_opts.frame_rate * 1000);
  strategy.drop_temporal_delimiters = user_opts.drop_temporal_delimiters;

  // Determine sample entry strategy
  if (stats.sequence_headers.has_changes) {
    spdlog::info("Multiple different sequence headers detected");
    spdlog::info("Strategy: Will use multiple sample entries");
    strategy.sample_entry_mode = PackagingStrategy::SampleEntryMode::kMultiple;
  } else {
    spdlog::info("Strategy: Using single sample entry");
    strategy.sample_entry_mode = PackagingStrategy::SampleEntryMode::kSingle;
  }

  // Determine temporal unit mode
  if (user_opts.force_frame_hack) {
    // User explicitly requested the frame-based hack
    spdlog::warn("Strategy: Using frame-based HACK mode (each frame = one TU)");
    strategy.temporal_unit_mode = PackagingStrategy::TemporalUnitMode::kFrameBased;
  } else if (user_opts.force_td_mode) {
    // User explicitly requested TD mode
    if (!stats.temporal.has_temporal_delimiters) {
      spdlog::error("TD mode requested but no temporal delimiters found in bitstream!");
      spdlog::warn("Falling back to frame-based HACK mode");
      strategy.temporal_unit_mode = PackagingStrategy::TemporalUnitMode::kFrameBased;
    } else {
      spdlog::info("Strategy: Using temporal delimiter boundaries for TU detection");
      strategy.temporal_unit_mode = PackagingStrategy::TemporalUnitMode::kTDBased;
    }
  } else {
    // Auto-detect best mode
    if (stats.temporal.has_temporal_delimiters) {
      spdlog::info("Temporal delimiters detected in bitstream");
      spdlog::info("Strategy: Using temporal delimiter boundaries for TU detection");
      strategy.temporal_unit_mode = PackagingStrategy::TemporalUnitMode::kTDBased;
    } else {
      spdlog::warn("No temporal delimiters in bitstream");
      spdlog::warn("TODO: order_hint parsing not yet implemented");
      spdlog::warn("Strategy: Using frame-based HACK mode (each frame = one TU)");
      strategy.temporal_unit_mode = PackagingStrategy::TemporalUnitMode::kFrameBased;
    }
  }

  // Log additional info
  spdlog::info("Timing: {} fps (timescale: {})", strategy.frame_rate, strategy.timescale);
  spdlog::info("Drop TDs from samples: {}", strategy.drop_temporal_delimiters ? "yes" : "no");

  return strategy;
}

}  // namespace av2_obu
