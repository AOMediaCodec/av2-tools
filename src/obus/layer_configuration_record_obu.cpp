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

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/layer_configuration_record_obu.h>

namespace av2_obu {

using LCROBU = LayerConfigurationRecordOBU;

// Parse lcr_seq_profile_tier_level_info()
static LCROBU::XLayerPTL parse_ptl(BitstreamReader& br, uint32_t xlayer_id) {
  LCROBU::XLayerPTL ptl;
  ptl.xlayer_id = xlayer_id;
  ptl.seq_profile_idc = br.read_bits(5);
  ptl.max_level_idx = br.read_bits(5);
  ptl.tier_flag = br.read_bit();
  ptl.max_mlayer_count = br.read_bits(3);
  br.read_bits(2);  // lsptli_reserved_2bits
  return ptl;
}

// Parse lcr_rep_info()
static LCROBU::RepInfo parse_rep_info(BitstreamReader& br) {
  LCROBU::RepInfo ri;
  ri.max_pic_width = br.read_uvlc();
  ri.max_pic_height = br.read_uvlc();
  ri.format_info_present = br.read_bit();
  ri.cropping_window_present = br.read_bit();
  if (ri.format_info_present) {
    ri.bit_depth_idc = br.read_uvlc();
    ri.chroma_format_idc = br.read_uvlc();
  }
  if (ri.cropping_window_present) {
    ri.crop_left = br.read_uvlc();
    ri.crop_right = br.read_uvlc();
    ri.crop_top = br.read_uvlc();
    ri.crop_bottom = br.read_uvlc();
  }
  return ri;
}

// Parse lcr_xlayer_color_info()
static LCROBU::ColorInfo parse_color_info(BitstreamReader& br) {
  LCROBU::ColorInfo ci;
  ci.color_description_idc = br.read_rg(2);
  if (ci.color_description_idc == 0) {
    ci.color_primaries = br.read_bits(8);
    ci.transfer_characteristics = br.read_bits(8);
    ci.matrix_coefficients = br.read_bits(8);
  }
  ci.full_range_flag = br.read_bit();
  return ci;
}

// Parse lcr_embedded_layer_info()
static std::vector<LCROBU::EmbeddedLayerEntry> parse_embedded_layer_info(
    BitstreamReader& br, bool atlas_segment_present) {
  std::vector<LCROBU::EmbeddedLayerEntry> entries;
  uint32_t mlayer_map = br.read_bits(8);

  for (uint32_t j = 0; j < 8; j++) {
    if (mlayer_map & (1u << j)) {
      LCROBU::EmbeddedLayerEntry e;
      e.mlayer_id = j;
      e.tlayer_map = br.read_bits(MAX_NUM_TLAYERS);
      if (atlas_segment_present) {
        e.atlas_segment_id = br.read_bits(8);
        e.priority_order = br.read_bits(8);
        e.rendering_method = br.read_bits(8);
      }
      e.layer_type = br.read_bits(8);
      if (e.layer_type == AUX_LAYER) {
        e.auxiliary_type = br.read_bits(8);
      }
      e.view_type = br.read_bits(8);
      if (e.view_type == VIEW_EXPLICIT) {
        e.view_id = br.read_bits(8);
      }
      if (j > 0) {
        e.dependent_layer_map = br.read_bits(j);
      }
      e.same_sh_max_resolution = br.read_bit();
      if (!e.same_sh_max_resolution) {
        e.max_expected_width = br.read_uvlc();
        e.max_expected_height = br.read_uvlc();
      }
      br.byte_align();
      entries.push_back(e);
    }
  }
  return entries;
}

// Parse lcr_xlayer_info()
static LCROBU::XLayerInfo parse_xlayer_info(BitstreamReader& br, bool is_global,
                                            bool atlas_id_present) {
  LCROBU::XLayerInfo xi;
  xi.rep_info_present = br.read_bit();
  xi.purpose_present = br.read_bit();
  xi.color_info_present = br.read_bit();
  xi.embedded_layer_info_present = br.read_bit();

  if (xi.rep_info_present) {
    xi.rep_info = parse_rep_info(br);
  }
  if (xi.purpose_present) {
    xi.purpose_id = br.read_bits(7);
  }
  if (xi.color_info_present) {
    xi.color_info = parse_color_info(br);
  }
  br.byte_align();
  if (xi.embedded_layer_info_present) {
    xi.embedded_layers = parse_embedded_layer_info(br, atlas_id_present);
  } else {
    if (is_global && atlas_id_present) {
      xi.atlas_segment_id = br.read_bits(8);
      xi.priority_order = br.read_bits(8);
      xi.rendering_method = br.read_bits(8);
    }
  }
  return xi;
}

bool LayerConfigurationRecordOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing LAYER_CONFIGURATION_RECORD payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read LAYER_CONFIGURATION_RECORD payload");
    return false;
  }

  BitstreamReader br(raw_payload_);
  is_global_ = (header_.get_xlayer_id() == 31);

  if (is_global_) {
    // lcr_global_info()
    lcr_global_config_record_id_ = br.read_bits(3);
    lcr_xlayer_map_ = br.read_bits(31);

    xlayer_ids_.clear();
    for (uint32_t i = 0; i < 31; i++) {
      if (lcr_xlayer_map_ & (1u << i)) {
        xlayer_ids_.push_back(i);
      }
    }

    lcr_aggregate_info_present_flag_ = br.read_bit();
    lcr_seq_profile_tier_level_info_present_flag_ = br.read_bit();
    lcr_global_payload_present_flag_ = br.read_bit();
    lcr_dependent_xlayers_flag_ = br.read_bit();
    lcr_global_atlas_id_present_flag_ = br.read_bit();
    lcr_global_purpose_id_ = br.read_bits(7);
    lcr_doh_constraint_flag_ = br.read_bit();
    lcr_enforce_tile_alignment_flag_ = br.read_bit();

    if (lcr_global_atlas_id_present_flag_) {
      lcr_global_atlas_id_ = br.read_bits(3);
    } else {
      br.read_bits(3);  // reserved
    }
    br.read_bits(5);  // reserved

    if (lcr_aggregate_info_present_flag_) {
      aggregate_info_.config_idc = br.read_bits(6);
      aggregate_info_.aggregate_level_idx = br.read_bits(5);
      aggregate_info_.max_tier_flag = br.read_bit();
      aggregate_info_.max_interop = br.read_bits(4);
    }

    if (lcr_seq_profile_tier_level_info_present_flag_) {
      xlayer_ptls_.clear();
      for (uint32_t xlid : xlayer_ids_) {
        xlayer_ptls_.push_back(parse_ptl(br, xlid));
      }
    }

    // lcr_global_payload per xlayer
    if (lcr_global_payload_present_flag_) {
      global_payloads_.resize(xlayer_ids_.size());
      for (size_t i = 0; i < xlayer_ids_.size(); i++) {
        auto& gp = global_payloads_[i];
        gp.xlayer_id = xlayer_ids_[i];
        gp.data_size = br.read_leb128();

        // Extract payload bytes and parse with a bounded sub-reader
        size_t payload_start_byte = br.bits_read() / 8;
        if (payload_start_byte + gp.data_size <= raw_payload_.size()) {
          std::vector<uint8_t> sub_data(raw_payload_.begin() + payload_start_byte,
                                        raw_payload_.begin() + payload_start_byte + gp.data_size);
          BitstreamReader sub_br(sub_data);

          try {
            // lcr_global_payload(n, sz)
            if (lcr_dependent_xlayers_flag_ && xlayer_ids_[i] > 0) {
              gp.num_dependent_xlayer_map = sub_br.read_bits(xlayer_ids_[i]);
            }
            gp.xlayer_info = parse_xlayer_info(sub_br, true, lcr_global_atlas_id_present_flag_);
          } catch (const std::exception& e) {
            spdlog::warn("LCR: failed to parse xlayer {} payload: {}", gp.xlayer_id, e.what());
          }
        }

        // Advance past this payload in the main reader
        br.skip_bits(static_cast<size_t>(gp.data_size) * 8);
      }
    }
  } else {
    // lcr_local_info()
    uint32_t xlayer_id = header_.get_xlayer_id();
    lcr_global_id_ = br.read_bits(3);
    lcr_local_id_ = br.read_bits(3);
    lcr_profile_tier_level_info_present_flag_ = br.read_bit();
    lcr_local_atlas_id_present_flag_ = br.read_bit();

    if (lcr_profile_tier_level_info_present_flag_) {
      local_ptl_ = parse_ptl(br, xlayer_id);
    }

    if (lcr_local_atlas_id_present_flag_) {
      lcr_local_atlas_id_ = br.read_bits(3);
    } else {
      br.read_bits(3);  // reserved
    }
    br.read_bits(5);  // reserved

    local_xlayer_info_ = parse_xlayer_info(br, false, lcr_local_atlas_id_present_flag_);
  }

  parsed_ = true;
  spdlog::debug("LCR: {} mode, xlayer_map=0x{:08x}, {} xlayers", is_global_ ? "global" : "local",
                lcr_xlayer_map_, xlayer_ids_.size());
  return true;
}

static json xlayer_info_to_json(const LCROBU::XLayerInfo& xi) {
  json j;
  j["rep_info_present"] = xi.rep_info_present;
  if (xi.rep_info_present) {
    json ri;
    ri["max_pic_width"] = xi.rep_info.max_pic_width;
    ri["max_pic_height"] = xi.rep_info.max_pic_height;
    if (xi.rep_info.format_info_present) {
      ri["bit_depth_idc"] = xi.rep_info.bit_depth_idc;
      ri["chroma_format_idc"] = xi.rep_info.chroma_format_idc;
    }
    if (xi.rep_info.cropping_window_present) {
      ri["crop_left"] = xi.rep_info.crop_left;
      ri["crop_right"] = xi.rep_info.crop_right;
      ri["crop_top"] = xi.rep_info.crop_top;
      ri["crop_bottom"] = xi.rep_info.crop_bottom;
    }
    j["rep_info"] = ri;
  }
  if (xi.purpose_present) {
    j["purpose_id"] = xi.purpose_id;
  }
  if (xi.color_info_present) {
    json ci;
    ci["color_description_idc"] = xi.color_info.color_description_idc;
    if (xi.color_info.color_description_idc == 0) {
      ci["color_primaries"] = xi.color_info.color_primaries;
      ci["transfer_characteristics"] = xi.color_info.transfer_characteristics;
      ci["matrix_coefficients"] = xi.color_info.matrix_coefficients;
    }
    ci["full_range_flag"] = xi.color_info.full_range_flag;
    j["color_info"] = ci;
  }
  if (xi.embedded_layer_info_present && !xi.embedded_layers.empty()) {
    json layers = json::array();
    for (const auto& e : xi.embedded_layers) {
      json el;
      el["mlayer_id"] = e.mlayer_id;
      el["tlayer_map"] = e.tlayer_map;
      el["layer_type"] = e.layer_type;
      if (e.layer_type == AUX_LAYER) {
        el["auxiliary_type"] = e.auxiliary_type;
      }
      el["view_type"] = e.view_type;
      if (e.view_type == VIEW_EXPLICIT) {
        el["view_id"] = e.view_id;
      }
      if (!e.same_sh_max_resolution) {
        el["max_expected_width"] = e.max_expected_width;
        el["max_expected_height"] = e.max_expected_height;
      }
      layers.push_back(el);
    }
    j["embedded_layers"] = layers;
  }
  return j;
}

json LayerConfigurationRecordOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "LAYER_CONFIGURATION_RECORD";
  if (!parsed_) return j;

  if (is_global_) {
    j["lcr_mode"] = "global";
    j["lcr_global_config_record_id"] = lcr_global_config_record_id_;
    j["lcr_xlayer_map"] = lcr_xlayer_map_;
    j["xlayer_ids"] = xlayer_ids_;
    j["lcr_aggregate_info_present_flag"] = lcr_aggregate_info_present_flag_;
    j["lcr_seq_profile_tier_level_info_present_flag"] =
        lcr_seq_profile_tier_level_info_present_flag_;
    j["lcr_global_payload_present_flag"] = lcr_global_payload_present_flag_;
    j["lcr_dependent_xlayers_flag"] = lcr_dependent_xlayers_flag_;
    j["lcr_global_purpose_id"] = lcr_global_purpose_id_;
    j["lcr_global_atlas_id_present_flag"] = lcr_global_atlas_id_present_flag_;
    j["lcr_doh_constraint_flag"] = lcr_doh_constraint_flag_;
    j["lcr_enforce_tile_alignment_flag"] = lcr_enforce_tile_alignment_flag_;
    if (lcr_global_atlas_id_present_flag_) {
      j["lcr_global_atlas_id"] = lcr_global_atlas_id_;
    }

    if (lcr_aggregate_info_present_flag_) {
      j["aggregate_info"] = {{"config_idc", aggregate_info_.config_idc},
                             {"aggregate_level_idx", aggregate_info_.aggregate_level_idx},
                             {"max_tier_flag", aggregate_info_.max_tier_flag},
                             {"max_interop", aggregate_info_.max_interop}};
    }

    if (!xlayer_ptls_.empty()) {
      json ptls = json::array();
      for (const auto& ptl : xlayer_ptls_) {
        ptls.push_back({{"xlayer_id", ptl.xlayer_id},
                        {"seq_profile_idc", ptl.seq_profile_idc},
                        {"max_level_idx", ptl.max_level_idx},
                        {"tier_flag", ptl.tier_flag},
                        {"max_mlayer_count", ptl.max_mlayer_count}});
      }
      j["xlayer_ptl"] = ptls;
    }

    if (!global_payloads_.empty()) {
      json payloads = json::array();
      for (const auto& gp : global_payloads_) {
        json gpj;
        gpj["xlayer_id"] = gp.xlayer_id;
        gpj["data_size"] = gp.data_size;
        if (gp.num_dependent_xlayer_map != 0) {
          gpj["num_dependent_xlayer_map"] = gp.num_dependent_xlayer_map;
        }
        gpj["xlayer_info"] = xlayer_info_to_json(gp.xlayer_info);
        payloads.push_back(gpj);
      }
      j["global_payloads"] = payloads;
    }
  } else {
    j["lcr_mode"] = "local";
    j["lcr_global_id"] = lcr_global_id_;
    j["lcr_local_id"] = lcr_local_id_;
    j["lcr_profile_tier_level_info_present_flag"] = lcr_profile_tier_level_info_present_flag_;
    if (lcr_profile_tier_level_info_present_flag_) {
      j["local_ptl"] = {{"seq_profile_idc", local_ptl_.seq_profile_idc},
                        {"max_level_idx", local_ptl_.max_level_idx},
                        {"tier_flag", local_ptl_.tier_flag},
                        {"max_mlayer_count", local_ptl_.max_mlayer_count}};
    }
    if (lcr_local_atlas_id_present_flag_) {
      j["lcr_local_atlas_id"] = lcr_local_atlas_id_;
    }
    j["xlayer_info"] = xlayer_info_to_json(local_xlayer_info_);
  }

  return j;
}

}  // namespace av2_obu
