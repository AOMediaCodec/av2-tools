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

#include <av2_obu/obus/metadata_obu.h>

namespace av2_obu {

bool MetadataOBU::read(BitstreamReader& br) {
  uint8_t first = static_cast<uint8_t>(br.read_bits(8));

  metadata_is_suffix = (first >> 7) & 0x1;
  metadata_necessity_idc = (first >> 5) & 0x3;
  metadata_application_id = first & 0x1F;

  metadata_unit_cnt = br.read_leb128();

  metadata_units.clear();
  for (uint32_t i = 0; i < metadata_unit_cnt; ++i) {
    MetadataUnit unit;
    if (!unit.read(br))
      return false;
    metadata_units.push_back(unit);
  }
  return true;
}

void MetadataOBU::dump() const {
  spdlog::debug("       MetadataOBU {{");
  spdlog::debug("         is_suffix={}, necessity_idc={}, application_id={}, unit_cnt={}",
                int(metadata_is_suffix), int(metadata_necessity_idc), int(metadata_application_id),
                metadata_unit_cnt);
  for (size_t i = 0; i < metadata_units.size(); ++i) {
    spdlog::debug("         unit[{}]:", i);
    metadata_units[i].dump();
  }
  spdlog::debug("       }}");
}

}  // namespace av2_obu
