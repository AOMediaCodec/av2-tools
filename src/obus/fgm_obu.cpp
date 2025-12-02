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

#include <av2_obu/obus/fgm_obu.h>

namespace av2_obu {

bool FGMOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing FGM payload ({} bytes)", position_.payload_size);

  // Read raw payload
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read FGM payload");
    return false;
  }

  // TODO: Implement FGM parsing
  spdlog::warn("FGM parsing not yet implemented");
  return true;
}

json FGMOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "FGM";
  // TODO: Add FGM-specific fields
  return j;
}

}  // namespace av2_obu
