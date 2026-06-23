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
#include <av2_obu/core/logging.h>

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/operating_point_set_obu.h>

namespace av2_obu {

using OPS = OperatingPointSetOBU;

// Parse ops_aggregate_info()
static OPS::AggregateInfo parse_aggregate_info(BitstreamReader& br) {
  OPS::AggregateInfo ai;
  ai.config_idc = br.read_bits(6);
  ai.aggregate_level_idx = br.read_bits(5);
  ai.max_tier_flag = br.read_bit();
  ai.max_interop = br.read_bits(4);
  return ai;
}

// Parse ops_seq_profile_tier_level_info()
static OPS::PTLInfo parse_ptl_info(BitstreamReader& br, uint32_t xlayer_id) {
  OPS::PTLInfo ptl;
  ptl.xlayer_id = xlayer_id;
  ptl.seq_profile_idc = br.read_bits(5);
  ptl.level_idx = br.read_bits(5);
  ptl.tier_flag = br.read_bit();
  ptl.mlayer_count = br.read_bits(3);
  br.read_bits(2);  // ops_ptl_reserved_2bits
  return ptl;
}

// Parse ops_color_info()
static OPS::ColorInfo parse_color_info(BitstreamReader& br) {
  OPS::ColorInfo ci;
  ci.color_description_idc = br.read_rg(2);
  if (ci.color_description_idc == 0) {
    ci.color_primaries = br.read_bits(8);
    ci.transfer_characteristics = br.read_bits(8);
    ci.matrix_coefficients = br.read_bits(8);
  }
  ci.full_range_flag = br.read_bit();
  return ci;
}

// Parse ops_decoder_model_info()
static OPS::DecoderModelInfo parse_decoder_model_info(BitstreamReader& br) {
  OPS::DecoderModelInfo dmi;
  dmi.decoder_buffer_delay = br.read_uvlc();
  dmi.encoder_buffer_delay = br.read_uvlc();
  dmi.low_delay_mode_flag = br.read_bit();
  return dmi;
}

// Parse ops_mlayer_info()
static OPS::MlayerEntry parse_mlayer_info(BitstreamReader& br, uint32_t xlayer_id) {
  OPS::MlayerEntry me;
  me.xlayer_id = xlayer_id;
  me.mlayer_map = br.read_bits(8);
  for (uint32_t j = 0; j < 8; j++) {
    if (me.mlayer_map & (1u << j)) {
      uint32_t tlayer_map = br.read_bits(4);
      me.tlayer_maps.push_back(tlayer_map);
    }
  }
  return me;
}

bool OperatingPointSetOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing OPERATING_POINT_SET payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read OPERATING_POINT_SET payload");
    return false;
  }

  BitstreamReader br(raw_payload_);

  uint32_t xlayer_id = header_.get_xlayer_id();
  bool is_global = (xlayer_id == GLOBAL_XLAYER_ID);

  // operating_point_set_obu()
  ops_reset_flag_ = br.read_bit();
  ops_id_ = br.read_bits(4);
  ops_cnt_ = br.read_bits(3);

  if (ops_cnt_ > 0) {
    ops_priority_ = br.read_bits(4);
    ops_intent_ = br.read_bits(7);
    ops_intent_present_flag_ = br.read_bit();
    ops_ptl_present_flag_ = br.read_bit();
    ops_color_info_present_flag_ = br.read_bit();

    if (is_global) {
      ops_mlayer_info_idc_ = br.read_bits(2);
    } else {
      br.read_bits(2);  // ops_reserved_2bits
    }

    // Parse each operating_point_payload()
    operating_points_.resize(ops_cnt_);
    for (uint32_t i = 0; i < ops_cnt_; i++) {
      auto& op = operating_points_[i];
      op.ops_data_size = br.read_leb128();
      size_t start_pos = br.bits_read();

      // ops_op_intent (conditional on ops_intent_present_flag)
      if (ops_intent_present_flag_) {
        op.ops_op_intent = br.read_bits(7);
      }

      // PTL info
      if (ops_ptl_present_flag_) {
        if (is_global) {
          op.aggregate_info = parse_aggregate_info(br);
        } else {
          op.xlayer_ptls.push_back(parse_ptl_info(br, xlayer_id));
        }
      }

      // Color info
      if (ops_color_info_present_flag_) {
        op.has_color_info = true;
        op.color_info = parse_color_info(br);
      }

      // Decoder model info
      op.decoder_model_present = br.read_bit();
      if (op.decoder_model_present) {
        op.decoder_model_info = parse_decoder_model_info(br);
      }

      // Initial display delay
      op.initial_display_delay_present = br.read_bit();
      if (op.initial_display_delay_present) {
        op.initial_display_delay_minus_1 = br.read_bits(4);
      }

      if (is_global) {
        // Global: xlayer map + per-xlayer PTL and mlayer info
        op.ops_xlayer_map = br.read_bits(31);

        for (uint32_t j = 0; j < 31; j++) {
          if (op.ops_xlayer_map & (1u << j)) {
            // Per-xlayer PTL
            if (ops_ptl_present_flag_) {
              op.xlayer_ptls.push_back(parse_ptl_info(br, j));
            }

            // Per-xlayer mlayer info
            uint32_t idc = ops_mlayer_info_idc_;
            if (idc == 1) {
              op.mlayer_entries.push_back(parse_mlayer_info(br, j));
            } else if (idc == 2) {
              uint32_t explicit_flag = br.read_bit();
              if (explicit_flag) {
                op.mlayer_entries.push_back(parse_mlayer_info(br, j));
              } else {
                br.read_bits(4);  // ops_embedded_ops_id
                br.read_bits(3);  // ops_embedded_op_index
              }
            }
          }
        }
      } else {
        // Non-global: mlayer info for this xlayer
        op.mlayer_entries.push_back(parse_mlayer_info(br, xlayer_id));
      }

      br.byte_align();

      // Advance to exact end of this OP payload using data_size
      size_t end_pos = start_pos + static_cast<size_t>(op.ops_data_size) * 8;
      if (br.bits_read() != end_pos) {
        LIB_DEBUG("OPS: OP {} consumed {} bits, data_size={} bytes ({} bits)", i,
                      br.bits_read() - start_pos, op.ops_data_size, end_pos - start_pos);
        br.set_bit_pos(end_pos);
      }
    }
  }

  // Parse obu_extension_flag + trailing_bits (extensible OBU)
  if (!parse_obu_trailing_bits(br))
    return false;

  LIB_DEBUG("OPS: id={}, cnt={}, intent={}, reset={}", ops_id_, ops_cnt_, ops_intent_,
                ops_reset_flag_);
  return true;
}

json OperatingPointSetOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "OPERATING_POINT_SET";
  if (!parsed_) return j;

  j["ops_reset_flag"] = ops_reset_flag_;
  j["ops_id"] = ops_id_;
  j["ops_cnt"] = ops_cnt_;

  if (ops_cnt_ > 0) {
    j["ops_priority"] = ops_priority_;
    j["ops_intent"] = ops_intent_;
    j["ops_intent_present_flag"] = ops_intent_present_flag_;
    j["ops_ptl_present_flag"] = ops_ptl_present_flag_;
    j["ops_color_info_present_flag"] = ops_color_info_present_flag_;
    if (header_.get_xlayer_id() == GLOBAL_XLAYER_ID) {
      j["ops_mlayer_info_idc"] = ops_mlayer_info_idc_;
    }

    json ops_json = json::array();
    for (const auto& op : operating_points_) {
      json op_j;
      op_j["ops_data_size"] = op.ops_data_size;

      if (ops_intent_present_flag_) {
        op_j["ops_op_intent"] = op.ops_op_intent;
      }

      if (ops_ptl_present_flag_ && header_.get_xlayer_id() == GLOBAL_XLAYER_ID) {
        op_j["ops_config_idc"] = op.aggregate_info.config_idc;
        op_j["ops_aggregate_level_idx"] = op.aggregate_info.aggregate_level_idx;
        op_j["ops_max_tier_flag"] = op.aggregate_info.max_tier_flag;
        op_j["ops_max_interop"] = op.aggregate_info.max_interop;
      }

      if (op.has_color_info) {
        json ci;
        ci["color_description_idc"] = op.color_info.color_description_idc;
        if (op.color_info.color_description_idc == 0) {
          ci["color_primaries"] = op.color_info.color_primaries;
          ci["transfer_characteristics"] = op.color_info.transfer_characteristics;
          ci["matrix_coefficients"] = op.color_info.matrix_coefficients;
        }
        ci["full_range_flag"] = op.color_info.full_range_flag;
        op_j["color_info"] = ci;
      }

      op_j["ops_decoder_model_info_for_this_op_present_flag"] = op.decoder_model_present;
      if (op.decoder_model_present) {
        op_j["decoder_buffer_delay"] = op.decoder_model_info.decoder_buffer_delay;
        op_j["encoder_buffer_delay"] = op.decoder_model_info.encoder_buffer_delay;
        op_j["low_delay_mode_flag"] = op.decoder_model_info.low_delay_mode_flag;
      }

      op_j["ops_initial_display_delay_present_flag"] = op.initial_display_delay_present;
      if (op.initial_display_delay_present) {
        op_j["ops_initial_display_delay_minus_1"] = op.initial_display_delay_minus_1;
      }

      if (op.ops_xlayer_map != 0) {
        op_j["ops_xlayer_map"] = op.ops_xlayer_map;
      }

      if (!op.xlayer_ptls.empty()) {
        json ptls = json::array();
        for (const auto& ptl : op.xlayer_ptls) {
          ptls.push_back({{"xlayer_id", ptl.xlayer_id},
                          {"seq_profile_idc", ptl.seq_profile_idc},
                          {"level_idx", ptl.level_idx},
                          {"tier_flag", ptl.tier_flag},
                          {"mlayer_count", ptl.mlayer_count}});
        }
        op_j["xlayer_ptl"] = ptls;
      }

      if (!op.mlayer_entries.empty()) {
        json mls = json::array();
        for (const auto& me : op.mlayer_entries) {
          json ml;
          ml["xlayer_id"] = me.xlayer_id;
          ml["mlayer_map"] = me.mlayer_map;
          if (!me.tlayer_maps.empty()) {
            ml["tlayer_maps"] = me.tlayer_maps;
          }
          mls.push_back(ml);
        }
        op_j["mlayer_info"] = mls;
      }

      ops_json.push_back(op_j);
    }
    j["operating_points"] = ops_json;
  }

  return j;
}

}  // namespace av2_obu
