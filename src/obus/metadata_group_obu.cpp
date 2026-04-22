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
#include <av2_obu/obus/metadata_group_obu.h>

namespace av2_obu {

bool MetadataGroupOBU::parse_payload(std::ifstream& ifs) {
  if (position_.payload_size == 0) {
    spdlog::warn("Metadata Group OBU has no payload");
    return true;
  }

  spdlog::debug("Parsing metadata group payload ({} bytes)", position_.payload_size);

  try {
    BitstreamReader br(ifs, position_.payload_size);

    uint8_t byte1 = static_cast<uint8_t>(br.read_bits(8));
    metadata_is_suffix_ = (byte1 >> 7) & 0x1;
    metadata_necessity_idc_ = (byte1 >> 5) & 0x3;
    metadata_application_id_ = byte1 & 0x1F;
    spdlog::debug("  metadata_is_suffix: {}", int(metadata_is_suffix_));
    spdlog::debug("  metadata_necessity_idc: {}", int(metadata_necessity_idc_));
    spdlog::debug("  metadata_application_id: {}", int(metadata_application_id_));

    uint32_t metadata_unit_cnt_minus_1 = br.read_leb128();
    metadata_unit_cnt_ = metadata_unit_cnt_minus_1 + 1;

    spdlog::debug("  metadata_unit_cnt: {}", metadata_unit_cnt_);

    // Parse each metadata unit
    units_.clear();
    for (uint32_t i = 0; i < metadata_unit_cnt_; ++i) {
      MetadataUnit unit;

      spdlog::debug("  Parsing metadata unit {}", i);

      if (!unit.parse_group_header(br, header_.get_xlayer_id())) {
        spdlog::warn("Failed to parse metadata unit {} header", i);
        return true;
      }

      // Parse metadata unit payload if not cancelled
      if (!unit.is_cancelled()) {
        if (!unit.parse_payload(br)) {
          spdlog::warn("Failed to parse metadata unit {} payload", i);
        }
        // TODO: Parse mup_extension_bytes
      }

      units_.push_back(unit);
    }

    // Parse trailing_bits (non-extensible OBU)
    if (!parse_obu_trailing_bits(br))
      return false;

    spdlog::debug("Successfully parsed all {} metadata units", units_.size());
    return true;

  } catch (const std::exception& e) {
    spdlog::warn("Failed to parse metadata group OBU after {} units ({}), continuing with next OBU",
                 units_.size(), e.what());
    return true;  // Continue parsing rest of file
  }
}

json MetadataGroupOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["metadata_is_suffix"] = metadata_is_suffix_;
  j["metadata_necessity_idc"] = metadata_necessity_idc_;
  j["metadata_application_id"] = metadata_application_id_;
  j["metadata_unit_cnt"] = metadata_unit_cnt_;

  json units_array = json::array();
  for (const auto& unit : units_) {
    units_array.push_back(unit.to_json());
  }
  j["units"] = units_array;

  return j;
}

}  // namespace av2_obu
