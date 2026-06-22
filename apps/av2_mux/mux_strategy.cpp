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

#include "mux_strategy.h"
#include "mux_log.h"

#include <sstream>

#include <spdlog/spdlog.h>

namespace av2_obu {

std::optional<ColrInfo> parse_colr_override(const std::string& spec) {
  if (spec.empty()) return std::nullopt;
  if (auto profile = colr_profile_by_name(spec)) return profile;

  std::stringstream ss(spec);
  std::string tok;
  std::vector<uint32_t> v;
  while (std::getline(ss, tok, ':')) {
    try {
      v.push_back(static_cast<uint32_t>(std::stoul(tok)));
    } catch (...) {
      MUX_ERROR("--colr-override: '{}' is neither a known profile [{}] nor a numeric tuple",
                spec, supported_colr_profile_names());
      return std::nullopt;
    }
  }
  if (v.size() != 4) {
    MUX_ERROR("--colr-override: expected 4 CICP values 'cp:tc:mc:fr' or a profile name [{}]; "
              "got '{}'",
              supported_colr_profile_names(), spec);
    return std::nullopt;
  }
  return ColrInfo{v[0], v[1], v[2], v[3]};
}

MuxStrategy determine_strategy(const OBUParser::Statistics& stats,
                               const UserOptions& user_opts) {
  MuxStrategy s;

  s.drop_temporal_delimiters = user_opts.drop_temporal_delimiters;
  s.samples_per_chunk = user_opts.samples_per_chunk;
  s.start_tu = user_opts.start_tu;
  s.num_samples = user_opts.num_samples;
  s.colr_override = parse_colr_override(user_opts.colr_override);

  // Timing precedence: --fps > CI timing_info > default 30 fps.
  const auto& ci = stats.content_interpretation;
  const bool ci_usable = ci.has_timing_info && ci.timing_info.time_scale > 0 &&
                         ci.timing_info.num_units_in_display_tick > 0;
  if (user_opts.frame_rate_explicit) {
    s.frame_rate = user_opts.frame_rate;
    s.timescale = static_cast<uint32_t>(user_opts.frame_rate * 1000);
    s.default_sample_duration = 1000;
    s.timing_source = MuxStrategy::TimingSource::kCli;
  } else if (ci_usable) {
    const auto& t = ci.timing_info;
    s.timescale = t.time_scale;
    if (t.equal_picture_interval) {
      uint64_t dur = static_cast<uint64_t>(t.num_ticks_per_picture_minus_1 + 1) *
                     t.num_units_in_display_tick;
      s.default_sample_duration =
        dur > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(dur);
    } else {
      // Variable per-frame timing not yet supported; fall back to display-tick.
      s.default_sample_duration = t.num_units_in_display_tick;
      MUX_WARN("CI timing_info has equal_picture_interval=0; using num_units_in_display_tick "
               "as constant duration");
    }
    s.frame_rate = static_cast<double>(t.time_scale) /
                   static_cast<double>(s.default_sample_duration);
    s.timing_source = MuxStrategy::TimingSource::kCiTimingInfo;
  } else {
    s.frame_rate = 30.0;
    s.timescale = 30000;
    s.default_sample_duration = 1000;
    s.timing_source = MuxStrategy::TimingSource::kDefault;
  }

  s.sample_entry_mode = stats.sequence_headers.has_changes
                          ? MuxStrategy::SampleEntryMode::kMultiple
                          : MuxStrategy::SampleEntryMode::kSingle;

  const char* src = "default";
  switch (s.timing_source) {
    case MuxStrategy::TimingSource::kCli: src = "CLI"; break;
    case MuxStrategy::TimingSource::kCiTimingInfo: src = "CI timing_info"; break;
    case MuxStrategy::TimingSource::kDefault: src = "default"; break;
  }
  MUX_DEBUG("Timing: {:.3f} fps (timescale={}, duration={}, source={})", s.frame_rate,
            s.timescale, s.default_sample_duration, src);
  MUX_DEBUG("Strategy: sample_entry_mode={}, drop_TDs={}, samples_per_chunk={}",
            s.sample_entry_mode == MuxStrategy::SampleEntryMode::kMultiple ? "kMultiple"
                                                                           : "kSingle",
            s.drop_temporal_delimiters ? "yes" : "no", s.samples_per_chunk);

  return s;
}

}  // namespace av2_obu
