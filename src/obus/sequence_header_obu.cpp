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

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/sequence_header_obu.h>

namespace av2_obu {

bool SequenceHeaderOBU::parse_payload(std::ifstream& ifs) {
  if (position_.payload_size == 0) {
    LIB_WARN("Sequence header has no payload");
    return false;
  }

  LIB_DEBUG("Parsing AV2 sequence header payload ({} bytes)", position_.payload_size);

  // Snapshot the raw payload bytes so callers (e.g. OBUParser::get_statistics) can
  // detect SH-byte changes across the stream by direct comparison.
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read sequence header payload");
    return false;
  }

  try {
    BitstreamReader br(raw_payload_);

    // Parse using the full AV2 sequence header parser
    if (!seq_header_.parse(br)) {
      LIB_ERROR("Failed to parse AV2 sequence header");
      return false;
    }

    // Parse trailing bits (handles extensible OBU logic)
    if (!parse_obu_trailing_bits(br))
      return false;

    LIB_DEBUG("Successfully parsed AV2 sequence header");
    return true;

  } catch (const std::exception& e) {
    LIB_ERROR("Exception while parsing sequence header: {}", e.what());
    return false;
  }
}

json SequenceHeaderOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["sequence_header"] = seq_header_.to_json();
  return j;
}

}  // namespace av2_obu
