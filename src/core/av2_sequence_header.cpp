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

#include <algorithm>

#include <av2_obu/core/av2_sequence_header.h>

namespace av2_obu {

// ==================== ColorConfig ====================

bool ColorConfig::parse(BitstreamReader& br) {
  spdlog::debug("Parsing color_config");

  chroma_format_idc = br.read_uvlc();
  spdlog::debug("  chroma_format_idc = {}", chroma_format_idc);

  if (chroma_format_idc == CHROMA_FORMAT_420) {
    SubsamplingX = 1;
    SubsamplingY = 1;
  } else if (chroma_format_idc == CHROMA_FORMAT_444) {
    SubsamplingX = 0;
    SubsamplingY = 0;
  } else if (chroma_format_idc == CHROMA_FORMAT_422) {
    SubsamplingX = 1;
    SubsamplingY = 0;
  } else if (chroma_format_idc == CHROMA_FORMAT_400) {
    SubsamplingX = 1;
    SubsamplingY = 1;
  }

  bit_depth_idc = br.read_uvlc();
  BitDepth = (bit_depth_idc == 0) ? 10 : ((bit_depth_idc == 1) ? 8 : 12);
  MaxQ = (BitDepth == 8) ? MAXQ_8_BITS : ((BitDepth == 10) ? MAXQ_10_BITS : MAXQ_12_BITS);
  spdlog::debug("  bit_depth_idc = {}, BitDepth = {}", bit_depth_idc, BitDepth);

  Monochrome = (chroma_format_idc == CHROMA_FORMAT_400);
  NumPlanes = Monochrome ? 1 : 3;

  color_description_present_flag = br.read_bit();
  if (color_description_present_flag) {
    color_primaries = br.read_bits(8);
    transfer_characteristics = br.read_bits(8);
    matrix_coefficients = br.read_bits(8);
    spdlog::debug("  color: primaries={}, transfer={}, matrix={}", color_primaries,
                  transfer_characteristics, matrix_coefficients);
  } else {
    color_primaries = CP_UNSPECIFIED;
    transfer_characteristics = TC_UNSPECIFIED;
    matrix_coefficients = MC_UNSPECIFIED;
  }

  if (Monochrome) {
    color_range = br.read_bit();
    chroma_sample_position = CSP_UNSPECIFIED;
  } else if (color_primaries == CP_BT_709 && transfer_characteristics == TC_SRGB &&
             matrix_coefficients == MC_IDENTITY) {
    color_range = 1;
    chroma_sample_position = CSP_UNSPECIFIED;
  } else {
    color_range = br.read_bit();
    if (SubsamplingX) {
      csp_present_flag = br.read_bit();
      if (csp_present_flag) {
        chroma_sample_position = br.read_bits(SubsamplingY ? 3 : 1);
      } else {
        chroma_sample_position = CSP_UNSPECIFIED;
      }
    }
  }

  return true;
}

json ColorConfig::to_json() const {
  return json{{"chroma_format_idc", chroma_format_idc},
              {"bit_depth_idc", bit_depth_idc},
              {"BitDepth", BitDepth},
              {"Monochrome", Monochrome},
              {"NumPlanes", NumPlanes},
              {"SubsamplingX", SubsamplingX},
              {"SubsamplingY", SubsamplingY},
              {"color_primaries", color_primaries},
              {"transfer_characteristics", transfer_characteristics},
              {"matrix_coefficients", matrix_coefficients},
              {"color_range", color_range},
              {"chroma_sample_position", chroma_sample_position}};
}

// ==================== TimingInfo ====================

bool TimingInfo::parse(BitstreamReader& br) {
  spdlog::debug("Parsing timing_info");

  num_units_in_display_tick = br.read_bits(32);
  time_scale = br.read_bits(32);
  equal_picture_interval = br.read_bit();

  if (equal_picture_interval) {
    num_ticks_per_picture_minus_1 = br.read_uvlc();
  }

  spdlog::debug("  num_units_in_display_tick = {}", num_units_in_display_tick);
  spdlog::debug("  time_scale = {}", time_scale);

  return true;
}

json TimingInfo::to_json() const {
  json j = {{"num_units_in_display_tick", num_units_in_display_tick},
            {"time_scale", time_scale},
            {"equal_picture_interval", equal_picture_interval}};
  if (equal_picture_interval) {
    j["num_ticks_per_picture_minus_1"] = num_ticks_per_picture_minus_1;
  }
  return j;
}

// ==================== DecoderModelInfo ====================

bool DecoderModelInfo::parse(BitstreamReader& br) {
  spdlog::debug("Parsing decoder_model_info");

  buffer_delay_length_minus_1 = br.read_bits(5);
  num_units_in_decoding_tick = br.read_bits(32);
  frame_presentation_time_length_minus_1 = br.read_bits(5);

  return true;
}

json DecoderModelInfo::to_json() const {
  return json{{"buffer_delay_length_minus_1", buffer_delay_length_minus_1},
              {"num_units_in_decoding_tick", num_units_in_decoding_tick},
              {"frame_presentation_time_length_minus_1", frame_presentation_time_length_minus_1}};
}

// ==================== OperatingParametersInfo ====================

bool OperatingParametersInfo::parse(BitstreamReader& br, uint32_t buffer_delay_length_minus_1) {
  uint32_t n = buffer_delay_length_minus_1 + 1;

  decoder_buffer_delay = br.read_bits(n);
  encoder_buffer_delay = br.read_bits(n);
  low_delay_mode_flag = br.read_bit();

  return true;
}

json OperatingParametersInfo::to_json() const {
  return json{{"decoder_buffer_delay", decoder_buffer_delay},
              {"encoder_buffer_delay", encoder_buffer_delay},
              {"low_delay_mode_flag", low_delay_mode_flag}};
}

// ==================== SequenceIntraConfig ====================

bool SequenceIntraConfig::parse(BitstreamReader& br) {
  spdlog::debug("Parsing sequence_intra_config");

  enable_dip = br.read_bit();
  enable_intra_edge_filter = br.read_bit();
  enable_mrls = br.read_bit();
  enable_cfl_intra = br.read_bit();
  enable_mhccp = br.read_bit();
  enable_orip = br.read_bit();
  enable_ibp = br.read_bit();

  return true;
}

json SequenceIntraConfig::to_json() const {
  return json{
    {"enable_dip", enable_dip},     {"enable_intra_edge_filter", enable_intra_edge_filter},
    {"enable_mrls", enable_mrls},   {"enable_cfl_intra", enable_cfl_intra},
    {"enable_mhccp", enable_mhccp}, {"enable_orip", enable_orip},
    {"enable_ibp", enable_ibp}};
}

// ==================== SequenceInterConfig ====================

bool SequenceInterConfig::parse(BitstreamReader& br, bool single_picture_header_flag) {
  spdlog::debug("Parsing sequence_inter_config (single_picture_header={}))",
                single_picture_header_flag);

  seq_enabled_motion_modes.resize(MOTION_MODES, 0);

  if (single_picture_header_flag) {
    // Set defaults for single picture
    for (uint32_t i = 0; i < MOTION_MODES; i++) {
      seq_enabled_motion_modes[i] = 0;
    }
    enable_six_param_warp_delta = 0;
    enable_masked_compound = 0;
    enable_ref_frame_mvs = 0;
    reduced_ref_frame_mvs_mode = 0;
    OrderHintBits = 0;
    enable_opfl_refine = REFINE_NONE;

    enable_refmvbank = br.read_bit();
    disable_drl_reorder = br.read_bit();

    if (disable_drl_reorder) {
      DrlReorder = DRL_REORDER_DISABLED;
    } else {
      constrain_drl_reorder = br.read_bit();
      DrlReorder = constrain_drl_reorder ? DRL_REORDER_CONSTRAINT : DRL_REORDER_ALWAYS;
    }

    // enable_frame_output_order = 1 (implicit)

    seq_max_bvp_drl_bits_minus1 = br.read_ns(MAX_REF_BV_STACK_SIZE - 1);
    allow_frame_max_bvp_drl_bits = br.read_bit();

    enable_mv_traj = br.read_bit();
    enable_bawp = br.read_bit();
    enable_imp_msk_bld = br.read_bit();
    enable_fsc = br.read_bit();

    if (enable_fsc) {
      enable_idtx_intra = 1;
    } else {
      enable_idtx_intra = br.read_bit();
    }

    NumRefFrames = 2;
  } else {
    // Multi-picture mode
    uint32_t motionModeEnabled = 0;

    for (uint32_t mode = INTERINTRA; mode < MOTION_MODES; mode++) {
      seq_enabled_motion_modes[mode] = br.read_bit();
      motionModeEnabled |= seq_enabled_motion_modes[mode];
    }

    if (motionModeEnabled) {
      seq_frame_motion_modes_present_flag = br.read_bit();
    } else {
      seq_frame_motion_modes_present_flag = 0;
    }

    if (seq_enabled_motion_modes[DELTAWARP]) {
      enable_six_param_warp_delta = br.read_bit();
    } else {
      enable_six_param_warp_delta = 0;
    }

    enable_masked_compound = br.read_bit();
    enable_ref_frame_mvs = br.read_bit();

    if (enable_ref_frame_mvs) {
      reduced_ref_frame_mvs_mode = br.read_bit();
    } else {
      reduced_ref_frame_mvs_mode = 0;
    }

    order_hint_bits_minus_1 = br.read_bits(3);
    OrderHintBits = order_hint_bits_minus_1 + 1;

    enable_refmvbank = br.read_bit();
    disable_drl_reorder = br.read_bit();

    if (disable_drl_reorder) {
      DrlReorder = DRL_REORDER_DISABLED;
    } else {
      constrain_drl_reorder = br.read_bit();
      DrlReorder = constrain_drl_reorder ? DRL_REORDER_CONSTRAINT : DRL_REORDER_ALWAYS;
    }

    explicit_ref_frame_map = br.read_bit();
    use_extra_ref_frames = br.read_bit();

    if (use_extra_ref_frames) {
      num_ref_frames_minus_1 = br.read_bits(4);
      NumRefFrames = num_ref_frames_minus_1 + 1;
    } else {
      NumRefFrames = 8;
    }

    MaxReferenceFrames = std::min(REFS_PER_FRAME, NumRefFrames);

    seq_max_drl_bits_minus1 = br.read_ns(MAX_REF_MV_STACK_SIZE - 1);
    allow_frame_max_drl_bits = br.read_bit();

    seq_max_bvp_drl_bits_minus1 = br.read_ns(MAX_REF_BV_STACK_SIZE - 1);
    allow_frame_max_bvp_drl_bits = br.read_bit();

    num_same_ref_compound = br.read_bits(2);

    enable_tip = br.read_bit();
    if (enable_tip) {
      disable_tip_output = br.read_bit();
      EnableTipOutput = !disable_tip_output;
      enable_tip_hole_fill = br.read_bit();
    } else {
      enable_tip_hole_fill = 0;
      EnableTipOutput = false;
    }

    enable_mv_traj = br.read_bit();
    enable_bawp = br.read_bit();
    enable_cwp = br.read_bit();
    enable_imp_msk_bld = br.read_bit();
    enable_fsc = br.read_bit();

    if (enable_fsc) {
      enable_idtx_intra = 1;
    } else {
      enable_idtx_intra = br.read_bit();
    }

    enable_lf_sub_pu = br.read_bit();

    if (EnableTipOutput && enable_lf_sub_pu) {
      enable_tip_explicit_qp = br.read_bit();
    } else {
      enable_tip_explicit_qp = 0;
    }

    enable_opfl_refine = br.read_bits(2);
    enable_adaptive_mvd = br.read_bit();
    enable_refinemv = br.read_bit();

    if (enable_tip && (enable_opfl_refine != 0 || enable_refinemv)) {
      enable_tip_refinemv = br.read_bit();
    } else {
      enable_tip_refinemv = 0;
    }

    enable_bru = br.read_bit();
    enable_mvd_sign_derive = br.read_bit();
    enable_flex_mvres = br.read_bit();

    if (single_picture_header_flag) {
      enable_global_motion = 0;
    } else {
      enable_global_motion = br.read_bit();
    }

    enable_short_refresh_frame_flags = br.read_bit();
  }

  spdlog::debug("  NumRefFrames = {}, OrderHintBits = {}", NumRefFrames, OrderHintBits);

  return true;
}

json SequenceInterConfig::to_json() const {
  json j = {{"seq_enabled_motion_modes", seq_enabled_motion_modes},
            {"enable_refmvbank", enable_refmvbank},
            {"disable_drl_reorder", disable_drl_reorder},
            {"DrlReorder", DrlReorder},
            {"seq_max_bvp_drl_bits_minus1", seq_max_bvp_drl_bits_minus1},
            {"allow_frame_max_bvp_drl_bits", allow_frame_max_bvp_drl_bits},
            {"enable_mv_traj", enable_mv_traj},
            {"enable_bawp", enable_bawp},
            {"enable_imp_msk_bld", enable_imp_msk_bld},
            {"enable_fsc", enable_fsc},
            {"enable_idtx_intra", enable_idtx_intra},
            {"NumRefFrames", NumRefFrames},
            {"OrderHintBits", OrderHintBits}};

  if (seq_frame_motion_modes_present_flag) {
    j["seq_frame_motion_modes_present_flag"] = seq_frame_motion_modes_present_flag;
  }

  return j;
}

// Note: SequenceFilterConfig and SequenceTransformConfig implementations
// and the main AV2SequenceHeader::parse() will be in a follow-up message
// due to length constraints...

// ==================== SequenceFilterConfig ====================

bool SequenceFilterConfig::parse(BitstreamReader& br, bool single_picture_header_flag,
                                 bool Monochrome) {
  spdlog::debug("Parsing sequence_filter_config");

  if (single_picture_header_flag) {
    seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    seq_force_integer_mv = SELECT_INTEGER_MV;
  } else {
    seq_choose_screen_content_tools = br.read_bit();
    if (seq_choose_screen_content_tools) {
      seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    } else {
      seq_force_screen_content_tools = br.read_bit();
    }

    if (seq_force_screen_content_tools > 0) {
      seq_choose_integer_mv = br.read_bit();
      if (seq_choose_integer_mv) {
        seq_force_integer_mv = SELECT_INTEGER_MV;
      } else {
        seq_force_integer_mv = br.read_bit();
      }
    } else {
      seq_force_integer_mv = SELECT_INTEGER_MV;
    }
  }

  disable_loopfilters_across_tiles = br.read_bit();
  enable_cdef = br.read_bit();
  enable_gdf = br.read_bit();
  enable_restoration = br.read_bit();

  if (enable_restoration) {
    lr_tools_disable.resize(2, std::vector<uint32_t>(2, 0));
    lr_tools_disable[0][RESTORE_PC_WIENER] = br.read_bit();
    lr_tools_disable[0][RESTORE_WIENER_NONSEP] = br.read_bit();
    lr_tools_disable[1][RESTORE_PC_WIENER] = 1;

    lr_tools_uv_present = br.read_bit();
    if (lr_tools_uv_present) {
      lr_tools_disable[1][RESTORE_WIENER_NONSEP] = br.read_bit();
    } else {
      lr_tools_disable[1][RESTORE_WIENER_NONSEP] = lr_tools_disable[0][RESTORE_WIENER_NONSEP];
    }
  }

  enable_ccso = br.read_bit();

  if (Monochrome) {
    cfl_ds_filter_index = 0;
  } else {
    cfl_ds_filter_index = br.read_bits(2);
  }

  enable_tcq = br.read_bit();
  if (enable_tcq) {
    choose_tcq_per_frame = br.read_bit();
  } else {
    choose_tcq_per_frame = 0;
  }

  if (enable_tcq && !choose_tcq_per_frame) {
    enable_parity_hiding = 0;
  } else {
    enable_parity_hiding = br.read_bit();
  }

  enable_ext_partitions = br.read_bit();
  if (enable_ext_partitions) {
    enable_uneven_4way_partitions = br.read_bit();
  } else {
    enable_uneven_4way_partitions = 0;
  }

  return true;
}

json SequenceFilterConfig::to_json() const {
  return json{{"seq_force_screen_content_tools", seq_force_screen_content_tools},
              {"seq_force_integer_mv", seq_force_integer_mv},
              {"disable_loopfilters_across_tiles", disable_loopfilters_across_tiles},
              {"enable_cdef", enable_cdef},
              {"enable_gdf", enable_gdf},
              {"enable_restoration", enable_restoration},
              {"enable_ccso", enable_ccso},
              {"cfl_ds_filter_index", cfl_ds_filter_index},
              {"enable_tcq", enable_tcq},
              {"enable_parity_hiding", enable_parity_hiding},
              {"enable_ext_partitions", enable_ext_partitions},
              {"enable_uneven_4way_partitions", enable_uneven_4way_partitions}};
}

// ==================== SequenceTransformConfig ====================

bool SequenceTransformConfig::parse(BitstreamReader& br, bool single_picture_header_flag,
                                    bool Monochrome) {
  spdlog::debug("Parsing sequence_transform_config");

  if (Monochrome) {
    enable_sdp = 0;
  } else {
    enable_sdp = br.read_bit();
  }

  if (enable_sdp && !single_picture_header_flag) {
    enable_extended_sdp = br.read_bit();
  } else {
    enable_extended_sdp = 0;
  }

  enable_intra_ist = br.read_bit();
  enable_inter_ist = br.read_bit();

  if (Monochrome) {
    enable_chroma_dctonly = 0;
  } else {
    enable_chroma_dctonly = br.read_bit();
  }

  if (!single_picture_header_flag) {
    enable_inter_ddt = br.read_bit();
  }

  reduced_tx_part_set = br.read_bit();

  if (Monochrome) {
    enable_cctx = 0;
  } else {
    enable_cctx = br.read_bit();
  }

  long_term_frame_id_bits = br.read_bits(3);
  enable_ext_seg = br.read_bit();

  MaxSegments = enable_ext_seg ? 16 : 8;

  return true;
}

json SequenceTransformConfig::to_json() const {
  return json{{"enable_sdp", enable_sdp},
              {"enable_extended_sdp", enable_extended_sdp},
              {"enable_intra_ist", enable_intra_ist},
              {"enable_inter_ist", enable_inter_ist},
              {"enable_chroma_dctonly", enable_chroma_dctonly},
              {"enable_inter_ddt", enable_inter_ddt},
              {"reduced_tx_part_set", reduced_tx_part_set},
              {"enable_cctx", enable_cctx},
              {"long_term_frame_id_bits", long_term_frame_id_bits},
              {"enable_ext_seg", enable_ext_seg},
              {"MaxSegments", MaxSegments}};
}

// ==================== AV2SequenceHeader (Main) ====================

bool AV2SequenceHeader::parse(BitstreamReader& br) {
  spdlog::debug("Parsing AV2 Sequence Header");

  seq_header_id = br.read_uvlc();
  // TODO: if this is confirmed, add as a member variable
  uint32_t seq_lcr_id = br.read_bits(3);  // CONFIG_LCR_ID_IN_SH
  seq_profile = br.read_bits(3);
  frame_width_bits_minus_1 = br.read_bits(4);
  frame_height_bits_minus_1 = br.read_bits(4);

  uint32_t n = frame_width_bits_minus_1 + 1;
  max_frame_width_minus_1 = br.read_bits(n);

  n = frame_height_bits_minus_1 + 1;
  max_frame_height_minus_1 = br.read_bits(n);

  spdlog::debug("  seq_header_id = {}", seq_header_id);
  spdlog::debug("  seq_profile = {}", seq_profile);
  spdlog::debug("  max_frame_width = {}", max_frame_width_minus_1 + 1);
  spdlog::debug("  max_frame_height = {}", max_frame_height_minus_1 + 1);

  seq_cropping_window_present_flag = br.read_bit();
  if (seq_cropping_window_present_flag) {
    seq_cropping_win_left_offset = br.read_uvlc();
    seq_cropping_win_right_offset = br.read_uvlc();
    seq_cropping_win_top_offset = br.read_uvlc();
    seq_cropping_win_bottom_offset = br.read_uvlc();
  }

  // Parse color config
  if (!color_config.parse(br))
    return false;

  still_picture = br.read_bit();
  single_picture_header_flag = br.read_bit();

  if (single_picture_header_flag) {
    timing_info_present_flag = 0;
    decoder_model_info_present_flag = 0;
    initial_display_delay_present_flag = 0;
    operating_points_cnt_minus_1 = 0;

    operating_point_idc.resize(1, 0);
    seq_level_idx.resize(1);
    seq_level_idx[0] = br.read_bits(5);
    seq_tier.resize(1, 0);
    decoder_model_present_for_this_op.resize(1, 0);
    initial_display_delay_present_for_this_op.resize(1, 0);
  } else {
    timing_info_present_flag = br.read_bit();
    if (timing_info_present_flag) {
      if (!timing_info.parse(br))
        return false;

      decoder_model_info_present_flag = br.read_bit();
      if (decoder_model_info_present_flag) {
        if (!decoder_model_info.parse(br))
          return false;
      }
    } else {
      decoder_model_info_present_flag = 0;
    }

    initial_display_delay_present_flag = br.read_bit();
    operating_points_cnt_minus_1 = br.read_bits(5);

    uint32_t op_count = operating_points_cnt_minus_1 + 1;
    operating_point_idc.resize(op_count);
    seq_level_idx.resize(op_count);
    seq_tier.resize(op_count);
    decoder_model_present_for_this_op.resize(op_count);
    operating_parameters.resize(op_count);
    initial_display_delay_present_for_this_op.resize(op_count);
    initial_display_delay_minus_1.resize(op_count);

    for (uint32_t i = 0; i < op_count; i++) {
      operating_point_idc[i] = br.read_bits(MAX_NUM_TLAYERS + MAX_NUM_MLAYERS);
      seq_level_idx[i] = br.read_bits(5);

      if (seq_level_idx[i] > 7) {
        seq_tier[i] = br.read_bit();
      } else {
        seq_tier[i] = 0;
      }

      if (decoder_model_info_present_flag) {
        decoder_model_present_for_this_op[i] = br.read_bit();
        if (decoder_model_present_for_this_op[i]) {
          if (!operating_parameters[i].parse(br, decoder_model_info.buffer_delay_length_minus_1)) {
            return false;
          }
        }
      } else {
        decoder_model_present_for_this_op[i] = 0;
      }

      if (initial_display_delay_present_flag) {
        initial_display_delay_present_for_this_op[i] = br.read_bit();
        if (initial_display_delay_present_for_this_op[i]) {
          initial_display_delay_minus_1[i] = br.read_bits(4);
        }
      }
    }
  }

  // operatingPoint = choose_operating_point() - skip for now
  // OperatingPointIdc = operating_point_idc[operatingPoint]

  if (single_picture_header_flag) {
    max_tlayer_id = 0;
    max_mlayer_id = 0;
  } else {
    max_tlayer_id = br.read_bits(2);
    max_mlayer_id = br.read_bits(3);
  }

  // Skip tlayer_dependency and mlayer_dependency parsing for now (complex nested loops)
  if (max_tlayer_id > 0) {
    tlayer_dependency_present_flag = br.read_bit();
    if (tlayer_dependency_present_flag) {
      for (uint32_t currLayer = 1; currLayer <= max_tlayer_id; currLayer++) {
        for (uint32_t refLayer = currLayer;; refLayer--) {
          br.read_bit();  // tlayer_dependency_map
          if (refLayer == 0)
            break;
        }
      }
    }
  }

  if (max_mlayer_id > 0) {
    mlayer_dependency_present_flag = br.read_bit();
    if (mlayer_dependency_present_flag) {
      for (uint32_t currLayer = 1; currLayer <= max_mlayer_id; currLayer++) {
        for (uint32_t refLayer = currLayer;; refLayer--) {
          br.read_bit();  // mlayer_dependency_map
          if (refLayer == 0)
            break;
        }
      }
    }
  }

  use_256x256_superblock = br.read_bit();
  if (!use_256x256_superblock) {
    use_128x128_superblock = br.read_bit();
  }

  // Parse sub-configs
  if (!intra_config.parse(br))
    return false;
  if (!inter_config.parse(br, single_picture_header_flag))
    return false;
  if (!filter_config.parse(br, single_picture_header_flag, color_config.Monochrome))
    return false;
  if (!transform_config.parse(br, single_picture_header_flag, color_config.Monochrome))
    return false;

  // Parse delta Q configuration
  if (color_config.Monochrome) {
    separate_uv_delta_q = 0;
  } else {
    separate_uv_delta_q = br.read_bit();
  }

  BaseYDcDeltaQ = 0;
  BaseUVDcDeltaQ = 0;
  BaseUVAcDeltaQ = 0;

  equal_ac_dc_q = br.read_bit();
  if (!equal_ac_dc_q) {
    base_y_dc_delta_q = br.read_bits(DELTA_DCQUANT_BITS);
    BaseYDcDeltaQ = DELTA_DCQUANT_MIN + base_y_dc_delta_q;
    y_dc_delta_q_enabled = br.read_bit();
  }

  if (!color_config.Monochrome) {
    if (!equal_ac_dc_q) {
      base_uv_dc_delta_q = br.read_bits(DELTA_DCQUANT_BITS);
      BaseUVDcDeltaQ = DELTA_DCQUANT_MIN + base_uv_dc_delta_q;
      uv_dc_delta_q_enabled = br.read_bit();
    }

    base_uv_ac_delta_q = br.read_bits(DELTA_DCQUANT_BITS);
    BaseUVAcDeltaQ = DELTA_DCQUANT_MIN + base_uv_ac_delta_q;
    uv_ac_delta_q_enabled = br.read_bit();

    if (equal_ac_dc_q) {
      BaseUVDcDeltaQ = BaseUVAcDeltaQ;
    }
  }

  // Tile info
  seq_tile_info_present_flag = br.read_bit();
  if (seq_tile_info_present_flag) {
    allow_tile_info_change = br.read_bit();
    // Skip tile_params for now - very complex
    spdlog::warn("Tile parameters parsing not yet implemented - skipping");
  }

  film_grain_params_present = br.read_bit();

  // CDEF on skip transform
  if (single_picture_header_flag) {
    CdefOnSkipTxfm = CDEF_ON_SKIP_TXFM_ADAPTIVE;
    enable_avg_cdf = 1;
    avg_cdf_type = 1;
  } else {
    cdef_on_skip_txfm_always_on = br.read_bit();
    if (cdef_on_skip_txfm_always_on) {
      CdefOnSkipTxfm = CDEF_ON_SKIP_TXFM_ALWAYS_ON;
    } else {
      cdef_on_skip_txfm_disabled = br.read_bit();
      CdefOnSkipTxfm =
        cdef_on_skip_txfm_disabled ? CDEF_ON_SKIP_TXFM_DISABLED : CDEF_ON_SKIP_TXFM_ADAPTIVE;
    }

    enable_avg_cdf = br.read_bit();
    if (enable_avg_cdf) {
      avg_cdf_type = br.read_bit();
    }
  }

  // Aspect ratio
  reduce_aspect_ratio = br.read_bit();
  if (reduce_aspect_ratio) {
    max_pb_aspect_ratio_log2_m1 = br.read_bit();
    MaxAspectRatio = 1 << (max_pb_aspect_ratio_log2_m1 + 1);
  } else {
    MaxAspectRatio = 8;
  }

  df_par_bits_minus2 = br.read_bits(2);

  user_defined_qmatrix = br.read_bit();
  if (user_defined_qmatrix) {
    // Skip user_defined_qms for now
    spdlog::warn("User-defined QM parsing not yet implemented - skipping");
  }

  scan_type_info_present_flag = br.read_bit();
  if (scan_type_info_present_flag) {
    scan_type_idc = br.read_bits(2);
    fixed_cvs_pic_rate_flag = br.read_bit();
    if (fixed_cvs_pic_rate_flag) {
      elemental_ct_duration_minus_1 = br.read_uvlc();
    }
  } else {
    scan_type_idc = 0;
    fixed_cvs_pic_rate_flag = 0;
  }

  spdlog::debug("Successfully parsed AV2 Sequence Header");
  return true;
}

json AV2SequenceHeader::to_json() const {
  json j = {{"seq_header_id", seq_header_id},
            {"seq_profile", seq_profile},
            {"max_frame_width", max_frame_width_minus_1 + 1},
            {"max_frame_height", max_frame_height_minus_1 + 1},
            {"still_picture", still_picture},
            {"single_picture_header_flag", single_picture_header_flag},
            {"color_config", color_config.to_json()},
            {"intra_config", intra_config.to_json()},
            {"inter_config", inter_config.to_json()},
            {"filter_config", filter_config.to_json()},
            {"transform_config", transform_config.to_json()},
            {"use_256x256_superblock", use_256x256_superblock},
            {"use_128x128_superblock", use_128x128_superblock},
            {"film_grain_params_present", film_grain_params_present}};

  if (seq_cropping_window_present_flag) {
    j["cropping_window"] = {{"left", seq_cropping_win_left_offset},
                            {"right", seq_cropping_win_right_offset},
                            {"top", seq_cropping_win_top_offset},
                            {"bottom", seq_cropping_win_bottom_offset}};
  }

  if (timing_info_present_flag) {
    j["timing_info"] = timing_info.to_json();
  }

  return j;
}

}  // namespace av2_obu
