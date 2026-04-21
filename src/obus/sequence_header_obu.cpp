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
#include <av2_obu/obus/sequence_header_obu.h>

namespace av2_obu {

bool SequenceHeaderOBU::parse_payload(std::ifstream& ifs) {
  if (position_.payload_size == 0) {
    spdlog::warn("Sequence header has no payload");
    return false;
  }

  spdlog::debug("Parsing AV2 sequence header payload ({} bytes)", position_.payload_size);

  try {
    // Create bitstream reader from the payload
    BitstreamReader br(ifs, position_.payload_size);

    // Parse using the full AV2 sequence header parser
    if (!seq_header_.parse(br)) {
      spdlog::error("Failed to parse AV2 sequence header");
      return false;
    }

    // Check for trailing_bits()
    size_t bits_remaining = br.bits_remaining();

    if (bits_remaining > 0) {
      if (!br.read_trailing_bits(bits_remaining)) {
        spdlog::warn("Sequence header has invalid trailing bits ({} bits)", bits_remaining);
      }
    }

    spdlog::debug("Successfully parsed AV2 sequence header");
    return true;

  } catch (const std::exception& e) {
    spdlog::error("Exception while parsing sequence header: {}", e.what());
    return false;
  }
}

json SequenceHeaderOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["sequence_header"] = seq_header_.to_json();
  return j;
}

}  // namespace av2_obu
