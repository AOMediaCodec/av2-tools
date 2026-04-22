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

#include <cstdint>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_reader.h>

namespace av2_obu {

using json = nlohmann::ordered_json;

// Timing info
struct TimingInfo {
  uint32_t num_units_in_display_tick = 0;
  uint32_t time_scale = 0;
  uint32_t equal_picture_interval = 0;
  uint64_t num_ticks_per_picture_minus_1 = 0;

  bool parse(BitstreamReader& br);
  json to_json() const;
};

// Sequence decoder model info
struct SeqDecoderModelInfo {
  uint64_t decoder_buffer_delay = 0;
  uint64_t encoder_buffer_delay = 0;
  uint32_t low_delay_mode_flag = 0;

  bool parse(BitstreamReader& br);
  json to_json() const;
};

// Sequence partition config
struct SequencePartitionConfig {
  uint32_t use_256x256_superblock = 0;
  uint32_t use_128x128_superblock = 0;
  uint32_t enable_sdp = 0;
  uint32_t enable_extended_sdp = 0;
  uint32_t enable_ext_partitions = 0;
  uint32_t enable_uneven_4way_partitions = 0;
  uint32_t reduce_pb_aspect_ratio = 0;
  uint32_t max_pb_aspect_ratio_log2_minus1 = 0;

  // Computed values
  uint32_t MaxPbAspectRatio = 8;

  bool parse(BitstreamReader& br, bool single_picture_header_flag, bool Monochrome);
  json to_json() const;
};

// Sequence segment config
struct SequenceSegmentConfig {
  uint32_t enable_ext_seg = 0;
  uint32_t seq_seg_info_present_flag = 0;
  uint32_t seq_allow_seg_info_change = 0;

  // Computed values
  uint32_t MaxSegments = 8;

  bool parse(BitstreamReader& br);
  json to_json() const;
};

// Sequence intra config
struct SequenceIntraConfig {
  uint32_t enable_dip = 0;
  uint32_t enable_intra_edge_filter = 0;
  uint32_t enable_mrls = 0;
  uint32_t enable_cfl_intra = 0;
  uint32_t enable_mhccp = 0;
  uint32_t enable_ibp = 0;
  uint32_t cfl_ds_filter_index = 0;

  bool parse(BitstreamReader& br, bool Monochrome);
  json to_json() const;
};

// Sequence inter config
struct SequenceInterConfig {
  std::vector<uint32_t> seq_enabled_motion_modes;
  uint32_t seq_frame_motion_modes_present_flag = 0;
  uint32_t enable_six_param_warp_delta = 0;
  uint32_t enable_masked_compound = 0;
  uint32_t enable_ref_frame_mvs = 0;
  uint32_t reduced_ref_frame_mvs_mode = 0;
  uint32_t order_hint_bits_minus_1 = 0;
  uint32_t enable_refmvbank = 0;
  uint32_t disable_drl_reorder = 0;
  uint32_t constrain_drl_reorder = 0;
  uint32_t explicit_ref_frame_map = 0;
  uint32_t explicit_num_ref_frames = 0;
  uint32_t num_ref_frames_minus_1 = 0;
  uint32_t seq_max_drl_bits_minus1 = 0;
  uint32_t allow_frame_max_drl_bits = 0;
  uint32_t seq_max_bvp_drl_bits_minus1 = 0;
  uint32_t allow_frame_max_bvp_drl_bits = 0;
  uint32_t num_same_ref_compound = 0;
  uint32_t enable_tip = 0;
  uint32_t disable_tip_output = 0;
  uint32_t enable_tip_hole_fill = 0;
  uint32_t enable_mv_traj = 0;
  uint32_t enable_bawp = 0;
  uint32_t enable_cwp = 0;
  uint32_t enable_imp_msk_bld = 0;
  uint32_t enable_lf_sub_pu = 0;
  uint32_t enable_tip_explicit_qp = 0;
  uint32_t enable_opfl_refine = 0;
  uint32_t enable_adaptive_mvd = 0;
  uint32_t enable_refinemv = 0;
  uint32_t enable_tip_refinemv = 0;
  uint32_t enable_bru = 0;
  uint32_t enable_mvd_sign_derive = 0;
  uint32_t enable_flex_mvres = 0;
  uint32_t enable_global_motion = 0;
  uint32_t enable_short_refresh_frame_flags = 0;
  uint32_t long_term_frame_id_bits = 0;

  // Computed values
  uint32_t OrderHintBits = 0;
  uint32_t DrlReorder = 0;
  uint32_t NumRefFrames = 0;
  uint32_t ActiveNumRefFrames = 0;
  bool EnableTipOutput = false;

  bool parse(BitstreamReader& br, bool single_picture_header_flag);
  json to_json() const;
};

// Sequence SCC config
struct SequenceSCCConfig {
  uint32_t seq_choose_screen_content_tools = 0;
  uint32_t seq_force_screen_content_tools = 0;
  uint32_t seq_choose_integer_mv = 0;
  uint32_t seq_force_integer_mv = 0;

  bool parse(BitstreamReader& br, bool single_picture_header_flag);
  json to_json() const;
};

// Sequence transform/quant/entropy config
struct SequenceTransformQuantEntropyConfig {
  uint32_t enable_fsc = 0;
  uint32_t enable_idtx_intra = 0;
  uint32_t enable_intra_ist = 0;
  uint32_t enable_inter_ist = 0;
  uint32_t enable_chroma_dctonly = 0;
  uint32_t enable_inter_ddt = 0;
  uint32_t reduced_tx_part_set = 0;
  uint32_t enable_cctx = 0;
  uint32_t enable_tcq = 0;
  uint32_t choose_tcq_per_frame = 0;
  uint32_t enable_parity_hiding = 0;
  uint32_t enable_avg_cdf = 0;
  uint32_t avg_cdf_type = 0;
  uint32_t separate_uv_delta_q = 0;
  uint32_t equal_ac_dc_q = 0;
  uint32_t base_y_dc_delta_q = 0;
  uint32_t y_dc_delta_q_enabled = 0;
  uint32_t base_uv_dc_delta_q = 0;
  uint32_t uv_dc_delta_q_enabled = 0;
  uint32_t base_uv_ac_delta_q = 0;
  uint32_t uv_ac_delta_q_enabled = 0;

  // Computed delta Q values
  int32_t BaseYDcDeltaQ = 0;
  int32_t BaseUVDcDeltaQ = 0;
  int32_t BaseUVAcDeltaQ = 0;

  bool parse(BitstreamReader& br, bool single_picture_header_flag, bool Monochrome);
  json to_json() const;
};

// Sequence filter config
struct SequenceFilterConfig {
  uint32_t disable_loopfilters_across_tiles = 0;
  uint32_t enable_cdef = 0;
  uint32_t enable_gdf = 0;
  uint32_t gdf_unit_matches_sb_size = 0;
  uint32_t enable_restoration = 0;
  std::vector<std::vector<uint32_t>> lr_tools_disable;  // [plane][tool]
  uint32_t lr_tools_uv_present = 0;
  uint32_t enable_ccso = 0;
  uint32_t ccso_unit_matches_sb_size = 0;
  uint32_t cdef_on_skip_txfm_always_on = 0;
  uint32_t cdef_on_skip_txfm_disabled = 0;
  uint32_t df_par_bits_minus2 = 0;

  // Computed values
  uint32_t CdefOnSkipTxfm = CDEF_ON_SKIP_TXFM_ADAPTIVE;

  bool parse(BitstreamReader& br, bool single_picture_header_flag, bool Monochrome,
             BlockSize seq_sb_size);
  json to_json() const;
};

// Sequence tile config
struct SequenceTileConfig {
  uint32_t seq_tile_info_present_flag = 0;
  uint32_t allow_tile_info_change = 0;

  bool parse(BitstreamReader& br, uint32_t frameWidth, uint32_t frameHeight,
             bool use_256x256_superblock, bool use_128x128_superblock, uint32_t seq_level_idx,
             uint32_t seq_tier);
  json to_json() const;
};

// Complete AV2 Sequence Header
struct AV2SequenceHeader {
  uint64_t seq_header_id = 0;
  uint32_t seq_profile_idc = 0;
  uint32_t single_picture_header_flag = 0;
  uint32_t seq_level_idx = 0;
  uint32_t seq_tier = 0;

  // Chroma/bit depth
  uint64_t chroma_format_idc = 0;
  uint64_t bit_depth_idc = 0;

  // Computed color values
  uint32_t SubsamplingX = 0;
  uint32_t SubsamplingY = 0;
  uint32_t BitDepth = 10;
  uint32_t MaxQ = MAXQ_10_BITS;
  bool Monochrome = false;
  uint32_t NumPlanes = 3;

  uint32_t seq_lcr_id = 0;
  uint32_t still_picture = 0;
  uint32_t max_tlayer_id = 0;
  uint32_t max_mlayer_id = 0;
  uint32_t seq_max_mlayer_cnt_minus_1 = 0;
  uint32_t SeqMaxMlayerCnt = 0;
  // Output order: when 1, output order equals decoding order for the coded video sequence
  uint32_t monotonic_output_order_flag = 0;

  uint32_t frame_width_bits_minus_1 = 0;
  uint32_t frame_height_bits_minus_1 = 0;
  uint64_t max_frame_width_minus_1 = 0;
  uint64_t max_frame_height_minus_1 = 0;

  uint32_t seq_cropping_window_present_flag = 0;
  uint64_t seq_cropping_win_left_offset = 0;
  uint64_t seq_cropping_win_right_offset = 0;
  uint64_t seq_cropping_win_top_offset = 0;
  uint64_t seq_cropping_win_bottom_offset = 0;

  // Decoder model
  uint32_t seq_initial_display_delay_present_flag = 0;
  uint32_t seq_initial_display_delay_minus_1 = 0;
  uint32_t decoder_model_info_present_flag = 0;
  uint32_t num_units_in_decoding_tick = 0;
  uint32_t seq_decoder_model_info_present_flag = 0;
  SeqDecoderModelInfo seq_decoder_model_info;

  // Layer dependency
  uint32_t mlayer_dependency_present_flag = 0;
  uint32_t tlayer_dependency_present_flag = 0;
  uint32_t multi_tlayer_dependency_map_present_flag = 0;
  uint32_t TLayerDependencyMap[MAX_NUM_MLAYERS][MAX_NUM_TLAYERS][MAX_NUM_TLAYERS] = {};
  uint32_t MLayerDependencyMap[MAX_NUM_MLAYERS][MAX_NUM_MLAYERS] = {};
  uint32_t MLayerPresenceMap[MAX_NUM_MLAYERS][MAX_NUM_MLAYERS] = {};

  // Sub-configs
  SequencePartitionConfig partition_config;
  SequenceSegmentConfig segment_config;
  SequenceIntraConfig intra_config;
  SequenceInterConfig inter_config;
  SequenceSCCConfig scc_config;
  SequenceTransformQuantEntropyConfig tqe_config;
  SequenceFilterConfig filter_config;
  SequenceTileConfig tile_config;

  uint32_t film_grain_params_present = 0;

  // NOTE: obu_extension_flag is parsed at the obu_payload() level by
  // BaseOBU::parse_obu_trailing_bits(), not as part of sequence_header_obu().
  // See: https://github.com/AOMediaCodec/av2-spec-internal/issues/488

  // Helper: get sequence superblock size as BlockSize enum (spec: get_seq_sb_size())
  BlockSize get_seq_sb_size() const;

  // Parse from bitstream
  bool parse(BitstreamReader& br);

  // Serialize to JSON
  json to_json() const;

private:
  void set_chroma_format_and_bit_depth();
};

}  // namespace av2_obu
