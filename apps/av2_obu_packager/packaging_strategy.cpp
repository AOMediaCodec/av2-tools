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

  // Log additional info
  spdlog::info("Timing: {} fps (timescale: {})", strategy.frame_rate, strategy.timescale);
  spdlog::info("Drop TDs from samples: {}", strategy.drop_temporal_delimiters ? "yes" : "no");

  return strategy;
}

}  // namespace av2_obu
