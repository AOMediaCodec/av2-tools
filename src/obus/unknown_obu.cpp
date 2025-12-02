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

#include <av2_obu/obus/unknown_obu.h>

namespace av2_obu {

bool UnknownOBU::parse_payload(std::ifstream& ifs) {
  spdlog::warn("Unknown OBU type {}, storing raw bytes ({} bytes)", header_.get_obu_type_raw(),
               position_.payload_size);

  if (position_.payload_size > 0) {
    raw_payload_.resize(position_.payload_size);
    if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
      spdlog::error("Failed to read unknown OBU payload");
      return false;
    }
  }

  return true;
}

json UnknownOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["note"] = "Unknown OBU type - payload stored as raw bytes";
  j["raw_payload_size"] = raw_payload_.size();
  return j;
}

}  // namespace av2_obu
