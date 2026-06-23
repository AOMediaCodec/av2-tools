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
#include <av2_obu/obus/regular_sef_obu.h>

namespace av2_obu {

bool RegularSEFOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing REGULAR_SEF payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read REGULAR_SEF payload");
    return false;
  }

  if (active_seq_header_) {
    BitstreamReader br(raw_payload_);
    if (!frame_header_.parse_lightweight(br, OBUType::REGULAR_SEF, *active_seq_header_)) {
      LIB_ERROR("Failed to parse REGULAR_SEF frame header");
      return false;
    }
    LIB_DEBUG("REGULAR_SEF: show_ref={}, order_hint={}, derive_sef_oh={}",
                  frame_header_.frame_to_show_map_idx, frame_header_.order_hint,
                  frame_header_.derive_sef_order_hint);
  } else {
    LIB_WARN("REGULAR_SEF: no active sequence header — frame header not parsed");
  }

  return true;
}

json RegularSEFOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "REGULAR_SEF";
  if (frame_header_.parsed) {
    j["frame_header"] = frame_header_.to_json();
  }
  return j;
}

}  // namespace av2_obu
