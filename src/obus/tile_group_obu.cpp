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

#include <av2_obu/obus/tile_group_obu.h>

namespace av2_obu {

bool TileGroupOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Skipping tile group payload ({} bytes)", position_.payload_size);
  // For now, skip tile data (too large and complex)
  return skip_payload(ifs);
}

json TileGroupOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["note"] = "Tile data not parsed (too large)";
  return j;
}

}  // namespace av2_obu
