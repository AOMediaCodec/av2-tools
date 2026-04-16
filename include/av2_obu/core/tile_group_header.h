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

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/frame_header_info.h>

using json = nlohmann::ordered_json;

namespace av2_obu {

class BitstreamReader;
struct AV2SequenceHeader;

// Parsed tile_group_obu() header portion.
// Used by CLK, OLK, SWITCH, RAS, LEADING_TILE_GROUP, REGULAR_TILE_GROUP.
struct TileGroupHeader {
  uint32_t is_first_tile_group = 0;
  uint32_t frame_header_present_flag = 0;
  uint32_t tile_start_and_end_present_flag = 0;
  uint32_t tg_start = 0;
  uint32_t tg_end = 0;

  // Embedded frame header (populated when frame_header_present_flag == 1)
  FrameHeaderInfo frame_header;

  // Parse the tile_group_obu header (lightweight: header + frame header only).
  // Stops before tile_group_payload().
  bool parse_lightweight(BitstreamReader& br, OBUType obu_type,
                         const AV2SequenceHeader& seq_header);

  // Parse tile range info and tile data (deep mode).
  // Must be called after parse_lightweight().
  // TODO: Implement deep tile group parsing (tile_start/end, bru_tile_active,
  //       tile_group_payload with per-tile decode) — see plan step 17/18.
  bool parse_deep(BitstreamReader& br, OBUType obu_type, const AV2SequenceHeader& seq_header);

  json to_json() const;
};

}  // namespace av2_obu
