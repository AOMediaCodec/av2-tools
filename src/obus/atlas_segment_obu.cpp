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

#include <av2_obu/obus/atlas_segment_obu.h>

namespace av2_obu {

bool AtlasSegmentOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing ATLAS_SEGMENT payload ({} bytes)", position_.payload_size);

  // Read raw payload
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read ATLAS_SEGMENT payload");
    return false;
  }

  // TODO: Implement ATLAS_SEGMENT parsing
  spdlog::warn("ATLAS_SEGMENT parsing not yet implemented");
  return true;
}

json AtlasSegmentOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "ATLAS_SEGMENT";
  // TODO: Add ATLAS_SEGMENT-specific fields
  return j;
}

}  // namespace av2_obu
