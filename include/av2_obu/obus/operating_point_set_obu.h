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

#include <av2_obu/core/base_obu.h>

#include <vector>

namespace av2_obu {

// Operating Point Set OBU (OBU_OPERATING_POINT_SET)
class OperatingPointSetOBU : public BaseOBU {
public:
  explicit OperatingPointSetOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "OPERATING_POINT_SET"; }

  // Aggregate info (global OPS, per operating point)
  struct AggregateInfo {
    uint32_t config_idc = 0;
    uint32_t aggregate_level_idx = 0;
    uint32_t max_tier_flag = 0;
    uint32_t max_interop = 0;
  };

  // Profile/tier/level info (per xlayer within an operating point)
  struct PTLInfo {
    uint32_t xlayer_id = 0;
    uint32_t seq_profile_idc = 0;
    uint32_t level_idx = 0;
    uint32_t tier_flag = 0;
    uint32_t mlayer_count = 0;
  };

  // Color info (per operating point)
  struct ColorInfo {
    uint32_t color_description_idc = 0;
    uint32_t color_primaries = 0;
    uint32_t transfer_characteristics = 0;
    uint32_t matrix_coefficients = 0;
    uint32_t full_range_flag = 0;
  };

  // Decoder model info (per operating point)
  struct DecoderModelInfo {
    uint64_t decoder_buffer_delay = 0;
    uint64_t encoder_buffer_delay = 0;
    uint32_t low_delay_mode_flag = 0;
  };

  // Mlayer info entry (per xlayer per operating point)
  struct MlayerEntry {
    uint32_t xlayer_id = 0;
    uint32_t mlayer_map = 0;
    std::vector<uint32_t> tlayer_maps;  // per active mlayer
  };

  // Per operating point
  struct OperatingPoint {
    uint32_t ops_data_size = 0;
    uint32_t ops_op_intent = 0;

    // Aggregate info (global only)
    AggregateInfo aggregate_info;

    // Color info
    bool has_color_info = false;
    ColorInfo color_info;

    // Decoder model
    uint32_t decoder_model_present = 0;
    DecoderModelInfo decoder_model_info;

    // Display delay
    uint32_t initial_display_delay_present = 0;
    uint32_t initial_display_delay_minus_1 = 0;

    // Xlayer map (global only, 31-bit bitmask)
    uint32_t ops_xlayer_map = 0;

    // Per-xlayer PTL (global only)
    std::vector<PTLInfo> xlayer_ptls;

    // Per-xlayer mlayer info
    std::vector<MlayerEntry> mlayer_entries;
  };

  uint32_t ops_reset_flag() const { return ops_reset_flag_; }
  uint32_t ops_id() const { return ops_id_; }
  uint32_t ops_cnt() const { return ops_cnt_; }
  uint32_t ops_priority() const { return ops_priority_; }
  uint32_t ops_intent() const { return ops_intent_; }
  uint32_t ops_intent_present_flag() const { return ops_intent_present_flag_; }
  uint32_t ops_ptl_present_flag() const { return ops_ptl_present_flag_; }
  uint32_t ops_color_info_present_flag() const { return ops_color_info_present_flag_; }
  uint32_t ops_mlayer_info_idc() const { return ops_mlayer_info_idc_; }
  const std::vector<OperatingPoint>& operating_points() const { return operating_points_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t ops_reset_flag_ = 0;
  uint32_t ops_id_ = 0;
  uint32_t ops_cnt_ = 0;
  uint32_t ops_priority_ = 0;
  uint32_t ops_intent_ = 0;
  uint32_t ops_intent_present_flag_ = 0;
  uint32_t ops_ptl_present_flag_ = 0;
  uint32_t ops_color_info_present_flag_ = 0;
  uint32_t ops_mlayer_info_idc_ = 0;
  uint32_t obu_extension_flag_ = 0;
  std::vector<OperatingPoint> operating_points_;
  bool parsed_ = false;
};

}  // namespace av2_obu
