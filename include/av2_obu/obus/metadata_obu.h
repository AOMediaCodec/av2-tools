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

#pragma once

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/base_obu.h>
#include <av2_obu/obus/metadata_unit.h>

namespace av2_obu {

// Metadata OBU (short format) that contains single metadata unit
class MetadataOBU : public BaseOBU {
public:
  explicit MetadataOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "OBU_METADATA_SHORT"; }

  // Accessors
  uint8_t get_is_suffix() const { return metadata_is_suffix_; }
  const MetadataUnit& get_metadata_unit() const { return metadata_unit_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  // OBU-level field
  uint8_t metadata_is_suffix_ = 0;

  // Metadata unit (contains header + payload)
  MetadataUnit metadata_unit_;

  // metadata_application_idc = 0 (implicit per spec)
  // metadata_necessity_idc = 0 (implicit per spec)
};

}  // namespace av2_obu
