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
#include <optional>
#include <string>
#include <vector>

#include "colr_info.h"
#include <av2_obu/core/obu_parser.h>

namespace av2_obu {

// Big-endian fourcc; matches ISOBMFF on-wire byte order.
inline constexpr uint32_t fourcc(char a, char b, char c, char d) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(d));
}

struct MuxStrategy {
  enum class SampleEntryMode { kSingle, kMultiple };
  enum class TimingSource { kCli, kCiTimingInfo, kDefault };

  bool drop_temporal_delimiters = true;
  SampleEntryMode sample_entry_mode = SampleEntryMode::kSingle;

  double frame_rate = 30.0;
  uint32_t timescale = 30000;
  uint32_t default_sample_duration = 1000;
  TimingSource timing_source = TimingSource::kDefault;

  // True iff any SH has monotonic_output_order_flag == 0; gates ctts emission.
  bool any_non_monotonic = false;

  uint32_t samples_per_chunk = 30;

  uint32_t start_tu = 0;
  uint32_t num_samples = 0;  // 0 = all from start_tu

  uint32_t major_brand = fourcc('a', 'v', '0', '2');
  std::vector<uint32_t> compatible_brands = {
    fourcc('a', 'v', '0', '2'),
    fourcc('i', 's', 'o', '6'),
  };

  // If set, overrides any CI/LCR/OPS color metadata from the bitstream.
  std::optional<ColrInfo> colr_override;

  // Hooks for fragmented MP4 / DASH; not yet wired through.
  struct Fragmentation {
    bool enabled = false;
    uint32_t fragment_duration_ms = 0;
    bool emit_sidx = false;
    uint32_t subsegs_per_sidx = 0;
    bool daisy_chain_sidx = false;
  } fragmentation;

  // Hooks for CENC; not yet wired through.
  struct Encryption {
    bool enabled = false;
    uint32_t scheme = 0;
    std::vector<uint8_t> kid;
    uint32_t pattern_crypt_block_count = 0;
    uint32_t pattern_skip_block_count = 0;
  } encryption;
};

struct UserOptions {
  double frame_rate = 30.0;
  bool frame_rate_explicit = false;
  bool drop_temporal_delimiters = true;
  uint32_t samples_per_chunk = 30;
  uint32_t start_tu = 0;
  uint32_t num_samples = 0;
  std::string colr_override;
};

// Accepts either a CICP profile name (see colr_info.h::supported_colr_profile_names())
// or a raw 'cp:tc:mc:fr' tuple per ITU-T H.273.
std::optional<ColrInfo> parse_colr_override(const std::string& spec);

MuxStrategy determine_strategy(const OBUParser::Statistics& stats,
                               const UserOptions& user_opts);

}  // namespace av2_obu
