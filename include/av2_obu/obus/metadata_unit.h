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
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_reader.h>

namespace av2_obu {

// Metadata Unit that contains metadata unit header and payload
// This class is used by both MetadataOBU and MetadataGroupOBU
class MetadataUnit {
public:
  MetadataUnit() = default;

  // Parse simple metadata unit header
  bool parse_simple_header(BitstreamReader& br);

  // Parse metadata group unit header
  bool parse_group_header(BitstreamReader& br, uint32_t obu_xlayer_id);

  // Parse metadata_unit payload
  bool parse_payload(BitstreamReader& br);

  void dump() const;
  nlohmann::ordered_json to_json() const;

  // Getters
  MetadataType get_metadata_type() const { return to_metadata_type(metadata_type_); }
  uint32_t get_header_size() const { return muh_header_size_; }
  bool is_cancelled() const { return muh_cancel_flag_ != 0; }
  uint32_t get_payload_size() const { return muh_payload_size_; }
  uint32_t get_layer_idc() const { return muh_layer_idc_; }
  uint32_t get_persistence_idc() const { return muh_persistence_idc_; }
  uint32_t get_priority() const { return muh_priority_; }

private:
  // Metadata unit header fields
  uint32_t metadata_type_ = 0;
  uint32_t muh_header_size_ = 0;        // 1 for simple, actual size for group
  uint32_t muh_cancel_flag_ = 0;
  uint32_t muh_payload_size_ = 0;       // 0 for simple (not signaled)
  uint32_t muh_layer_idc_ = 0;
  uint32_t muh_persistence_idc_ = 0;
  uint32_t muh_priority_ = 0;           // 0 for simple
  uint32_t muh_reserved_zero_2bits_ = 0;
  uint32_t muh_xlayer_map_ = 0;         // 0 for simple
  std::vector<uint8_t> muh_mlayer_maps_;
  std::vector<uint8_t> muh_header_extension_bytes_;

  // Metadata payload fields
  // HDR_CLL (Content Light Level)
  uint16_t max_cll_ = 0;
  uint16_t max_fall_ = 0;

  // TIMECODE
  uint8_t counting_type_ = 0;
  uint8_t full_timestamp_flag_ = 0;
  uint8_t discontinuity_flag_ = 0;
  uint8_t cnt_dropped_flag_ = 0;
  uint16_t n_frames_ = 0;
  uint8_t seconds_value_ = 0;
  uint8_t minutes_value_ = 0;
  uint8_t hours_value_ = 0;
  uint8_t seconds_flag_ = 0;
  uint8_t minutes_flag_ = 0;
  uint8_t hours_flag_ = 0;
  uint8_t time_offset_length_ = 0;
  uint32_t time_offset_value_ = 0;

  // HASH (decoded frame hash)
  uint8_t hash_type_ = 0;
  uint8_t per_plane_ = 0;
  uint8_t has_grain_ = 0;
  uint8_t hash_reserved_ = 0;
  std::vector<std::array<uint8_t, 16>> hashes_;  // Each hash is 16 bytes (frame_hash or plane_hash)
};

}  // namespace av2_obu
