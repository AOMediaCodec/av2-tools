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
#include <av2_obu/obus/metadata_obu.h>

namespace av2_obu {

bool MetadataOBU::parse_payload(std::ifstream& ifs) {
  if (position_.payload_size == 0) {
    spdlog::warn("Metadata OBU has no payload");
    return true;
  }

  spdlog::debug("Parsing metadata OBU payload ({} bytes)", position_.payload_size);

  try {
    BitstreamReader br(ifs, position_.payload_size);

    // Parse OBU-level field
    metadata_is_suffix_ = static_cast<uint8_t>(br.read_bits(1));
    spdlog::debug("  metadata_is_suffix: {}", int(metadata_is_suffix_));

    // Parse short metadata unit header
    if (!metadata_unit_.parse_simple_header(br)) {
      spdlog::warn("Failed to parse metadata unit header");
      return true;
    }

    // Parse metadata unit payload if not cancelled
    if (!metadata_unit_.is_cancelled()) {
      if (!metadata_unit_.parse_payload(br)) {
        spdlog::warn("Failed to parse metadata unit payload");
      }
    }

    return true;
  } catch (const std::exception& e) {
    spdlog::warn("Failed to parse metadata OBU ({}), continuing with next OBU", e.what());
    return true;
  }
}

json MetadataOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["payload"] = {{"metadata_is_suffix", metadata_is_suffix_}};

  // Add metadata unit
  j["payload"]["metadata_unit"] = metadata_unit_.to_json();

  return j;
}

}  // namespace av2_obu
