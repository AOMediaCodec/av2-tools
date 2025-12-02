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

#include <av2_obu/obus/multi_frame_header_obu.h>

namespace av2_obu {

bool MultiFrameHeaderOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing MULTI_FRAME_HEADER OBU payload ({} bytes)", position_.payload_size);

  // TBD: Multi-frame header parsing not yet implemented
  // For now, just skip the payload
  spdlog::warn("MULTI_FRAME_HEADER parsing not implemented - skipping {} bytes",
               position_.payload_size);

  return skip_payload(ifs);
}

json MultiFrameHeaderOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["payload"] = {{"note", "TBD: Multi-frame header parsing not yet implemented"}};
  return j;
}

}  // namespace av2_obu
