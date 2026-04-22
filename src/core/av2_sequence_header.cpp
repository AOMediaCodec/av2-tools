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
#include <av2_obu/core/av2_tables.h>

namespace av2_obu {

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

// ==================== SeqDecoderModelInfo ====================

bool SeqDecoderModelInfo::parse(BitstreamReader& br) {
  spdlog::debug("Parsing seq_decoder_model_info");

  decoder_buffer_delay = br.read_uvlc();
  encoder_buffer_delay = br.read_uvlc();
  low_delay_mode_flag = br.read_bit();

  spdlog::debug("  decoder_buffer_delay = {}", decoder_buffer_delay);
  spdlog::debug("  encoder_buffer_delay = {}", encoder_buffer_delay);
  spdlog::debug("  low_delay_mode_flag = {}", low_delay_mode_flag);

  return true;
}

json SeqDecoderModelInfo::to_json() const {
  return json{{"decoder_buffer_delay", decoder_buffer_delay},
              {"encoder_buffer_delay", encoder_buffer_delay},
              {"low_delay_mode_flag", low_delay_mode_flag}};
}

// ==================== SequencePartitionConfig ====================

bool SequencePartitionConfig::parse(BitstreamReader& br, bool single_picture_header_flag,
                                    bool Monochrome) {
  spdlog::debug("Parsing sequence_partition_config");

  use_256x256_superblock = br.read_bit();
  if (!use_256x256_superblock) {
    use_128x128_superblock = br.read_bit();
  } else {
    use_128x128_superblock = 0;
  }

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

  enable_ext_partitions = br.read_bit();
  if (enable_ext_partitions) {
    enable_uneven_4way_partitions = br.read_bit();
  } else {
    enable_uneven_4way_partitions = 0;
  }

  reduce_pb_aspect_ratio = br.read_bit();
  if (reduce_pb_aspect_ratio) {
    max_pb_aspect_ratio_log2_minus1 = br.read_bit();
    MaxPbAspectRatio = 1 << (max_pb_aspect_ratio_log2_minus1 + 1);
  } else {
    MaxPbAspectRatio = 8;
  }

  spdlog::debug("  use_256x256_superblock = {}", use_256x256_superblock);
  spdlog::debug("  use_128x128_superblock = {}", use_128x128_superblock);

  return true;
}

json SequencePartitionConfig::to_json() const {
  return json{{"use_256x256_superblock", use_256x256_superblock},
              {"use_128x128_superblock", use_128x128_superblock},
              {"enable_sdp", enable_sdp},
              {"enable_extended_sdp", enable_extended_sdp},
              {"enable_ext_partitions", enable_ext_partitions},
              {"enable_uneven_4way_partitions", enable_uneven_4way_partitions},
              {"reduce_pb_aspect_ratio", reduce_pb_aspect_ratio},
              {"MaxPbAspectRatio", MaxPbAspectRatio}};
}

// ==================== seg_info() ====================

// Segmentation feature tables
static constexpr uint32_t Segmentation_Feature_Bits[SEG_LVL_MAX] = {9, 0, 0};
static constexpr uint32_t Segmentation_Feature_Signed[SEG_LVL_MAX] = {1, 0, 0};

// Parse seg_info(numSegments) — reads per-segment feature enable flags and values
static void parse_seg_info(BitstreamReader& br, uint32_t numSegments) {
  for (uint32_t i = 0; i < numSegments; i++) {
    for (uint32_t j = 0; j < SEG_LVL_MAX; j++) {
      uint32_t feature_enabled = br.read_bit();
      if (feature_enabled && Segmentation_Feature_Bits[j] > 0) {
        if (Segmentation_Feature_Signed[j]) {
          br.read_su(1 + Segmentation_Feature_Bits[j]);
        } else {
          br.read_bits(Segmentation_Feature_Bits[j]);
        }
      }
    }
  }
}

// ==================== SequenceSegmentConfig ====================

bool SequenceSegmentConfig::parse(BitstreamReader& br) {
  spdlog::debug("Parsing sequence_segment_config");

  enable_ext_seg = br.read_bit();
  MaxSegments = enable_ext_seg ? 16 : 8;

  seq_seg_info_present_flag = br.read_bit();
  if (seq_seg_info_present_flag) {
    seq_allow_seg_info_change = br.read_bit();
    parse_seg_info(br, MaxSegments);
  }

  spdlog::debug("  enable_ext_seg = {}, MaxSegments = {}", enable_ext_seg, MaxSegments);

  return true;
}

json SequenceSegmentConfig::to_json() const {
  return json{{"enable_ext_seg", enable_ext_seg},
              {"MaxSegments", MaxSegments},
              {"seq_seg_info_present_flag", seq_seg_info_present_flag},
              {"seq_allow_seg_info_change", seq_allow_seg_info_change}};
}

// ==================== SequenceIntraConfig ====================

bool SequenceIntraConfig::parse(BitstreamReader& br, bool Monochrome) {
  spdlog::debug("Parsing sequence_intra_config");

  enable_dip = br.read_bit();
  enable_intra_edge_filter = br.read_bit();
  enable_mrls = br.read_bit();
  enable_cfl_intra = br.read_bit();

  if (Monochrome) {
    cfl_ds_filter_index = 0;
  } else {
    cfl_ds_filter_index = br.read_bits(2);
  }

  enable_mhccp = br.read_bit();
  enable_ibp = br.read_bit();

  return true;
}

json SequenceIntraConfig::to_json() const {
  return json{{"enable_dip", enable_dip},
              {"enable_intra_edge_filter", enable_intra_edge_filter},
              {"enable_mrls", enable_mrls},
              {"enable_cfl_intra", enable_cfl_intra},
              {"enable_mhccp", enable_mhccp},
              {"enable_ibp", enable_ibp},
              {"cfl_ds_filter_index", cfl_ds_filter_index}};
}

// ==================== SequenceInterConfig ====================

bool SequenceInterConfig::parse(BitstreamReader& br, bool single_picture_header_flag) {
  spdlog::debug("Parsing sequence_inter_config (single_picture_header={})",
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
    enable_mv_traj = 0;
    enable_imp_msk_bld = 0;
    long_term_frame_id_bits = 0;

    enable_refmvbank = br.read_bit();
    disable_drl_reorder = br.read_bit();

    if (disable_drl_reorder) {
      DrlReorder = DRL_REORDER_DISABLED;
    } else {
      constrain_drl_reorder = br.read_bit();
      DrlReorder = constrain_drl_reorder ? DRL_REORDER_CONSTRAINT : DRL_REORDER_ALWAYS;
    }

    seq_max_bvp_drl_bits_minus1 = br.read_ns(MAX_REF_BV_STACK_SIZE - 1);
    allow_frame_max_bvp_drl_bits = br.read_bit();

    enable_bawp = br.read_bit();

    NumRefFrames = 2;
    ActiveNumRefFrames = std::min(REFS_PER_FRAME, NumRefFrames);
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

    order_hint_bits_minus_1 = br.read_bits(4);
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
    explicit_num_ref_frames = br.read_bit();

    if (explicit_num_ref_frames) {
      num_ref_frames_minus_1 = br.read_bits(4);
      NumRefFrames = num_ref_frames_minus_1 + 1;
    } else {
      NumRefFrames = 8;
    }

    ActiveNumRefFrames = std::min(REFS_PER_FRAME, NumRefFrames);

    long_term_frame_id_bits = br.read_bits(3);

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

    enable_lf_sub_pu = br.read_bit();

    if (EnableTipOutput && enable_lf_sub_pu) {
      enable_tip_explicit_qp = br.read_bit();
    } else {
      enable_tip_explicit_qp = 0;
    }

    enable_opfl_refine = br.read_bits(2);
    enable_refinemv = br.read_bit();

    if (enable_tip && (enable_opfl_refine != 0 || enable_refinemv)) {
      enable_tip_refinemv = br.read_bit();
    } else {
      enable_tip_refinemv = 0;
    }

    enable_bru = br.read_bit();
    enable_adaptive_mvd = br.read_bit();
    enable_mvd_sign_derive = br.read_bit();
    enable_flex_mvres = br.read_bit();

    enable_global_motion = br.read_bit();

    enable_short_refresh_frame_flags = br.read_bit();
  }

  spdlog::debug("  NumRefFrames = {}, OrderHintBits = {}", NumRefFrames, OrderHintBits);

  return true;
}

json SequenceInterConfig::to_json() const {
  json j = {{"seq_enabled_motion_modes", seq_enabled_motion_modes},
            {"seq_frame_motion_modes_present_flag", seq_frame_motion_modes_present_flag},
            {"enable_six_param_warp_delta", enable_six_param_warp_delta},
            {"enable_masked_compound", enable_masked_compound},
            {"enable_ref_frame_mvs", enable_ref_frame_mvs},
            {"reduced_ref_frame_mvs_mode", reduced_ref_frame_mvs_mode},
            {"OrderHintBits", OrderHintBits},
            {"enable_refmvbank", enable_refmvbank},
            {"disable_drl_reorder", disable_drl_reorder},
            {"DrlReorder", DrlReorder},
            {"explicit_ref_frame_map", explicit_ref_frame_map},
            {"NumRefFrames", NumRefFrames},
            {"ActiveNumRefFrames", ActiveNumRefFrames},
            {"long_term_frame_id_bits", long_term_frame_id_bits},
            {"seq_max_drl_bits_minus1", seq_max_drl_bits_minus1},
            {"allow_frame_max_drl_bits", allow_frame_max_drl_bits},
            {"seq_max_bvp_drl_bits_minus1", seq_max_bvp_drl_bits_minus1},
            {"allow_frame_max_bvp_drl_bits", allow_frame_max_bvp_drl_bits},
            {"num_same_ref_compound", num_same_ref_compound},
            {"enable_tip", enable_tip},
            {"enable_tip_hole_fill", enable_tip_hole_fill},
            {"enable_mv_traj", enable_mv_traj},
            {"enable_bawp", enable_bawp},
            {"enable_cwp", enable_cwp},
            {"enable_imp_msk_bld", enable_imp_msk_bld},
            {"enable_lf_sub_pu", enable_lf_sub_pu},
            {"enable_tip_explicit_qp", enable_tip_explicit_qp},
            {"enable_opfl_refine", enable_opfl_refine},
            {"enable_adaptive_mvd", enable_adaptive_mvd},
            {"enable_refinemv", enable_refinemv},
            {"enable_tip_refinemv", enable_tip_refinemv},
            {"enable_bru", enable_bru},
            {"enable_mvd_sign_derive", enable_mvd_sign_derive},
            {"enable_flex_mvres", enable_flex_mvres},
            {"enable_global_motion", enable_global_motion},
            {"enable_short_refresh_frame_flags", enable_short_refresh_frame_flags}};

  return j;
}

// ==================== SequenceSCCConfig ====================

bool SequenceSCCConfig::parse(BitstreamReader& br, bool single_picture_header_flag) {
  spdlog::debug("Parsing sequence_scc_config");

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

  spdlog::debug("  seq_force_screen_content_tools = {}", seq_force_screen_content_tools);
  spdlog::debug("  seq_force_integer_mv = {}", seq_force_integer_mv);

  return true;
}

json SequenceSCCConfig::to_json() const {
  return json{{"seq_choose_screen_content_tools", seq_choose_screen_content_tools},
              {"seq_force_screen_content_tools", seq_force_screen_content_tools},
              {"seq_choose_integer_mv", seq_choose_integer_mv},
              {"seq_force_integer_mv", seq_force_integer_mv}};
}

// ==================== SequenceTransformQuantEntropyConfig ====================

bool SequenceTransformQuantEntropyConfig::parse(BitstreamReader& br,
                                                bool single_picture_header_flag, bool Monochrome) {
  spdlog::debug("Parsing sequence_transform_quant_entropy_config");

  enable_fsc = br.read_bit();

  if (enable_fsc) {
    enable_idtx_intra = 1;
  } else {
    enable_idtx_intra = br.read_bit();
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
  } else {
    enable_inter_ddt = 0;
  }

  reduced_tx_part_set = br.read_bit();

  if (Monochrome) {
    enable_cctx = 0;
  } else {
    enable_cctx = br.read_bit();
  }

  enable_tcq = br.read_bit();
  if (enable_tcq && !single_picture_header_flag) {
    choose_tcq_per_frame = br.read_bit();
  } else {
    choose_tcq_per_frame = 0;
  }

  if (enable_tcq && !choose_tcq_per_frame) {
    enable_parity_hiding = 0;
  } else {
    enable_parity_hiding = br.read_bit();
  }

  if (single_picture_header_flag) {
    enable_avg_cdf = 1;
    avg_cdf_type = 1;
  } else {
    enable_avg_cdf = br.read_bit();
    if (enable_avg_cdf) {
      avg_cdf_type = br.read_bit();
    }
  }

  // Delta Q configuration
  if (Monochrome) {
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

  if (!Monochrome) {
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

  return true;
}

json SequenceTransformQuantEntropyConfig::to_json() const {
  return json{{"enable_fsc", enable_fsc},
              {"enable_idtx_intra", enable_idtx_intra},
              {"enable_intra_ist", enable_intra_ist},
              {"enable_inter_ist", enable_inter_ist},
              {"enable_chroma_dctonly", enable_chroma_dctonly},
              {"enable_inter_ddt", enable_inter_ddt},
              {"reduced_tx_part_set", reduced_tx_part_set},
              {"enable_cctx", enable_cctx},
              {"enable_tcq", enable_tcq},
              {"choose_tcq_per_frame", choose_tcq_per_frame},
              {"enable_parity_hiding", enable_parity_hiding},
              {"enable_avg_cdf", enable_avg_cdf},
              {"avg_cdf_type", avg_cdf_type},
              {"separate_uv_delta_q", separate_uv_delta_q},
              {"equal_ac_dc_q", equal_ac_dc_q},
              {"base_y_dc_delta_q", base_y_dc_delta_q},
              {"y_dc_delta_q_enabled", y_dc_delta_q_enabled},
              {"base_uv_dc_delta_q", base_uv_dc_delta_q},
              {"uv_dc_delta_q_enabled", uv_dc_delta_q_enabled},
              {"base_uv_ac_delta_q", base_uv_ac_delta_q},
              {"uv_ac_delta_q_enabled", uv_ac_delta_q_enabled},
              {"BaseYDcDeltaQ", BaseYDcDeltaQ},
              {"BaseUVDcDeltaQ", BaseUVDcDeltaQ},
              {"BaseUVAcDeltaQ", BaseUVAcDeltaQ}};
}

// ==================== SequenceFilterConfig ====================

bool SequenceFilterConfig::parse(BitstreamReader& br, bool single_picture_header_flag,
                                 bool /* Monochrome */, BlockSize seq_sb_size) {
  spdlog::debug("Parsing sequence_filter_config");

  disable_loopfilters_across_tiles = br.read_bit();
  enable_cdef = br.read_bit();
  enable_gdf = br.read_bit();

  if (enable_gdf && seq_sb_size == BLOCK_64X64) {
    gdf_unit_matches_sb_size = br.read_bit();
  } else {
    gdf_unit_matches_sb_size = 0;
  }

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
  if (enable_ccso) {
    ccso_unit_matches_sb_size = br.read_bit();
  } else {
    ccso_unit_matches_sb_size = 0;
  }

  // CDEF on skip transform
  if (single_picture_header_flag) {
    CdefOnSkipTxfm = CDEF_ON_SKIP_TXFM_ADAPTIVE;
  } else {
    cdef_on_skip_txfm_always_on = br.read_bit();
    if (cdef_on_skip_txfm_always_on) {
      CdefOnSkipTxfm = CDEF_ON_SKIP_TXFM_ALWAYS_ON;
    } else {
      cdef_on_skip_txfm_disabled = br.read_bit();
      CdefOnSkipTxfm =
        cdef_on_skip_txfm_disabled ? CDEF_ON_SKIP_TXFM_DISABLED : CDEF_ON_SKIP_TXFM_ADAPTIVE;
    }
  }

  df_par_bits_minus2 = br.read_bits(2);

  return true;
}

json SequenceFilterConfig::to_json() const {
  return json{{"disable_loopfilters_across_tiles", disable_loopfilters_across_tiles},
              {"enable_cdef", enable_cdef},
              {"enable_gdf", enable_gdf},
              {"gdf_unit_matches_sb_size", gdf_unit_matches_sb_size},
              {"enable_restoration", enable_restoration},
              {"enable_ccso", enable_ccso},
              {"ccso_unit_matches_sb_size", ccso_unit_matches_sb_size},
              {"CdefOnSkipTxfm", CdefOnSkipTxfm},
              {"df_par_bits_minus2", df_par_bits_minus2}};
}

// ==================== SequenceTileConfig ====================

// Parse tile_params() — computes tile grid and reads tile spacing syntax
static void parse_tile_params(BitstreamReader& br, uint32_t frameWidth, uint32_t frameHeight,
                              BlockSize uniformSbSize, BlockSize sbSize, bool isBridge,
                              uint32_t seq_level_idx, uint32_t seq_tier) {
  uint32_t miCols = 2 * ((frameWidth + 7) >> 3);
  uint32_t miRows = 2 * ((frameHeight + 7) >> 3);
  uint32_t sb4x4 = Num_4x4_Blocks_Wide[sbSize];
  uint32_t sbShift = Mi_Width_Log2[sbSize];
  uint32_t sbCols = (miCols + sb4x4 - 1) >> sbShift;
  uint32_t sbRows = (miRows + sb4x4 - 1) >> sbShift;

  uint32_t maxTileWidthSb, maxTileAreaSb;
  if (seq_level_idx != 31) {
    maxTileWidthSb = (Tile_Width_Scaling_Factor[seq_tier][seq_level_idx] * MAX_TILE_WIDTH) >>
                     (sbShift + 4);
    maxTileAreaSb = (Tile_Area_Scaling_Factor[seq_tier][seq_level_idx] * MAX_TILE_AREA) >>
                    (2 * (sbShift + 2) + 2);
  } else {
    maxTileWidthSb = sbCols;
    maxTileAreaSb = sbCols * sbRows;
  }

  uint32_t minLog2TileCols = tile_log2(maxTileWidthSb, sbCols);
  uint32_t maxLog2TileCols = tile_log2(1, Min(sbCols, MAX_TILE_COLS));
  uint32_t maxLog2TileRows = tile_log2(1, Min(sbRows, MAX_TILE_ROWS));
  uint32_t minLog2Tiles = Max(minLog2TileCols, tile_log2(maxTileAreaSb, sbRows * sbCols));

  uint32_t uniform_tile_spacing_flag;
  if (isBridge) {
    uniform_tile_spacing_flag = 1;
  } else {
    uniform_tile_spacing_flag = br.read_bit();
  }

  if (uniform_tile_spacing_flag) {
    uint32_t tileColsLog2 = minLog2TileCols;
    if (!isBridge) {
      while (tileColsLog2 < maxLog2TileCols) {
        uint32_t increment = br.read_bit();
        if (increment) {
          tileColsLog2++;
        } else {
          break;
        }
      }
    }
    // uniform_spacing() computes tile starts — we don't need the results,
    // just need to consume the right bits (already done above)

    // Compute tileCols for minLog2TileRows
    uint32_t uniformSbShift = Mi_Width_Log2[uniformSbSize];
    uint32_t uniformSb4x4 = Num_4x4_Blocks_Wide[uniformSbSize];
    uint32_t uniformSbs = (miCols + uniformSb4x4 - 1) >> uniformSbShift;
    uint32_t fullSbs = miCols >> uniformSbShift;
    uint32_t tileSb = (tileColsLog2 > 0) ? (fullSbs >> tileColsLog2) : fullSbs;
    uint32_t tileCols = (tileSb > 0) ? (1u << tileColsLog2) : uniformSbs;
    // Clamp
    if (tileCols == 0) tileCols = 1;
    uint32_t actualTileColsLog2 = tile_log2(1, tileCols);
    uint32_t minLog2TileRows = (minLog2Tiles > actualTileColsLog2)
                                   ? (minLog2Tiles - actualTileColsLog2)
                                   : 0;

    uint32_t tileRowsLog2 = minLog2TileRows;
    if (!isBridge) {
      while (tileRowsLog2 < maxLog2TileRows) {
        uint32_t increment = br.read_bit();
        if (increment) {
          tileRowsLog2++;
        } else {
          break;
        }
      }
    }
  } else {
    // Non-uniform tile spacing — explicit tile sizes
    uint32_t widestTileSb = 1;
    uint32_t startSb = 0;
    uint32_t tileCols = 0;
    while (startSb < sbCols) {
      uint32_t n = Min(sbCols - startSb, maxTileWidthSb);
      uint32_t width_in_sbs_minus_1 = br.read_ns(n);
      uint32_t sizeSb = width_in_sbs_minus_1 + 1;
      widestTileSb = Max(sizeSb, widestTileSb);
      startSb += sizeSb;
      tileCols++;
    }
    (void)tile_log2(1, tileCols);  // tileColsLog2 used later in non-uniform row height calc

    uint32_t maxTileHeightSb;
    if (minLog2Tiles > 0) {
      maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
    }
    maxTileHeightSb = Max(maxTileAreaSb / widestTileSb, 1u);

    startSb = 0;
    while (startSb < sbRows) {
      uint32_t maxHeight = Min(sbRows - startSb, maxTileHeightSb);
      uint32_t height_in_sbs_minus_1 = br.read_ns(maxHeight);
      startSb += height_in_sbs_minus_1 + 1;
    }
  }
}

bool SequenceTileConfig::parse(BitstreamReader& br, uint32_t frameWidth, uint32_t frameHeight,
                               bool use_256x256_superblock, bool use_128x128_superblock,
                               uint32_t seq_level_idx, uint32_t seq_tier) {
  spdlog::debug("Parsing sequence_tile_config");

  seq_tile_info_present_flag = br.read_bit();
  if (seq_tile_info_present_flag) {
    allow_tile_info_change = br.read_bit();
    BlockSize seqSbSize = get_seq_sb_size(use_256x256_superblock, use_128x128_superblock);
    parse_tile_params(br, frameWidth, frameHeight, seqSbSize, seqSbSize, false, seq_level_idx,
                      seq_tier);
  }

  return true;
}

json SequenceTileConfig::to_json() const {
  return json{{"seq_tile_info_present_flag", seq_tile_info_present_flag},
              {"allow_tile_info_change", allow_tile_info_change}};
}

// ==================== AV2SequenceHeader Helpers ====================

BlockSize AV2SequenceHeader::get_seq_sb_size() const {
  if (partition_config.use_256x256_superblock)
    return BLOCK_256X256;
  if (partition_config.use_128x128_superblock)
    return BLOCK_128X128;
  return BLOCK_64X64;
}

void AV2SequenceHeader::set_chroma_format_and_bit_depth() {
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

  BitDepth = (bit_depth_idc == 0) ? 10 : ((bit_depth_idc == 1) ? 8 : 12);
  MaxQ = (BitDepth == 8) ? MAXQ_8_BITS : ((BitDepth == 10) ? MAXQ_10_BITS : MAXQ_12_BITS);

  Monochrome = (chroma_format_idc == CHROMA_FORMAT_400);
  NumPlanes = Monochrome ? 1 : 3;
}

// ==================== AV2SequenceHeader (Main) ====================

bool AV2SequenceHeader::parse(BitstreamReader& br) {
  spdlog::debug("Parsing AV2 Sequence Header");

  seq_header_id = br.read_uvlc();
  seq_profile_idc = br.read_bits(5);
  single_picture_header_flag = br.read_bit();

  seq_level_idx = br.read_bits(5);
  if (seq_level_idx > 3 && !single_picture_header_flag) {
    seq_tier = br.read_bit();
  } else {
    seq_tier = 0;
  }

  chroma_format_idc = br.read_uvlc();
  bit_depth_idc = br.read_uvlc();
  set_chroma_format_and_bit_depth();

  spdlog::debug("  seq_header_id = {}", seq_header_id);
  spdlog::debug("  seq_profile_idc = {}", seq_profile_idc);
  spdlog::debug("  single_picture_header_flag = {}", single_picture_header_flag);
  spdlog::debug("  chroma_format_idc = {}, bit_depth_idc = {}, BitDepth = {}", chroma_format_idc,
                bit_depth_idc, BitDepth);

  if (single_picture_header_flag) {
    seq_lcr_id = 0;
    still_picture = 1;
    max_tlayer_id = 0;
    max_mlayer_id = 0;
    SeqMaxMlayerCnt = 1;
    monotonic_output_order_flag = 1;
  } else {
    seq_lcr_id = br.read_bits(3);
    still_picture = br.read_bit();
    max_tlayer_id = br.read_bits(2);
    max_mlayer_id = br.read_bits(3);

    if (max_mlayer_id > 0) {
      // CeilLog2(max_mlayer_id + 1)
      uint32_t val = max_mlayer_id + 1;
      uint32_t n = 0;
      uint32_t tmp = val;
      while (tmp > 1) {
        tmp = (tmp + 1) >> 1;
        n++;
      }
      seq_max_mlayer_cnt_minus_1 = br.read_bits(n);
      SeqMaxMlayerCnt = seq_max_mlayer_cnt_minus_1 + 1;
    } else {
      SeqMaxMlayerCnt = 1;
    }

    monotonic_output_order_flag = br.read_bit();
  }

  // Frame dimensions
  frame_width_bits_minus_1 = br.read_bits(4);
  frame_height_bits_minus_1 = br.read_bits(4);

  uint32_t n = frame_width_bits_minus_1 + 1;
  max_frame_width_minus_1 = br.read_bits(n);

  n = frame_height_bits_minus_1 + 1;
  max_frame_height_minus_1 = br.read_bits(n);

  spdlog::debug("  max_frame_width = {}", max_frame_width_minus_1 + 1);
  spdlog::debug("  max_frame_height = {}", max_frame_height_minus_1 + 1);

  // Cropping window
  seq_cropping_window_present_flag = br.read_bit();
  if (seq_cropping_window_present_flag) {
    seq_cropping_win_left_offset = br.read_uvlc();
    seq_cropping_win_right_offset = br.read_uvlc();
    seq_cropping_win_top_offset = br.read_uvlc();
    seq_cropping_win_bottom_offset = br.read_uvlc();
  } else {
    seq_cropping_win_left_offset = 0;
    seq_cropping_win_right_offset = 0;
    seq_cropping_win_top_offset = 0;
    seq_cropping_win_bottom_offset = 0;
  }

  // Decoder model
  if (single_picture_header_flag) {
    decoder_model_info_present_flag = 0;
  } else {
    seq_initial_display_delay_present_flag = br.read_bit();
    if (seq_initial_display_delay_present_flag) {
      seq_initial_display_delay_minus_1 = br.read_bits(4);
    }

    decoder_model_info_present_flag = br.read_bit();
    if (decoder_model_info_present_flag) {
      num_units_in_decoding_tick = br.read_bits(32);
      seq_decoder_model_info_present_flag = br.read_bit();
      if (seq_decoder_model_info_present_flag) {
        if (!seq_decoder_model_info.parse(br))
          return false;
      }
    }
  }

  // Initialize default TLayerDependencyMap
  for (uint32_t mLayer = 0; mLayer < MAX_NUM_MLAYERS; mLayer++) {
    for (uint32_t currTLayer = 0; currTLayer < MAX_NUM_TLAYERS; currTLayer++) {
      for (uint32_t refTLayer = 0; refTLayer < MAX_NUM_TLAYERS; refTLayer++) {
        TLayerDependencyMap[mLayer][currTLayer][refTLayer] =
          (refTLayer <= currTLayer && currTLayer <= max_tlayer_id && mLayer <= max_mlayer_id) ? 1
                                                                                             : 0;
      }
    }
  }

  // Initialize default MLayerDependencyMap
  for (uint32_t currLayer = 0; currLayer < MAX_NUM_MLAYERS; currLayer++) {
    for (uint32_t refLayer = 0; refLayer < MAX_NUM_MLAYERS; refLayer++) {
      MLayerDependencyMap[currLayer][refLayer] =
        (refLayer <= currLayer && currLayer <= max_mlayer_id) ? 1 : 0;
    }
  }

  // Parse mlayer_dependency_map
  if (max_mlayer_id > 0) {
    mlayer_dependency_present_flag = br.read_bit();
    if (mlayer_dependency_present_flag) {
      for (uint32_t currLayer = 1; currLayer <= max_mlayer_id; currLayer++) {
        for (int32_t refLayer = static_cast<int32_t>(currLayer); refLayer >= 0; refLayer--) {
          MLayerDependencyMap[currLayer][refLayer] = br.read_bit();
        }
      }
    }
  }

  // Parse tlayer_dependency_map
  if (max_tlayer_id > 0) {
    tlayer_dependency_present_flag = br.read_bit();
    if (tlayer_dependency_present_flag) {
      if (max_mlayer_id > 0) {
        multi_tlayer_dependency_map_present_flag = br.read_bit();
      } else {
        multi_tlayer_dependency_map_present_flag = 0;
      }

      for (uint32_t mLayer = 0; mLayer <= max_mlayer_id; mLayer++) {
        for (uint32_t currTLayer = 1; currTLayer <= max_tlayer_id; currTLayer++) {
          for (int32_t refTLayer = static_cast<int32_t>(currTLayer); refTLayer >= 0; refTLayer--) {
            if (multi_tlayer_dependency_map_present_flag > 0 || mLayer == 0) {
              TLayerDependencyMap[mLayer][currTLayer][refTLayer] = br.read_bit();
            } else {
              TLayerDependencyMap[mLayer][currTLayer][refTLayer] =
                TLayerDependencyMap[0][currTLayer][refTLayer];
            }
          }
        }
      }
    }
  }

  // Compute MLayerPresenceMap
  for (uint32_t mlayerId = 0; mlayerId < MAX_NUM_MLAYERS; mlayerId++) {
    for (uint32_t refMlayer = 0; refMlayer < MAX_NUM_MLAYERS; refMlayer++) {
      MLayerPresenceMap[mlayerId][refMlayer] = 0;
      if (mlayerId == refMlayer || MLayerDependencyMap[mlayerId][refMlayer]) {
        MLayerPresenceMap[mlayerId][refMlayer] = 1;
        for (uint32_t depMLayerId = 0; depMLayerId < refMlayer; depMLayerId++) {
          MLayerPresenceMap[mlayerId][depMLayerId] |= MLayerPresenceMap[refMlayer][depMLayerId];
        }
      }
    }
  }

  // Sub-configs
  if (!partition_config.parse(br, single_picture_header_flag, Monochrome))
    return false;
  if (!segment_config.parse(br))
    return false;
  if (!intra_config.parse(br, Monochrome))
    return false;
  if (!inter_config.parse(br, single_picture_header_flag))
    return false;
  if (!scc_config.parse(br, single_picture_header_flag))
    return false;
  if (!tqe_config.parse(br, single_picture_header_flag, Monochrome))
    return false;
  if (!filter_config.parse(br, single_picture_header_flag, Monochrome, get_seq_sb_size()))
    return false;
  if (!tile_config.parse(br, static_cast<uint32_t>(max_frame_width_minus_1) + 1,
                         static_cast<uint32_t>(max_frame_height_minus_1) + 1,
                         partition_config.use_256x256_superblock,
                         partition_config.use_128x128_superblock, seq_level_idx, seq_tier))
    return false;

  film_grain_params_present = br.read_bit();

  // NOTE: obu_extension_flag is NOT part of sequence_header_obu() in the spec.
  // It belongs to obu_payload() and is parsed by BaseOBU::parse_obu_trailing_bits().
  // We should fix this: https://github.com/AOMediaCodec/av2-spec-internal/issues/488

  spdlog::debug("Successfully parsed AV2 Sequence Header");
  return true;
}

json AV2SequenceHeader::to_json() const {
  json j = {{"seq_header_id", seq_header_id},
            {"seq_profile_idc", seq_profile_idc},
            {"single_picture_header_flag", single_picture_header_flag},
            {"seq_level_idx", seq_level_idx},
            {"seq_tier", seq_tier},
            {"chroma_format_idc", chroma_format_idc},
            {"bit_depth_idc", bit_depth_idc},
            {"BitDepth", BitDepth},
            {"Monochrome", Monochrome},
            {"NumPlanes", NumPlanes},
            {"seq_lcr_id", seq_lcr_id},
            {"still_picture", still_picture},
            {"max_tlayer_id", max_tlayer_id},
            {"max_mlayer_id", max_mlayer_id},
            {"SeqMaxMlayerCnt", SeqMaxMlayerCnt},
            {"monotonic_output_order_flag", monotonic_output_order_flag},
            {"max_frame_width", max_frame_width_minus_1 + 1},
            {"max_frame_height", max_frame_height_minus_1 + 1},
            {"decoder_model_info_present_flag", decoder_model_info_present_flag},
            {"partition_config", partition_config.to_json()},
            {"segment_config", segment_config.to_json()},
            {"intra_config", intra_config.to_json()},
            {"inter_config", inter_config.to_json()},
            {"scc_config", scc_config.to_json()},
            {"tqe_config", tqe_config.to_json()},
            {"filter_config", filter_config.to_json()},
            {"tile_config", tile_config.to_json()},
            {"film_grain_params_present", film_grain_params_present}};

  if (seq_cropping_window_present_flag) {
    j["cropping_window"] = {{"left", seq_cropping_win_left_offset},
                            {"right", seq_cropping_win_right_offset},
                            {"top", seq_cropping_win_top_offset},
                            {"bottom", seq_cropping_win_bottom_offset}};
  }

  if (seq_decoder_model_info_present_flag) {
    j["seq_decoder_model_info"] = seq_decoder_model_info.to_json();
  }

  if (seq_initial_display_delay_present_flag) {
    j["seq_initial_display_delay_minus_1"] = seq_initial_display_delay_minus_1;
  }

  if (decoder_model_info_present_flag) {
    j["num_units_in_decoding_tick"] = num_units_in_decoding_tick;
  }

  return j;
}

}  // namespace av2_obu
