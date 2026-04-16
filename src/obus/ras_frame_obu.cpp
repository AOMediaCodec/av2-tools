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
#include <av2_obu/obus/ras_frame_obu.h>

namespace av2_obu {

bool RASFrameOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing RAS_FRAME payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read RAS_FRAME payload");
    return false;
  }

  if (active_seq_header_) {
    BitstreamReader br(raw_payload_);
    if (!tile_group_header_.parse_lightweight(br, OBUType::RAS_FRAME, *active_seq_header_)) {
      spdlog::error("Failed to parse RAS_FRAME tile group header");
      return false;
    }
    if (tile_group_header_.frame_header.parsed) {
      spdlog::debug("RAS_FRAME: order_hint={}, refresh_flags=0x{:02x}",
                    tile_group_header_.frame_header.order_hint,
                    tile_group_header_.frame_header.refresh_frame_flags);
    }

    if (parse_mode() == ParseMode::kDeep) {
      tile_group_header_.frame_header.parse_deep(br, OBUType::RAS_FRAME, *active_seq_header_);
      tile_group_header_.parse_deep(br, OBUType::RAS_FRAME, *active_seq_header_);
    }
  } else {
    spdlog::warn("RAS_FRAME: no active sequence header — frame header not parsed");
  }

  return true;
}

json RASFrameOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "RAS_FRAME";
  if (tile_group_header_.frame_header.parsed) {
    j["tile_group"] = tile_group_header_.to_json();
  }
  return j;
}

}  // namespace av2_obu
