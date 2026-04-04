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

#include <av2_obu/core/obu_parser.h>

namespace av2_obu {

// Packaging strategy determined from bitstream analysis and user options
struct PackagingStrategy {
  enum class SampleEntryMode {
    kSingle,   // One sample entry for whole file
    kMultiple  // New entry when sequence header changes
  };
  SampleEntryMode sample_entry_mode = SampleEntryMode::kSingle;

  bool drop_temporal_delimiters = true;

  // Timing
  double frame_rate = 30.0;
  uint32_t timescale = 30000;  // frame_rate * 1000
};

// User options from CLI
struct UserOptions {
  double frame_rate = 30.0;
  bool drop_temporal_delimiters = true;
};

// Determines packaging strategy from bitstream statistics and user options
PackagingStrategy determine_strategy(const OBUParser::Statistics& stats,
                                     const UserOptions& user_opts);

}  // namespace av2_obu
