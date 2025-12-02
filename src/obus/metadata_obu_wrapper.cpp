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

#include <av2_obu/obus/metadata_obu_wrapper.h>

namespace av2_obu {

bool MetadataOBUWrapper::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing METADATA payload ({} bytes)", position_.payload_size);

  // Read raw payload
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read METADATA payload");
    return false;
  }

  // Parse using MetadataOBU helper class
  BitstreamReader br(raw_payload_);
  if (!metadata_.read(br)) {
    spdlog::error("Failed to parse METADATA structure");
    return false;
  }

  return true;
}

json MetadataOBUWrapper::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "METADATA";
  j["is_suffix"] = metadata_.get_is_suffix();
  j["necessity_idc"] = metadata_.get_necessity_idc();
  j["application_id"] = metadata_.get_application_id();
  j["unit_count"] = metadata_.get_unit_count();
  return j;
}

}  // namespace av2_obu
