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
#include <vector>

#include <av2_obu/core/obu_parser.h>

namespace av2_obu {

// FOURCC packed big-endian (matches ISOBMFF on-wire byte order: a is MSB).
inline constexpr uint32_t fourcc(char a, char b, char c, char d) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |
         static_cast<uint32_t>(static_cast<uint8_t>(d));
}

// Packaging strategy: derived from bitstream analysis and user options.
struct PackagingStrategy {
  enum class SampleEntryMode {
    kSingle,   // one sample entry for the whole file
    kMultiple  // new entry on each new SH (Step 6)
  };

  enum class TimingSource {
    kCli,           // --fps from CLI (highest precedence)
    kCiTimingInfo,  // CI OBU timing_info() (Step 8)
    kDefault        // 30 fps fallback
  };

  // === Pass-through / user-driven ===
  bool drop_temporal_delimiters = true;

  // === Sample entry (Step 6) ===
  SampleEntryMode sample_entry_mode = SampleEntryMode::kSingle;

  // === Timing ===
  double frame_rate = 30.0;
  uint32_t timescale = 30000;
  uint32_t default_sample_duration = 1000;
  TimingSource timing_source = TimingSource::kDefault;

  // === ctts (Step 5.3) ===
  // Set true if any SH in the stream has monotonic_output_order_flag == 0.
  // Triggers MP4UseSignedCompositionTimeOffsets in Mp4Writer.
  bool any_non_monotonic = false;

  // === Chunking (Step 5.4) ===
  // Auto-flush trigger inside Mp4Writer::add_sample. Default ≈ fps.
  uint32_t samples_per_chunk = 30;

  // === TU range selection (testing / trimming) ===
  uint32_t start_tu = 0;        // 0-based TU index to start from
  uint32_t num_samples = 0;     // 0 = all (from start_tu to end)

  // === File-type / brands (Step 9) ===
  uint32_t major_brand = fourcc('a', 'v', '0', '2');
  std::vector<uint32_t> compatible_brands = {
    fourcc('a', 'v', '0', '2'),
    fourcc('i', 's', 'o', '6'),
  };

  // === Fragmentation hooks (deferred — v2 / future) ===
  // Spec §CMAF will pin down what to put here. For now, defaults disable
  // fragmentation entirely; Mp4Writer ignores these fields.
  struct Fragmentation {
    bool enabled = false;
    uint32_t fragment_duration_ms = 0;     // target fragment duration; 0 = unset
    bool emit_sidx = false;                // produce SegmentIndexBox for DASH
    uint32_t subsegs_per_sidx = 0;         // sidx layout
    bool daisy_chain_sidx = false;         // chained vs root sidx
  } fragmentation;

  // === Encryption / CENC hooks (deferred — v2 / future) ===
  // Spec §CommonEncryption will pin down what to put here. For now, defaults
  // disable encryption entirely; Mp4Writer ignores these fields.
  struct Encryption {
    bool enabled = false;
    uint32_t scheme = 0;                       // 'cenc' / 'cbc1' / 'cens' / 'cbcs' / 'sve1'
    std::vector<uint8_t> kid;                  // 16-byte Key ID
    uint32_t pattern_crypt_block_count = 0;    // cbcs / cens pattern
    uint32_t pattern_skip_block_count = 0;
    // Key delivery, per-sample IVs, subsample mapping, PSSH boxes — out of
    // strategy scope; would be supplied by an EncryptionStrategy / KeySource
    // when CENC support lands.
  } encryption;
};

// User options from CLI (raw input, no analysis applied).
struct UserOptions {
  double frame_rate = 30.0;
  bool drop_temporal_delimiters = true;
  uint32_t samples_per_chunk = 30;
  uint32_t start_tu = 0;
  uint32_t num_samples = 0;
};

// Apply user options + analysis to produce a strategy.
PackagingStrategy determine_strategy(const OBUParser::Statistics& stats,
                                     const UserOptions& user_opts);

}  // namespace av2_obu
