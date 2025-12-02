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

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/metadata_obu_impl.h>

namespace av2_obu {

bool MetadataGroupOBU::parse_payload(std::ifstream& ifs) {
  if (position_.payload_size == 0) {
    spdlog::warn("Metadata Group OBU has no payload");
    return true;
  }

  spdlog::debug("Parsing metadata group payload ({} bytes)", position_.payload_size);

  try {
    // Create bitstream reader from the payload
    BitstreamReader br(ifs, position_.payload_size);

    // Read metadata type (leb128)
    metadata_type_ = to_metadata_type(br.read_leb128());
    spdlog::debug("  Metadata type: {}", to_string(metadata_type_));

    // TODO: Parse specific metadata types
    // For now, we've read the type, rest of payload is unread

    return true;
  } catch (const std::exception& e) {
    spdlog::error("Exception while parsing metadata group: {}", e.what());
    return false;
  }
}

json MetadataGroupOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["payload"] = {{"metadata_type", static_cast<int>(metadata_type_)},
                  {"metadata_type_name", to_string(metadata_type_)}};
  return j;
}

}  // namespace av2_obu
