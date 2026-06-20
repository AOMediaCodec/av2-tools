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
  PackagingStrategy s;

  // Pass-through user opts
  s.drop_temporal_delimiters = user_opts.drop_temporal_delimiters;
  s.samples_per_chunk = user_opts.samples_per_chunk;
  s.start_tu = user_opts.start_tu;
  s.num_samples = user_opts.num_samples;

  // Timing — CLI takes precedence today. CI timing_info() will land in Step 8.
  s.frame_rate = user_opts.frame_rate;
  s.timescale = static_cast<uint32_t>(user_opts.frame_rate * 1000);
  s.default_sample_duration = 1000;
  s.timing_source = PackagingStrategy::TimingSource::kCli;

  // Sample entry mode: any SH change → multiple entries.
  s.sample_entry_mode = stats.sequence_headers.has_changes
                          ? PackagingStrategy::SampleEntryMode::kMultiple
                          : PackagingStrategy::SampleEntryMode::kSingle;

  // any_non_monotonic and ctts wiring land in Step 5.3 — needs SH access
  // beyond what Statistics currently exposes.

  spdlog::debug("Strategy: sample_entry_mode={}",
                s.sample_entry_mode == PackagingStrategy::SampleEntryMode::kMultiple ? "kMultiple"
                                                                                     : "kSingle");
  spdlog::debug("Strategy: timescale={}, duration={}, source=CLI", s.timescale,
                s.default_sample_duration);
  spdlog::debug("Strategy: drop_TDs={}, samples_per_chunk={}",
                s.drop_temporal_delimiters ? "yes" : "no", s.samples_per_chunk);

  return s;
}

}  // namespace av2_obu
