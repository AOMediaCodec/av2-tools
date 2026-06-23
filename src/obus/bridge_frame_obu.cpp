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
#include <av2_obu/obus/bridge_frame_obu.h>

namespace av2_obu {

bool BridgeFrameOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing BRIDGE_FRAME payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read BRIDGE_FRAME payload");
    return false;
  }

  if (active_seq_header_) {
    BitstreamReader br(raw_payload_);
    if (!frame_header_.parse_lightweight(br, OBUType::BRIDGE_FRAME, *active_seq_header_)) {
      LIB_ERROR("Failed to parse BRIDGE_FRAME frame header");
      return false;
    }
    LIB_DEBUG("BRIDGE_FRAME: order_hint={}, refresh_flags=0x{:02x}, {}x{}",
                  frame_header_.order_hint, frame_header_.refresh_frame_flags,
                  frame_header_.FrameWidth, frame_header_.FrameHeight);
  } else {
    LIB_WARN("BRIDGE_FRAME: no active sequence header — frame header not parsed");
  }

  return true;
}

json BridgeFrameOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "BRIDGE_FRAME";
  if (frame_header_.parsed) {
    j["frame_header"] = frame_header_.to_json();
  }
  return j;
}

}  // namespace av2_obu
