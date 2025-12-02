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
#include <cstdint>
#include <vector>

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/metadata_unit.h>

namespace av2_obu {

class MetadataOBU {
public:
  MetadataOBU() = default;

  bool read(BitstreamReader& br);
  void dump() const;

  // Accessors
  uint8_t get_is_suffix() const { return metadata_is_suffix; }
  uint8_t get_necessity_idc() const { return metadata_necessity_idc; }
  uint8_t get_application_id() const { return metadata_application_id; }
  uint32_t get_unit_count() const { return metadata_unit_cnt; }
  const std::vector<MetadataUnit>& get_units() const { return metadata_units; }

private:
  uint8_t metadata_is_suffix = 0;
  uint8_t metadata_necessity_idc = 0;
  uint8_t metadata_application_id = 0;
  uint32_t metadata_unit_cnt = 0;
  std::vector<MetadataUnit> metadata_units;
};

}  // namespace av2_obu
