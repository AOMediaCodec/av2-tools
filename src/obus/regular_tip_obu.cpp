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
#include <av2_obu/obus/regular_tip_obu.h>

namespace av2_obu {

bool RegularTIPOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing REGULAR_TIP payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read REGULAR_TIP payload");
    return false;
  }

  if (active_seq_header_) {
    BitstreamReader br(raw_payload_);
    if (!frame_header_.parse_lightweight(br, OBUType::REGULAR_TIP, *active_seq_header_)) {
      spdlog::error("Failed to parse REGULAR_TIP frame header");
      return false;
    }
    spdlog::debug("REGULAR_TIP: order_hint={}, refresh_flags=0x{:02x}, immediate={}",
                  frame_header_.order_hint, frame_header_.refresh_frame_flags,
                  frame_header_.immediate_output_frame);
  } else {
    spdlog::warn("REGULAR_TIP: no active sequence header — frame header not parsed");
  }

  return true;
}

json RegularTIPOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "REGULAR_TIP";
  if (frame_header_.parsed) {
    j["frame_header"] = frame_header_.to_json();
  }
  return j;
}

}  // namespace av2_obu
