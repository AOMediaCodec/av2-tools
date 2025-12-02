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

#include <av2_obu/obus/metadata_unit.h>

namespace av2_obu {

MetadataUnit::MetadataUnit()
  : muh_metadata_type(0),
    muh_header_size(0),
    muh_cancel_flag(0),
    muh_payload_size(0),
    muh_layer_idc(0),
    muh_persistence_idc(0),
    muh_priority(0) {}

bool MetadataUnit::read(BitstreamReader& br) {
  muh_metadata_type = br.read_leb128();

  uint8_t byte1 = static_cast<uint8_t>(br.read_bits(8));

  muh_header_size = (byte1 & 0xFE) >> 1;
  muh_cancel_flag = byte1 & 0x1;

  if (!muh_cancel_flag) {
    muh_payload_size = br.read_leb128();

    uint8_t byte2 = static_cast<uint8_t>(br.read_bits(8));
    uint8_t byte3 = static_cast<uint8_t>(br.read_bits(8));

    muh_layer_idc = (byte2 & 0xE0) >> 5;
    muh_persistence_idc = (byte2 & 0x1C) >> 2;
    muh_priority = (byte2 & 0x3) | ((byte3 & 0xFC) >> 2);
  }

  return true;
}

void MetadataUnit::dump() const {
  spdlog::debug("       MetadataUnit {{");
  spdlog::debug("           muh_metadata_type: {} ({})", muh_metadata_type,
                to_string(get_metadata_type()));
  spdlog::debug("           muh_header_size: {}", muh_header_size);
  spdlog::debug("           muh_cancel_flag: {}", muh_cancel_flag);

  if (!muh_cancel_flag) {
    spdlog::debug("           muh_payload_size: {}", muh_payload_size);
    spdlog::debug("           muh_layer_idc: {}", muh_layer_idc);
    spdlog::debug("           muh_persistence_idc: {}", muh_persistence_idc);
    spdlog::debug("           muh_priority: {}", muh_priority);
  }

  spdlog::debug("       }}");
}

MetadataType MetadataUnit::get_metadata_type() const {
  return to_metadata_type(muh_metadata_type);
}

uint32_t MetadataUnit::get_payload_size() const {
  return muh_payload_size;
}

bool MetadataUnit::is_cancelled() const {
  return muh_cancel_flag != 0;
}

}  // namespace av2_obu
