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
#include <string>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_reader.h>

namespace av2_obu {

class MetadataUnit {
public:
  MetadataUnit();

  bool read(BitstreamReader& br);
  void dump() const;

  // Getters
  MetadataType get_metadata_type() const;
  uint32_t get_payload_size() const;
  bool is_cancelled() const;

private:
  uint32_t muh_metadata_type;
  uint32_t muh_header_size;
  uint32_t muh_cancel_flag;
  uint32_t muh_payload_size;
  uint32_t muh_layer_idc;
  uint32_t muh_persistence_idc;
  uint32_t muh_priority;
};

}  // namespace av2_obu
