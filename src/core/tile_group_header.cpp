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

#include <spdlog/spdlog.h>

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/core/tile_group_header.h>

namespace av2_obu {

bool TileGroupHeader::parse_lightweight(BitstreamReader& br, OBUType obu_type,
                                        const AV2SequenceHeader& sh) {
  // tile_group_obu() syntax
  is_first_tile_group = br.read_bit();

  if (is_first_tile_group) {
    frame_header_present_flag = 1;
  } else {
    frame_header_present_flag = br.read_bit();
  }

  // Parse frame header if present
  if (frame_header_present_flag) {
    if (!frame_header.parse_lightweight(br, obu_type, sh)) {
      spdlog::error("Failed to parse frame header in tile group");
      return false;
    }
  }

  // For lightweight mode, we don't parse tile range info because
  // it depends on tile_info() which is deep-mode parsing.
  // tg_start and tg_end remain at defaults (0).

  return true;
}

bool TileGroupHeader::parse_deep(BitstreamReader& /*br*/, OBUType /*obu_type*/,
                                 const AV2SequenceHeader& /*seq_header*/) {
  // TODO: Parse tile range (tg_start, tg_end) — requires tile_info() from
  //       deep frame header parse to know TileCols, TileRows, etc.
  // TODO: Parse bru_tile_active flags
  // TODO: Parse tile_group_payload (per-tile arithmetic-coded block data)
  spdlog::debug("Deep tile group parsing not yet implemented");
  return true;
}

json TileGroupHeader::to_json() const {
  json j;
  j["is_first_tile_group"] = is_first_tile_group;
  j["frame_header_present_flag"] = frame_header_present_flag;
  if (frame_header_present_flag && frame_header.parsed) {
    j["frame_header"] = frame_header.to_json();
  }
  return j;
}

}  // namespace av2_obu
