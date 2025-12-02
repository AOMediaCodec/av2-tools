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

#include <av2_obu/obus/qm_obu.h>

namespace av2_obu {

bool QMOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing QM payload ({} bytes)", position_.payload_size);

  // Read raw payload
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read QM payload");
    return false;
  }

  // TODO: Implement QM parsing
  spdlog::warn("QM parsing not yet implemented");
  return true;
}

json QMOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "QM";
  // TODO: Add QM-specific fields
  return j;
}

}  // namespace av2_obu
