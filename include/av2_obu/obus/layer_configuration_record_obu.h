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

// Layer Configuration Record OBU (OBU_LAYER_CONFIGURATION_RECORD)
// Defines multi-layer/multistream structure.
// Global LCR (xlayer_id == 31): describes all xlayers.
// Local LCR (xlayer_id < 31): describes a single xlayer.
class LayerConfigurationRecordOBU : public BaseOBU {
public:
  explicit LayerConfigurationRecordOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "LAYER_CONFIGURATION_RECORD"; }

  // Per-xlayer profile/tier/level info
  struct XLayerPTL {
    uint32_t xlayer_id = 0;
    uint32_t seq_profile_idc = 0;
    uint32_t max_level_idx = 0;
    uint32_t tier_flag = 0;
    uint32_t max_mlayer_count = 0;
  };

  // Aggregate info (global LCR)
  struct AggregateInfo {
    uint32_t config_idc = 0;
    uint32_t aggregate_level_idx = 0;
    uint32_t max_tier_flag = 0;
    uint32_t max_interop = 0;
  };

  // Rep info (resolution/format for an xlayer)
  struct RepInfo {
    uint32_t max_pic_width = 0;
    uint32_t max_pic_height = 0;
    bool format_info_present = false;
    uint32_t bit_depth_idc = 0;
    uint32_t chroma_format_idc = 0;
    bool cropping_window_present = false;
    uint32_t crop_left = 0;
    uint32_t crop_right = 0;
    uint32_t crop_top = 0;
    uint32_t crop_bottom = 0;
  };

  // Color info for an xlayer
  struct ColorInfo {
    uint32_t color_description_idc = 0;
    uint32_t color_primaries = 0;
    uint32_t transfer_characteristics = 0;
    uint32_t matrix_coefficients = 0;
    uint32_t full_range_flag = 0;
  };

  // Embedded layer info for a single mlayer within an xlayer
  struct EmbeddedLayerEntry {
    uint32_t mlayer_id = 0;
    uint32_t tlayer_map = 0;
    uint32_t atlas_segment_id = 0;
    uint32_t priority_order = 0;
    uint32_t rendering_method = 0;
    uint32_t layer_type = 0;
    uint32_t auxiliary_type = 0;
    uint32_t view_type = 0;
    uint32_t view_id = 0;
    uint32_t dependent_layer_map = 0;
    bool same_sh_max_resolution = false;
    uint32_t max_expected_width = 0;
    uint32_t max_expected_height = 0;
  };

  // Per-xlayer info (parsed from lcr_xlayer_info)
  struct XLayerInfo {
    bool rep_info_present = false;
    bool purpose_present = false;
    bool color_info_present = false;
    bool embedded_layer_info_present = false;
    RepInfo rep_info;
    uint32_t purpose_id = 0;
    ColorInfo color_info;
    std::vector<EmbeddedLayerEntry> embedded_layers;
    // Atlas segment fields (when no embedded layer info, global + atlas present)
    uint32_t atlas_segment_id = 0;
    uint32_t priority_order = 0;
    uint32_t rendering_method = 0;
  };

  // Per-xlayer global payload info
  struct GlobalPayloadEntry {
    uint32_t xlayer_id = 0;
    uint32_t data_size = 0;
    uint32_t num_dependent_xlayer_map = 0;
    XLayerInfo xlayer_info;
  };

  bool is_global() const { return is_global_; }
  uint32_t lcr_global_config_record_id() const { return lcr_global_config_record_id_; }
  uint32_t lcr_xlayer_map() const { return lcr_xlayer_map_; }
  uint32_t lcr_global_purpose_id() const { return lcr_global_purpose_id_; }
  bool lcr_enforce_tile_alignment() const { return lcr_enforce_tile_alignment_flag_; }
  const std::vector<uint32_t>& xlayer_ids() const { return xlayer_ids_; }
  const std::vector<XLayerPTL>& xlayer_ptls() const { return xlayer_ptls_; }
  const AggregateInfo& aggregate_info() const { return aggregate_info_; }
  const std::vector<GlobalPayloadEntry>& global_payloads() const { return global_payloads_; }

  // Local LCR fields
  uint32_t lcr_global_id() const { return lcr_global_id_; }
  uint32_t lcr_local_id() const { return lcr_local_id_; }
  const XLayerInfo& local_xlayer_info() const { return local_xlayer_info_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  bool is_global_ = false;

  // Global LCR fields
  uint32_t lcr_global_config_record_id_ = 0;
  uint32_t lcr_xlayer_map_ = 0;
  uint32_t lcr_aggregate_info_present_flag_ = 0;
  uint32_t lcr_seq_profile_tier_level_info_present_flag_ = 0;
  uint32_t lcr_global_payload_present_flag_ = 0;
  uint32_t lcr_dependent_xlayers_flag_ = 0;
  uint32_t lcr_global_atlas_id_present_flag_ = 0;
  uint32_t lcr_global_purpose_id_ = 0;
  uint32_t lcr_doh_constraint_flag_ = 0;
  uint32_t lcr_enforce_tile_alignment_flag_ = 0;
  uint32_t lcr_global_atlas_id_ = 0;
  AggregateInfo aggregate_info_;
  std::vector<uint32_t> xlayer_ids_;
  std::vector<XLayerPTL> xlayer_ptls_;
  std::vector<GlobalPayloadEntry> global_payloads_;

  // Local LCR fields
  uint32_t lcr_global_id_ = 0;
  uint32_t lcr_local_id_ = 0;
  uint32_t lcr_profile_tier_level_info_present_flag_ = 0;
  uint32_t lcr_local_atlas_id_present_flag_ = 0;
  uint32_t lcr_local_atlas_id_ = 0;
  XLayerPTL local_ptl_;
  XLayerInfo local_xlayer_info_;
};

}  // namespace av2_obu
