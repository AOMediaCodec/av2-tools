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
#include <av2_obu/core/logging.h>

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/olk_obu.h>

namespace av2_obu {

bool OLKOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing OLK payload ({} bytes)", position_.payload_size);

  // Always read raw payload for potential passthrough
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read OLK payload");
    return false;
  }

  // Parse tile group header (which includes frame header) from raw payload
  if (active_seq_header_) {
    BitstreamReader br(raw_payload_);
    if (!tile_group_header_.parse_lightweight(br, OBUType::OLK, *active_seq_header_)) {
      LIB_ERROR("Failed to parse OLK tile group header");
      return false;
    }
    LIB_DEBUG("OLK: order_hint={}, refresh_flags=0x{:02x}, {}x{}",
                  tile_group_header_.frame_header.order_hint,
                  tile_group_header_.frame_header.refresh_frame_flags,
                  tile_group_header_.frame_header.FrameWidth,
                  tile_group_header_.frame_header.FrameHeight);

    // Deep mode: continue parsing coding parameters and tile data
    if (parse_mode() == ParseMode::kDeep) {
      tile_group_header_.frame_header.parse_deep(br, OBUType::OLK, *active_seq_header_);
      tile_group_header_.parse_deep(br, OBUType::OLK, *active_seq_header_);
    }
  } else {
    LIB_WARN("OLK: no active sequence header — frame header not parsed");
  }

  return true;
}

json OLKOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "OLK";
  if (tile_group_header_.frame_header.parsed) {
    j["tile_group"] = tile_group_header_.to_json();
  }
  return j;
}

}  // namespace av2_obu
