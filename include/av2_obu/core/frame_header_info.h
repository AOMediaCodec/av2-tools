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

#include <nlohmann/json.hpp>

#include <cstdint>
#include <vector>

#include <av2_obu/core/av2_types.h>

using json = nlohmann::ordered_json;

namespace av2_obu {

class BitstreamReader;
struct AV2SequenceHeader;

// Frame header fields parsed from frame_header_info().
// Shared by CLK, OLK, RAS, SWITCH, BRIDGE, tile groups, SEF, and TIP OBUs.
struct FrameHeaderInfo {
  // Linking
  uint32_t cur_mfh_id = 0;
  uint32_t seq_header_id_in_frame_header = 0;

  // Frame type (derived from OBU type + bitstream fields)
  uint32_t FrameType = KEY_FRAME;
  bool FrameIsIntra = true;
  bool ShowExistingFrame = false;

  // SEF fields
  uint32_t frame_to_show_map_idx = 0;
  uint32_t derive_sef_order_hint = 0;
  uint32_t sef_order_hint = 0;

  // Type-specific
  uint32_t restricted_prediction_switch = 0;
  uint32_t frame_is_inter = 0;
  int32_t LongTermId = -1;
  uint32_t long_term_id_plus_1 = 0;

  uint32_t frame_size_override_flag = 0;

  // Display order
  uint32_t order_hint = 0;

  // Output flags
  uint32_t immediate_output_frame = 0;
  uint32_t implicit_output_frame = 0;

  // Primary reference
  uint32_t primary_ref_frame = PRIMARY_REF_NONE;
  uint32_t signal_primary_ref_frame = 0;
  uint32_t disable_cross_frame_cdf_init = 0;

  // Reference management
  uint32_t refresh_frame_flags = 0;
  uint32_t has_refresh_frame_flags = 0;
  uint32_t frame_to_refresh = 0;

  // Frame dimensions
  uint32_t FrameWidth = 0;
  uint32_t FrameHeight = 0;

  // References (inter frames)
  uint32_t NumTotalRefs = 0;
  uint32_t num_total_refs = 0;
  uint32_t explicitRefFrameMap = 0;
  std::vector<uint32_t> ref_frame_idx;

  // BRU
  uint32_t use_bru = 0;
  uint32_t bru_ref = 0;
  uint32_t bru_inactive = 0;

  // TIP
  uint32_t TipFrameMode = TIP_FRAME_DISABLED;

  // Whether parsing succeeded
  bool parsed = false;

  // Parse lightweight frame header fields from the bitstream.
  // Parses only what's needed for packaging: frame type, order hint,
  // refresh flags, dimensions, references. Stops before coding parameters.
  bool parse_lightweight(BitstreamReader& br, OBUType obu_type,
                         const AV2SequenceHeader& seq_header);

  // Parse remaining deep-mode fields (coding parameters, loop filter, etc.)
  // Must be called after parse_lightweight(). Only used in ParseMode::kDeep.
  // TODO: Implement deep parsing (quantization, segmentation, CDEF, loop filter,
  //       global motion, film grain config, etc.)
  bool parse_deep(BitstreamReader& br, OBUType obu_type, const AV2SequenceHeader& seq_header);

  json to_json() const;
};

}  // namespace av2_obu
