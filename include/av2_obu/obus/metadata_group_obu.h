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

// Metadata Group OBU that contains multiple metadata units
class MetadataGroupOBU : public BaseOBU {
public:
  explicit MetadataGroupOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "OBU_METADATA_GROUP"; }

  // Accessors
  uint8_t get_is_suffix() const { return metadata_is_suffix_; }
  uint8_t get_necessity_idc() const { return metadata_necessity_idc_; }
  uint8_t get_application_id() const { return metadata_application_id_; }
  uint32_t get_unit_count() const { return metadata_unit_cnt_; }
  const std::vector<MetadataUnit>& get_units() const { return units_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint8_t metadata_is_suffix_ = 0;
  uint8_t metadata_necessity_idc_ = 0;
  uint8_t metadata_application_id_ = 0;
  uint32_t metadata_unit_cnt_ = 0;
  std::vector<MetadataUnit> units_;
};

}  // namespace av2_obu
