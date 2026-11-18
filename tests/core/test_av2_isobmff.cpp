/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License and the Alliance for
 * Open Media Patent License 1.0. If the BSD 3-Clause Clear License was not distributed with this
 * source code in the LICENSE file, you can obtain it at
 * aomedia.org/license/software-license/bsd-3-c-c/. If the Alliance for Open Media Patent
 * License 1.0 was not distributed with this source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_writer.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/obus/sequence_header_obu.h>

#include "av2_codec_config.h"
#include "av2_demuxer.h"
#include "av2_muxer.h"
#include "colr_info.h"
#include "mux_strategy.h"

namespace av2_obu {
namespace {

void write_uvlc(BitstreamWriter& bw, uint32_t value) {
  if (value == 0) {
    bw.write_bit(1);
    return;
  }
  uint32_t v = value + 1;
  uint32_t leading_zeros = 0;
  uint32_t tmp = v;
  while (tmp > 1) {
    tmp >>= 1;
    ++leading_zeros;
  }
  for (uint32_t i = 0; i < leading_zeros; ++i) {
    bw.write_bit(0);
  }
  bw.write_bit(1);
  bw.write_bits(v - (1u << leading_zeros), leading_zeros);
}

void write_rg(BitstreamWriter& bw, uint32_t value, uint32_t n) {
  uint32_t q = value >> n;
  uint32_t r = value & ((1u << n) - 1u);
  for (uint32_t i = 0; i < q; ++i) {
    bw.write_bit(1);
  }
  bw.write_bit(0);
  if (n > 0) {
    bw.write_bits(r, n);
  }
}

void append_non_extensible_trailing_bits(BitstreamWriter& bw) {
  bw.write_bit(1);  // trailing_one_bit
  bw.byte_align();
}

void append_extensible_trailing_bits(BitstreamWriter& bw) {
  bw.write_bit(0);  // obu_extension_flag = 0
  bw.write_bit(1);  // trailing_one_bit = 1
  bw.byte_align();
}

std::vector<uint8_t> wrap_annexb_obu(OBUType type, const std::vector<uint8_t>& payload,
                                     uint8_t tlayer_id = 0, uint8_t mlayer_id = 0,
                                     uint8_t xlayer_id = 0, bool force_ext = false) {
  const bool has_ext = force_ext || mlayer_id != 0 || xlayer_id != 0;
  const uint32_t obu_size = (has_ext ? 2u : 1u) + static_cast<uint32_t>(payload.size());

  std::vector<uint8_t> out;
  // Write LEB128 size
  uint32_t val = obu_size;
  do {
    uint8_t byte = val & 0x7F;
    val >>= 7;
    if (val != 0) byte |= 0x80;
    out.push_back(byte);
  } while (val != 0);

  uint8_t h0 = static_cast<uint8_t>(((has_ext ? 1 : 0) << 7) |
                                    ((static_cast<uint8_t>(type) & 0x1F) << 2) |
                                    (tlayer_id & 0x03));
  out.push_back(h0);
  if (has_ext) {
    uint8_t h1 = static_cast<uint8_t>(((mlayer_id & 0x07) << 5) | (xlayer_id & 0x1F));
    out.push_back(h1);
  }
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::vector<uint8_t> make_td_obu() {
  return wrap_annexb_obu(OBUType::TEMPORAL_DELIMITER, {}, 0, 0, GLOBAL_XLAYER_ID, true);
}

std::vector<uint8_t> make_padding_obu() {
  return wrap_annexb_obu(OBUType::PADDING, {0x00, 0x80}, 0, 0, 0, false);
}

std::vector<uint8_t> make_seq_header_obu(uint8_t xlayer_id = 0, uint32_t profile_idc = 0,
                                         uint32_t level_idx = 1, uint32_t tier = 0,
                                         uint32_t chroma_format_idc = CHROMA_FORMAT_420,
                                         uint32_t bit_depth_idc = 1,  // 1 = 8-bit, 0 = 10-bit
                                         uint32_t width = 176, uint32_t height = 144) {
  BitstreamWriter bw;
  write_uvlc(bw, 0);  // seq_header_id = 0
  bw.write_bits(profile_idc, 5);
  bw.write_bit(0);  // single_picture_header_flag = 0
  bw.write_bits(level_idx, 5);
  if (level_idx > 3) {
    bw.write_bit(tier);
  }
  write_uvlc(bw, chroma_format_idc);
  write_uvlc(bw, bit_depth_idc);

  const bool monochrome = (chroma_format_idc == CHROMA_FORMAT_400);

  bw.write_bits(0, 3);  // seq_lcr_id
  bw.write_bit(0);      // still_picture
  bw.write_bits(0, 2);  // max_tlayer_id
  bw.write_bits(0, 3);  // max_mlayer_id
  bw.write_bit(1);      // monotonic_output_order_flag

  bw.write_bits(7, 4);  // frame_width_bits_minus_1 = 7 (8 bits)
  bw.write_bits(7, 4);  // frame_height_bits_minus_1 = 7 (8 bits)
  bw.write_bits(width - 1, 8);
  bw.write_bits(height - 1, 8);

  bw.write_bit(0);  // seq_cropping_window_present_flag
  bw.write_bit(0);  // seq_initial_display_delay_present_flag
  bw.write_bit(0);  // decoder_model_info_present_flag

  // sequence_partition_config
  bw.write_bit(0);  // use_256x256_superblock
  bw.write_bit(1);  // use_128x128_superblock
  if (!monochrome) {
    bw.write_bit(0);  // enable_sdp
  }
  bw.write_bit(0);  // enable_ext_partitions
  bw.write_bit(0);  // reduce_pb_aspect_ratio

  // sequence_segment_config
  bw.write_bit(0);  // enable_ext_seg
  bw.write_bit(0);  // seq_seg_info_present_flag

  // sequence_intra_config
  bw.write_bit(0);  // enable_dip
  bw.write_bit(0);  // enable_intra_edge_filter
  bw.write_bit(0);  // enable_mrls
  bw.write_bit(0);  // enable_cfl_intra
  if (!monochrome) {
    bw.write_bits(0, 2);  // cfl_ds_filter_index
  }
  bw.write_bit(0);  // enable_mhccp
  bw.write_bit(0);  // enable_ibp

  // sequence_inter_config
  bw.write_bits(0, 4);  // seq_enabled_motion_modes[1..4]
  bw.write_bit(0);      // enable_masked_compound
  bw.write_bit(0);      // enable_ref_frame_mvs
  bw.write_bits(7, 4);  // order_hint_bits_minus_1 = 7 (OrderHintBits = 8)
  bw.write_bit(0);      // enable_refmvbank
  bw.write_bit(1);      // disable_drl_reorder
  bw.write_bit(1);      // explicit_ref_frame_map
  bw.write_bit(0);      // explicit_num_ref_frames (NumRefFrames = 8)
  bw.write_bits(0, 3);  // long_term_frame_id_bits
  bw.write_bits(0, 2);  // seq_max_drl_bits_minus_1 = ns(5) -> 0
  bw.write_bit(0);      // allow_frame_max_drl_bits
  bw.write_bits(0, 1);  // seq_max_bvp_drl_bits_minus_1 = ns(3) -> 0
  bw.write_bit(0);      // allow_frame_max_bvp_drl_bits
  bw.write_bits(0, 2);  // num_same_ref_compound
  bw.write_bit(0);      // enable_tip
  bw.write_bit(0);      // enable_mv_traj
  bw.write_bit(0);      // enable_bawp
  bw.write_bit(0);      // enable_cwp
  bw.write_bit(0);      // enable_imp_msk_bld
  bw.write_bit(0);      // enable_lf_sub_pu
  bw.write_bits(0, 2);  // enable_opfl_refine
  bw.write_bit(0);      // enable_refinemv
  bw.write_bit(0);      // enable_bru
  bw.write_bit(0);      // enable_adaptive_mvd
  bw.write_bit(0);      // enable_mvd_sign_derive
  bw.write_bit(0);      // enable_flex_mvres
  bw.write_bit(0);      // enable_global_motion
  bw.write_bit(0);      // enable_short_refresh_frame_flags

  // sequence_scc_config
  bw.write_bit(0);  // seq_choose_screen_content_tools
  bw.write_bit(0);  // seq_force_screen_content_tools

  // sequence_transform_quant_entropy_config
  bw.write_bit(1);  // enable_fsc = 1
  bw.write_bit(0);  // enable_intra_ist
  bw.write_bit(0);  // enable_inter_ist
  if (!monochrome) {
    bw.write_bit(0);  // enable_chroma_dctonly
  }
  bw.write_bit(0);  // enable_inter_ddt
  bw.write_bit(0);  // reduced_tx_part_set
  if (!monochrome) {
    bw.write_bit(0);  // enable_cctx
  }
  bw.write_bit(0);  // enable_tcq
  bw.write_bit(0);  // enable_parity_hiding
  bw.write_bit(0);  // enable_avg_cdf
  if (!monochrome) {
    bw.write_bit(0);  // separate_uv_delta_q
  }
  bw.write_bit(1);  // equal_ac_dc_q = 1
  if (!monochrome) {
    bw.write_bits(0, 5);  // base_uv_ac_delta_q
    bw.write_bit(0);      // uv_ac_delta_q_enabled
  }

  // sequence_filter_config
  bw.write_bit(0);      // disable_loopfilters_across_tiles
  bw.write_bit(0);      // enable_cdef
  bw.write_bit(0);      // enable_gdf
  bw.write_bit(0);      // enable_restoration
  bw.write_bit(0);      // enable_ccso
  bw.write_bit(1);      // cdef_on_skip_txfm_always_on
  bw.write_bits(0, 2);  // df_par_bits_minus_2

  // sequence_tile_config
  bw.write_bit(0);  // seq_tile_info_present_flag

  bw.write_bit(0);  // film_grain_params_present
  append_extensible_trailing_bits(bw);

  return wrap_annexb_obu(OBUType::SEQUENCE_HEADER, bw.bytes(), 0, 0, xlayer_id);
}

std::vector<uint8_t> make_ci_obu(uint8_t xlayer_id = 0, uint8_t cp = 1, uint8_t tc = 1,
                                 uint8_t mc = 1, bool full_range = false,
                                 bool has_csp = false, uint32_t csp = CSP_UNSPECIFIED,
                                 bool has_sar = false, uint32_t sar_w = 1, uint32_t sar_h = 1) {
  BitstreamWriter bw;
  bw.write_bits(1, 2);                // scan_type_idc = 1 (progressive)
  bw.write_bit(1);                    // color_description_present_flag = 1
  bw.write_bit(has_csp ? 1 : 0);      // chroma_sample_position_present_flag
  bw.write_bit(has_sar ? 1 : 0);      // aspect_ratio_info_present_flag
  bw.write_bit(0);                    // timing_info_present_flag = 0
  bw.write_bits(0, 2);                // reserved_2bit = 0

  write_rg(bw, 0, 2);                 // color_description_idc = 0 (explicit CICP)
  bw.write_bits(cp, 8);
  bw.write_bits(tc, 8);
  bw.write_bits(mc, 8);
  bw.write_bit(full_range ? 1 : 0);

  if (has_csp) {
    write_uvlc(bw, csp);
  }
  if (has_sar) {
    bw.write_bits(255, 8);  // aspect_ratio_idc = 255 (extended SAR)
    write_uvlc(bw, sar_w);
    write_uvlc(bw, sar_h);
  }
  append_extensible_trailing_bits(bw);
  return wrap_annexb_obu(OBUType::CONTENT_INTERPRETATION, bw.bytes(), 0, 0, xlayer_id);
}

std::vector<uint8_t> make_local_lcr_obu(uint8_t xlayer_id, uint32_t layer_type,
                                        uint32_t aux_type = 0) {
  BitstreamWriter bw;
  bw.write_bits(0, 3);  // lcr_global_id
  bw.write_bits(0, 3);  // lcr_local_id
  bw.write_bit(0);      // lcr_profile_tier_level_info_present_flag
  bw.write_bit(0);      // lcr_local_atlas_id_present_flag
  bw.write_bits(0, 3);  // lcr_local_reserved_zero_3bits
  bw.write_bits(0, 5);  // lcr_local_reserved_zero_5bits

  // lcr_xlayer_info
  bw.write_bit(0);  // rep_info_present
  bw.write_bit(0);  // purpose_present
  bw.write_bit(0);  // color_info_present
  bw.write_bit(1);  // embedded_layer_info_present
  bw.byte_align();

  // lcr_embedded_layer_info: mlayer_map = 0x01 (mlayer 0 present)
  bw.write_bits(0x01, 8);
  bw.write_bits(0x01, MAX_NUM_TLAYERS);  // tlayer_map
  bw.write_bits(layer_type, 8);          // layer_type (0=TEXTURE_LAYER, 1=AUX_LAYER)
  if (layer_type == AUX_LAYER) {
    bw.write_bits(aux_type, 8);          // auxiliary_type
  }
  bw.write_bits(VIEW_UNSPECIFIED, 8);    // view_type
  bw.write_bit(1);                       // same_sh_max_resolution = 1
  bw.byte_align();

  append_extensible_trailing_bits(bw);
  return wrap_annexb_obu(OBUType::LAYER_CONFIGURATION_RECORD, bw.bytes(), 0, 0, xlayer_id, true);
}

std::vector<uint8_t> make_msdo_obu(uint32_t num_streams = 2, uint32_t profile_idc = 3,
                                   uint32_t level_idx = 8, uint32_t tier = 1) {
  BitstreamWriter bw;
  bw.write_bits(num_streams - 2, 3);  // num_streams_minus_2
  bw.write_bits(profile_idc, 5);
  bw.write_bits(level_idx, 5);
  bw.write_bit(tier);
  bw.write_bit(1);  // multistream_even_allocation_flag = 1
  for (uint32_t i = 0; i < num_streams; ++i) {
    bw.write_bits(i, 5);            // sub_xlayer_id
    bw.write_bits(profile_idc, 5);  // sub_stream_max_profile
    bw.write_bits(level_idx, 5);    // sub_stream_max_level
    bw.write_bit(tier);             // sub_stream_max_tier
  }
  bw.write_bit(1);  // multistream_doh_constraint_flag
  append_non_extensible_trailing_bits(bw);
  return wrap_annexb_obu(OBUType::MSDO, bw.bytes(), 0, 0, GLOBAL_XLAYER_ID, true);
}

std::vector<uint8_t> make_metadata_short_obu(uint8_t xlayer_id, bool is_suffix) {
  BitstreamWriter bw;
  bw.write_bit(is_suffix ? 1 : 0);  // metadata_is_suffix
  // Short metadata unit header:
  // muh_type_Below16_flag = 1 (1 bit), muh_unit_type = 1 (HDR_CLL, 4 bits),
  // muh_layer_idc = 0 (LAYER_UNSPECIFIED, 2 bits), muh_cancellation_flag = 1 (1 bit)
  bw.write_bit(1);
  bw.write_bits(1, 4);
  bw.write_bits(0, 2);
  bw.write_bit(1);
  append_non_extensible_trailing_bits(bw);
  return wrap_annexb_obu(OBUType::METADATA, bw.bytes(), 0, 0, xlayer_id, xlayer_id != 0);
}

std::vector<uint8_t> make_metadata_group_obu(uint8_t xlayer_id, bool is_suffix) {
  BitstreamWriter bw;
  bw.write_bit(is_suffix ? 1 : 0);  // metadata_is_suffix
  bw.write_bits(0, 2);              // metadata_necessity_idc
  bw.write_bits(0, 5);              // metadata_application_id
  bw.write_bits(0, 8);              // metadata_unit_cnt_minus_1 = 0 (LEB128)
  // Group metadata unit header (cancelled unit):
  bw.write_bit(1);                  // muh_type_Below16_flag = 1
  bw.write_bits(1, 4);              // muh_unit_type = 1 (HDR_CLL)
  bw.write_bits(0, 2);              // muh_layer_idc = 0
  bw.write_bit(1);                  // muh_cancellation_flag = 1
  bw.write_bit(0);                  // mup_extension_flag = 0
  bw.write_bits(0, 2);              // muh_reserved_zero_2bits = 0
  append_non_extensible_trailing_bits(bw);
  return wrap_annexb_obu(OBUType::METADATA_GROUP, bw.bytes(), 0, 0, xlayer_id, xlayer_id != 0);
}

std::vector<uint8_t> make_clk_obu(uint8_t xlayer_id = 0, uint32_t order_hint = 0) {
  BitstreamWriter bw;
  bw.write_bit(1);      // is_first_tile_group = 1
  write_uvlc(bw, 0);    // cur_mfh_id = 0
  write_uvlc(bw, 0);    // seq_header_id_in_frame_header = 0
  bw.write_bit(1);      // immediate_output_frame = 1
  bw.write_bit(0);      // frame_size_override_flag = 0
  bw.write_bits(order_hint, 8);
  bw.byte_align();
  bw.write_bits(0xAA, 8);
  bw.write_bits(0x80, 8);
  return wrap_annexb_obu(OBUType::CLK, bw.bytes(), 0, 0, xlayer_id, xlayer_id != 0);
}

std::vector<uint8_t> make_inter_tile_group_obu(uint8_t xlayer_id = 0, uint32_t order_hint = 1) {
  BitstreamWriter bw;
  bw.write_bit(1);      // is_first_tile_group = 1
  write_uvlc(bw, 0);    // cur_mfh_id = 0
  write_uvlc(bw, 0);    // seq_header_id_in_frame_header = 0
  bw.write_bit(1);      // frame_is_inter = 1
  bw.write_bit(1);      // immediate_output_frame = 1
  bw.write_bit(0);      // frame_size_override_flag = 0
  bw.write_bits(order_hint, 8);
  bw.write_bit(0);      // signal_primary_ref_frame = 0
  bw.write_bit(0);      // disable_cross_frame_cdf_init = 0
  bw.write_bits(0x01, 8);  // refresh_frame_flags
  bw.write_bit(1);      // explicitRefFrameMap = 1
  bw.write_bits(1, 3);  // num_total_refs = 1
  bw.write_bits(0, 3);  // ref_frame_idx[0] = 0
  bw.byte_align();
  bw.write_bits(0x55, 8);
  bw.write_bits(0x80, 8);
  return wrap_annexb_obu(OBUType::REGULAR_TILE_GROUP, bw.bytes(), 0, 0, xlayer_id, xlayer_id != 0);
}

void write_bytes_to_file(const std::string& path, const std::vector<uint8_t>& data) {
  std::ofstream ofs(path, std::ios::binary);
  ASSERT_TRUE(ofs.is_open());
  ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::vector<uint8_t> read_bytes_from_file(const std::string& path) {
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs.is_open()) return {};
  const auto sz = ifs.tellg();
  ifs.seekg(0);
  std::vector<uint8_t> buf(static_cast<size_t>(sz));
  ifs.read(reinterpret_cast<char*>(buf.data()), sz);
  return buf;
}

bool contains_fourcc(const std::vector<uint8_t>& buf, const char fourcc[4]) {
  const uint8_t needle[4] = {static_cast<uint8_t>(fourcc[0]), static_cast<uint8_t>(fourcc[1]),
                             static_cast<uint8_t>(fourcc[2]), static_cast<uint8_t>(fourcc[3])};
  return std::search(buf.begin(), buf.end(), std::begin(needle), std::end(needle)) != buf.end();
}

// ============================================================================
// 1. OBU Classification & TemporalUnit Filtering Tests
// ============================================================================

TEST(AV2ISOBMFFTest, OBUClassificationHelpers) {
  // Config OBUs per av2-isobmff #av2c
  EXPECT_TRUE(is_config_obu(OBUType::MSDO));
  EXPECT_TRUE(is_config_obu(OBUType::LAYER_CONFIGURATION_RECORD));
  EXPECT_TRUE(is_config_obu(OBUType::OPERATING_POINT_SET));
  EXPECT_TRUE(is_config_obu(OBUType::ATLAS_SEGMENT));
  EXPECT_TRUE(is_config_obu(OBUType::SEQUENCE_HEADER));

  // Content Interpretation is NOT a config OBU in av2C; it is forbidden in ISOBMFF
  EXPECT_FALSE(is_config_obu(OBUType::CONTENT_INTERPRETATION));
  EXPECT_FALSE(is_config_obu(OBUType::TEMPORAL_DELIMITER));
  EXPECT_FALSE(is_config_obu(OBUType::PADDING));
  EXPECT_FALSE(is_config_obu(OBUType::CLK));
  EXPECT_FALSE(is_config_obu(OBUType::METADATA));
  EXPECT_FALSE(is_config_obu(OBUType::METADATA_GROUP));

  // Forbidden OBUs in both av2C and AV2 samples
  EXPECT_TRUE(is_forbidden_in_isobmff(OBUType::TEMPORAL_DELIMITER));
  EXPECT_TRUE(is_forbidden_in_isobmff(OBUType::CONTENT_INTERPRETATION));
  EXPECT_TRUE(is_forbidden_in_isobmff(OBUType::PADDING));
  EXPECT_FALSE(is_forbidden_in_isobmff(OBUType::SEQUENCE_HEADER));
  EXPECT_FALSE(is_forbidden_in_isobmff(OBUType::CLK));
}

TEST(AV2ISOBMFFTest, MetadataConfigOBUAndSampleFiltering) {
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };

  append(make_td_obu());
  append(make_metadata_short_obu(GLOBAL_XLAYER_ID, /*is_suffix=*/false));  // Config OBU
  append(make_metadata_group_obu(GLOBAL_XLAYER_ID, /*is_suffix=*/false));  // Config OBU
  append(make_seq_header_obu(0));                                          // Config OBU
  append(make_ci_obu(0));                                                  // Forbidden in ISOBMFF
  append(make_metadata_short_obu(0, /*is_suffix=*/false));                 // Sample OBU (xlayer 0)
  append(make_metadata_group_obu(0, /*is_suffix=*/false));                 // Sample OBU (xlayer 0)
  append(make_clk_obu(0, 0));                                              // Sample OBU
  append(make_metadata_short_obu(GLOBAL_XLAYER_ID, /*is_suffix=*/true));   // Sample OBU (suffix)
  append(make_metadata_group_obu(GLOBAL_XLAYER_ID, /*is_suffix=*/true));   // Sample OBU (suffix)
  append(make_padding_obu());                                              // Forbidden in ISOBMFF

  const std::string fname = "test_meta_filtering.obu";
  write_bytes_to_file(fname, stream);

  OBUParser parser;
  ASSERT_TRUE(parser.parse_file(fname));
  ASSERT_EQ(parser.temporal_units().size(), 1u);
  const auto& tu = parser.temporal_units()[0];

  // Out-of-band ('av2E'): strip TD, CI, Padding, and all config OBUs
  auto oob_sample_obus = tu.sample_obus(/*keep_td=*/false, /*strip_config_obus=*/true);
  ASSERT_EQ(oob_sample_obus.size(), 5u);
  EXPECT_EQ(oob_sample_obus[0]->type(), OBUType::METADATA);
  EXPECT_EQ(oob_sample_obus[0]->header().get_xlayer_id(), 0u);
  EXPECT_EQ(oob_sample_obus[1]->type(), OBUType::METADATA_GROUP);
  EXPECT_EQ(oob_sample_obus[1]->header().get_xlayer_id(), 0u);
  EXPECT_EQ(oob_sample_obus[2]->type(), OBUType::CLK);
  EXPECT_EQ(oob_sample_obus[3]->type(), OBUType::METADATA);
  EXPECT_EQ(oob_sample_obus[3]->header().get_xlayer_id(), GLOBAL_XLAYER_ID);
  EXPECT_EQ(oob_sample_obus[4]->type(), OBUType::METADATA_GROUP);
  EXPECT_EQ(oob_sample_obus[4]->header().get_xlayer_id(), GLOBAL_XLAYER_ID);

  // In-band ('av2e'): strip TD, CI, Padding, but keep config OBUs in sample
  auto inband_sample_obus = tu.sample_obus(/*keep_td=*/false, /*strip_config_obus=*/false);
  ASSERT_EQ(inband_sample_obus.size(), 8u);
  EXPECT_EQ(inband_sample_obus[0]->type(), OBUType::METADATA);
  EXPECT_EQ(inband_sample_obus[1]->type(), OBUType::METADATA_GROUP);
  EXPECT_EQ(inband_sample_obus[2]->type(), OBUType::SEQUENCE_HEADER);

  std::remove(fname.c_str());
}

// ============================================================================
// 2. OutOfBandAV2CodecConfigurationBox ('av2C') Tests
// ============================================================================

TEST(AV2ISOBMFFTest, OutOfBandAV2CodecConfigurationBoxSerializeParseAndOrdering) {
  OutOfBandAV2CodecConfigurationBox av2c;
  // Add config OBUs intentionally out of TU order to test sort_in_tu_order()
  auto sh1 = make_seq_header_obu(/*xlayer_id=*/1);
  auto lcr1 = make_local_lcr_obu(/*xlayer_id=*/1, AUX_LAYER, ALPHA_AUX);
  auto sh0 = make_seq_header_obu(/*xlayer_id=*/0);
  auto lcr0 = make_local_lcr_obu(/*xlayer_id=*/0, TEXTURE_LAYER);
  auto meta_global = make_metadata_short_obu(GLOBAL_XLAYER_ID, /*is_suffix=*/false);
  auto msdo = make_msdo_obu();

  ASSERT_TRUE(av2c.append_raw_config_obu(sh1));
  ASSERT_TRUE(av2c.append_raw_config_obu(lcr1));
  ASSERT_TRUE(av2c.append_raw_config_obu(sh0));
  ASSERT_TRUE(av2c.append_raw_config_obu(lcr0));
  ASSERT_TRUE(av2c.append_raw_config_obu(meta_global));
  ASSERT_TRUE(av2c.append_raw_config_obu(msdo));
  // Duplicate OBU should be deduplicated
  ASSERT_TRUE(av2c.append_raw_config_obu(sh0));
  EXPECT_EQ(av2c.config_obus.size(), 6u);

  // Forbidden OBUs and non-config OBUs must be rejected
  EXPECT_FALSE(av2c.append_raw_config_obu(make_td_obu()));
  EXPECT_FALSE(av2c.append_raw_config_obu(make_ci_obu()));
  EXPECT_FALSE(av2c.append_raw_config_obu(make_padding_obu()));
  EXPECT_FALSE(av2c.append_raw_config_obu(make_clk_obu()));

  // Without sorting, out-of-order serialized bytes must fail OutOfBandAV2CodecConfigurationBox::parse()
  auto unsorted_bytes = av2c.serialize();
  OutOfBandAV2CodecConfigurationBox parsed_unsorted;
  EXPECT_FALSE(OutOfBandAV2CodecConfigurationBox::parse(unsorted_bytes.data(),
                                                        unsorted_bytes.size(), parsed_unsorted));

  // After sorting into canonical AV2 §7.3.5 TU order, parse() succeeds
  av2c.sort_in_tu_order();
  auto sorted_bytes = av2c.serialize();
  ASSERT_GE(sorted_bytes.size(), 2u);
  EXPECT_EQ(sorted_bytes[0], 0u);  // reserved1 = 0
  EXPECT_EQ(sorted_bytes[1], 5u);  // config_obus_count_minus1 = 6 - 1 = 5

  OutOfBandAV2CodecConfigurationBox parsed;
  ASSERT_TRUE(OutOfBandAV2CodecConfigurationBox::parse(sorted_bytes.data(), sorted_bytes.size(),
                                                       parsed));
  ASSERT_EQ(parsed.config_obus.size(), 6u);
  EXPECT_EQ(parsed.config_obus[0].obu_type, OBUType::MSDO);
  EXPECT_EQ(parsed.config_obus[1].obu_type, OBUType::METADATA);
  EXPECT_EQ(parsed.config_obus[1].xlayer_id, GLOBAL_XLAYER_ID);
  EXPECT_EQ(parsed.config_obus[2].obu_type, OBUType::LAYER_CONFIGURATION_RECORD);
  EXPECT_EQ(parsed.config_obus[2].xlayer_id, 0u);
  EXPECT_EQ(parsed.config_obus[3].obu_type, OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(parsed.config_obus[3].xlayer_id, 0u);
  EXPECT_EQ(parsed.config_obus[4].obu_type, OBUType::LAYER_CONFIGURATION_RECORD);
  EXPECT_EQ(parsed.config_obus[4].xlayer_id, 1u);
  EXPECT_EQ(parsed.config_obus[5].obu_type, OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(parsed.config_obus[5].xlayer_id, 1u);

  // Unrecognized trailing bytes in 'av2C' must be ignored per ISOBMFF
  auto sorted_with_trailing = sorted_bytes;
  sorted_with_trailing.push_back(0xDE);
  sorted_with_trailing.push_back(0xAD);
  OutOfBandAV2CodecConfigurationBox parsed_trailing;
  ASSERT_TRUE(OutOfBandAV2CodecConfigurationBox::parse(
      sorted_with_trailing.data(), sorted_with_trailing.size(), parsed_trailing));
  EXPECT_EQ(parsed_trailing.config_obus.size(), 6u);

  // Missing grouped HLS block (only global OBUs) must fail parse()
  OutOfBandAV2CodecConfigurationBox global_only;
  ASSERT_TRUE(global_only.append_raw_config_obu(msdo));
  auto global_only_bytes = global_only.serialize();
  OutOfBandAV2CodecConfigurationBox parsed_global_only;
  EXPECT_FALSE(OutOfBandAV2CodecConfigurationBox::parse(
      global_only_bytes.data(), global_only_bytes.size(), parsed_global_only));
}

// ============================================================================
// 3. InBandAV2CodecConfigurationBox ('av2c') Tests
// ============================================================================

TEST(AV2ISOBMFFTest, InBandAV2CodecConfigurationBoxSerializeAndParse) {
  InBandAV2CodecConfigurationBox inband;
  inband.reserved1 = 0;
  inband.max_num_streams_minus_1 = 2;  // 3 streams
  inband.max_profile_idc = 5;
  inband.max_level_idx = 12;
  inband.max_tier = 1;

  auto bytes = inband.serialize();
  ASSERT_EQ(bytes.size(), 2u);

  InBandAV2CodecConfigurationBox parsed;
  ASSERT_TRUE(InBandAV2CodecConfigurationBox::parse(bytes.data(), bytes.size(), parsed));
  EXPECT_EQ(parsed.reserved1, 0u);
  EXPECT_EQ(parsed.max_num_streams_minus_1, 2u);
  EXPECT_EQ(parsed.max_profile_idc, 5u);
  EXPECT_EQ(parsed.max_level_idx, 12u);
  EXPECT_EQ(parsed.max_tier, 1u);

  // Unrecognized trailing bytes in 'av2c' (> 2 bytes) must be ignored per ISOBMFF
  auto bytes_with_trailing = bytes;
  bytes_with_trailing.push_back(0xFF);
  bytes_with_trailing.push_back(0x00);
  InBandAV2CodecConfigurationBox parsed_trailing;
  ASSERT_TRUE(InBandAV2CodecConfigurationBox::parse(bytes_with_trailing.data(),
                                                    bytes_with_trailing.size(), parsed_trailing));
  EXPECT_EQ(parsed_trailing.max_num_streams_minus_1, 2u);
  EXPECT_EQ(parsed_trailing.max_profile_idc, 5u);
  EXPECT_EQ(parsed_trailing.max_level_idx, 12u);
  EXPECT_EQ(parsed_trailing.max_tier, 1u);

  // Test from_parser() with MSDO and SequenceHeader
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };
  append(make_td_obu());
  append(make_msdo_obu(/*num_streams=*/3, /*profile_idc=*/4, /*level_idx=*/10, /*tier=*/1));
  append(make_seq_header_obu(/*xlayer_id=*/0, /*profile_idc=*/1, /*level_idx=*/8, /*tier=*/0));
  append(make_clk_obu(0, 0));

  const std::string fname = "test_inband_parser.obu";
  write_bytes_to_file(fname, stream);
  OBUParser parser;
  ASSERT_TRUE(parser.parse_file(fname));

  auto derived = InBandAV2CodecConfigurationBox::from_parser(parser);
  EXPECT_EQ(derived.max_num_streams_minus_1, 2u);
  EXPECT_EQ(derived.max_profile_idc, 4u);
  EXPECT_EQ(derived.max_level_idx, 10u);
  EXPECT_EQ(derived.max_tier, 1u);

  std::remove(fname.c_str());
}

// ============================================================================
// 4. PixiBox ('pixi') & Codecs String Tests
// ============================================================================

TEST(AV2ISOBMFFTest, PixiBoxAndCodecsStringDerivation) {
  // Stream with xlayer 0 = YUV 4:2:0 10-bit texture (CSP_TOPLEFT=2),
  // xlayer 1 = 4:0:0 8-bit alpha auxiliary layer
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };
  append(make_td_obu());
  append(make_seq_header_obu(/*xlayer_id=*/0, /*profile_idc=*/0, /*level_idx=*/4, /*tier=*/0,
                             CHROMA_FORMAT_420, /*bit_depth_idc=*/0));  // 10-bit 4:2:0
  append(make_local_lcr_obu(/*xlayer_id=*/0, TEXTURE_LAYER));
  append(make_ci_obu(/*xlayer_id=*/0, /*cp=*/9, /*tc=*/16, /*mc=*/9, /*full_range=*/false,
                     /*has_csp=*/true, CSP_TOPLEFT));
  append(make_seq_header_obu(/*xlayer_id=*/1, /*profile_idc=*/0, /*level_idx=*/4, /*tier=*/0,
                             CHROMA_FORMAT_400, /*bit_depth_idc=*/1));  // 8-bit monochrome alpha
  append(make_local_lcr_obu(/*xlayer_id=*/1, AUX_LAYER, ALPHA_AUX));
  append(make_clk_obu(0, 0));
  append(make_clk_obu(1, 0));

  const std::string fname = "test_pixi_alpha.obu";
  write_bytes_to_file(fname, stream);
  OBUParser parser;
  ASSERT_TRUE(parser.parse_file(fname));

  PixiBox pixi = PixiBox::from_parser(parser);
  EXPECT_EQ(pixi.version, 0u);
  EXPECT_EQ(pixi.flags, 1u);
  ASSERT_EQ(pixi.channels.size(), 4u);

  // Y channel
  EXPECT_EQ(pixi.channels[0].bits_per_channel, 10u);
  EXPECT_EQ(pixi.channels[0].channel_idc, 2u);
  EXPECT_FALSE(pixi.channels[0].subsampling_flag);

  // U, V channels (4:2:0 -> subsampling_type = 2, CSP_TOPLEFT -> subsampling_location = 2)
  for (size_t i : {1u, 2u}) {
    EXPECT_EQ(pixi.channels[i].bits_per_channel, 10u);
    EXPECT_EQ(pixi.channels[i].channel_idc, i + 2);
    EXPECT_TRUE(pixi.channels[i].subsampling_flag);
    EXPECT_EQ(pixi.channels[i].subsampling_type, 2u);
    EXPECT_EQ(pixi.channels[i].subsampling_location, 2u);
  }

  // Alpha channel
  EXPECT_EQ(pixi.channels[3].bits_per_channel, 8u);
  EXPECT_EQ(pixi.channels[3].channel_idc, 5u);
  EXPECT_FALSE(pixi.channels[3].subsampling_flag);

  // Round-trip serialize/parse PixiBox (including unrecognized trailing bytes per ISOBMFF)
  auto pixi_bytes = pixi.serialize_payload();
  PixiBox parsed_pixi;
  ASSERT_TRUE(PixiBox::parse_payload(pixi_bytes.data(), pixi_bytes.size(), parsed_pixi));
  ASSERT_EQ(parsed_pixi.channels.size(), 4u);
  EXPECT_EQ(parsed_pixi.channels[3].channel_idc, 5u);
  auto pixi_with_trailing = pixi_bytes;
  pixi_with_trailing.push_back(0xAA);
  PixiBox parsed_pixi_trailing;
  ASSERT_TRUE(PixiBox::parse_payload(pixi_with_trailing.data(), pixi_with_trailing.size(),
                                     parsed_pixi_trailing));
  EXPECT_EQ(parsed_pixi_trailing.channels.size(), 4u);

  // Codecs string matches av2-isobmff #codecsparam example (with av2E 4CC)
  auto colr_opt = extract_colr_info(parser);
  ASSERT_TRUE(colr_opt.has_value());
  const auto* first_sh = dynamic_cast<const SequenceHeaderOBU*>(parser.obus()[1].get());
  ASSERT_NE(first_sh, nullptr);
  std::string codecs = build_codecs_string("av2E", first_sh->sequence_header(), pixi, *colr_opt);
  EXPECT_EQ(codecs, "av2E.0.04M.10.4.22.09.16.09.0");

  // Default suffix ".3.22.01.01.01.0" should be omitted
  PixiBox default_pixi;
  default_pixi.channels = pixi.channels;
  default_pixi.channels.pop_back();  // Remove alpha -> 3 channels
  ColrInfo default_colr{1, 1, 1, 0};
  std::string short_codecs =
      build_codecs_string("av2E", first_sh->sequence_header(), default_pixi, default_colr);
  EXPECT_EQ(short_codecs, "av2E.0.04M.10");

  std::remove(fname.c_str());
}

// ============================================================================
// 5. End-to-End Muxer & Demuxer Conformance Tests ('av2E' & 'av2e')
// ============================================================================

TEST(AV2ISOBMFFTest, OutOfBandMuxAndDemuxRoundTrip) {
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };

  // TU 0 (Key frame with SH, CI with SAR 4:3, CLK, and Padding)
  append(make_td_obu());
  append(make_seq_header_obu(0, 0, 1, 0, CHROMA_FORMAT_420, 1, 176, 144));
  append(make_ci_obu(0, 1, 1, 1, false, true, CSP_TOPLEFT, true, 4, 3));
  append(make_clk_obu(0, 0));
  append(make_padding_obu());

  // TU 1 (Inter frame with TD, Padding)
  append(make_td_obu());
  append(make_inter_tile_group_obu(0, 1));
  append(make_padding_obu());

  const std::string in_obu = "test_oob_in.obu";
  const std::string out_mp4 = "test_oob.mp4";
  const std::string out_obu = "test_oob_demux.obu";
  write_bytes_to_file(in_obu, stream);

  OBUParser in_parser;
  ASSERT_TRUE(in_parser.parse_file(in_obu));
  UserOptions user_opts;
  user_opts.frame_rate = 30.0;
  user_opts.inband_config = false;
  MuxStrategy strategy = determine_strategy(in_parser.get_statistics(), user_opts);
  Av2Muxer muxer(in_obu, strategy);
  ASSERT_TRUE(muxer.package(in_parser, out_mp4));

  // Inspect produced MP4 file for mandatory boxes ('av02' brand, 'av2E', 'av2C', 'colr', 'pixi', 'pasp')
  auto mp4_bytes = read_bytes_from_file(out_mp4);
  ASSERT_FALSE(mp4_bytes.empty());
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av02"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2E"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2C"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "colr"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "pixi"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "pasp"));

  // Demux back to .obu and verify reconstructed bitstream
  ASSERT_TRUE(Av2Demuxer().demux(out_mp4, out_obu));

  OBUParser demux_parser;
  ASSERT_TRUE(demux_parser.parse_file(out_obu));
  ASSERT_EQ(demux_parser.temporal_units().size(), 2u);
  ASSERT_EQ(demux_parser.obus().size(), 5u);
  EXPECT_EQ(demux_parser.obus()[0]->type(), OBUType::TEMPORAL_DELIMITER);
  EXPECT_EQ(demux_parser.obus()[1]->type(), OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(demux_parser.obus()[2]->type(), OBUType::CLK);
  EXPECT_EQ(demux_parser.obus()[3]->type(), OBUType::TEMPORAL_DELIMITER);
  EXPECT_EQ(demux_parser.obus()[4]->type(), OBUType::REGULAR_TILE_GROUP);

  // TU 0 has SEQUENCE_HEADER + CLK (with CI and PADDING stripped; TD splits the TUs)
  const auto& tu0 = demux_parser.temporal_units()[0];
  ASSERT_EQ(tu0.obus().size(), 2u);
  EXPECT_EQ(tu0.obus()[0]->type(), OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(tu0.obus()[1]->type(), OBUType::CLK);

  // TU 1 has REGULAR_TILE_GROUP (non-RAP sample does not duplicate av2C OBUs)
  const auto& tu1 = demux_parser.temporal_units()[1];
  ASSERT_EQ(tu1.obus().size(), 1u);
  EXPECT_EQ(tu1.obus()[0]->type(), OBUType::REGULAR_TILE_GROUP);

  std::remove(in_obu.c_str());
  std::remove(out_mp4.c_str());
  std::remove(out_obu.c_str());
}

TEST(AV2ISOBMFFTest, InBandMuxAndDemuxRoundTrip) {
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };

  append(make_td_obu());
  append(make_seq_header_obu(0, 0, 1, 0, CHROMA_FORMAT_420, 1, 176, 144));
  append(make_ci_obu(0, 1, 1, 1, false));
  append(make_clk_obu(0, 0));
  append(make_padding_obu());

  append(make_td_obu());
  append(make_inter_tile_group_obu(0, 1));

  const std::string in_obu = "test_inband_in.obu";
  const std::string out_mp4 = "test_inband.mp4";
  const std::string out_obu = "test_inband_demux.obu";
  write_bytes_to_file(in_obu, stream);

  OBUParser in_parser;
  ASSERT_TRUE(in_parser.parse_file(in_obu));
  UserOptions user_opts;
  user_opts.frame_rate = 30.0;
  user_opts.inband_config = true;
  MuxStrategy strategy = determine_strategy(in_parser.get_statistics(), user_opts);
  Av2Muxer muxer(in_obu, strategy);
  ASSERT_TRUE(muxer.package(in_parser, out_mp4));

  auto mp4_bytes = read_bytes_from_file(out_mp4);
  ASSERT_FALSE(mp4_bytes.empty());
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av02"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2e"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2c"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "colr"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "pixi"));

  ASSERT_TRUE(Av2Demuxer().demux(out_mp4, out_obu));

  OBUParser demux_parser;
  ASSERT_TRUE(demux_parser.parse_file(out_obu));
  ASSERT_EQ(demux_parser.temporal_units().size(), 2u);
  ASSERT_EQ(demux_parser.obus().size(), 5u);
  EXPECT_EQ(demux_parser.obus()[0]->type(), OBUType::TEMPORAL_DELIMITER);
  EXPECT_EQ(demux_parser.obus()[1]->type(), OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(demux_parser.obus()[2]->type(), OBUType::CLK);
  EXPECT_EQ(demux_parser.temporal_units()[0].obus()[0]->type(), OBUType::SEQUENCE_HEADER);
  EXPECT_EQ(demux_parser.temporal_units()[0].obus()[1]->type(), OBUType::CLK);

  std::remove(in_obu.c_str());
  std::remove(out_mp4.c_str());
  std::remove(out_obu.c_str());
}

TEST(AV2ISOBMFFTest, Chroma422And444AndDepthGainMapPixi) {
  // xlayer 0 = 4:2:2 12-bit texture, xlayer 1 = 4:4:4 10-bit gain map, xlayer 2 = 4:0:0 8-bit depth
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };
  append(make_td_obu());
  append(make_seq_header_obu(/*xlayer_id=*/0, 0, 4, 0, CHROMA_FORMAT_422, /*bit_depth_idc=*/2));
  append(make_local_lcr_obu(/*xlayer_id=*/0, TEXTURE_LAYER));
  append(make_seq_header_obu(/*xlayer_id=*/1, 0, 4, 0, CHROMA_FORMAT_444, /*bit_depth_idc=*/0));
  append(make_local_lcr_obu(/*xlayer_id=*/1, AUX_LAYER, GAIN_MAP_AUX));
  append(make_seq_header_obu(/*xlayer_id=*/2, 0, 4, 0, CHROMA_FORMAT_400, /*bit_depth_idc=*/1));
  append(make_local_lcr_obu(/*xlayer_id=*/2, AUX_LAYER, DEPTH_AUX));
  append(make_clk_obu(0, 0));
  append(make_clk_obu(1, 0));
  append(make_clk_obu(2, 0));

  const std::string fname = "test_pixi_422_444.obu";
  write_bytes_to_file(fname, stream);
  OBUParser parser;
  ASSERT_TRUE(parser.parse_file(fname));

  PixiBox pixi = PixiBox::from_parser(parser);
  ASSERT_EQ(pixi.channels.size(), 7u);
  // xlayer 0: 12-bit 4:2:2 texture -> channel_idc 2, 3 (subsampling_type=1), 4 (subsampling_type=1)
  EXPECT_EQ(pixi.channels[0].bits_per_channel, 12u);
  EXPECT_EQ(pixi.channels[0].channel_idc, 2u);
  EXPECT_FALSE(pixi.channels[0].subsampling_flag);
  EXPECT_EQ(pixi.channels[1].channel_idc, 3u);
  EXPECT_TRUE(pixi.channels[1].subsampling_flag);
  EXPECT_EQ(pixi.channels[1].subsampling_type, 1u);  // 4:2:2
  EXPECT_EQ(pixi.channels[2].channel_idc, 4u);
  EXPECT_TRUE(pixi.channels[2].subsampling_flag);
  EXPECT_EQ(pixi.channels[2].subsampling_type, 1u);

  // xlayer 1: 10-bit 4:4:4 gain map -> 3 channels with channel_idc = 1 (Unspecified);
  // per ISOBMFF 9th Ed. §12.1.11.2, subsampling fields are only present when channel_idc > 1.
  for (size_t i : {3u, 4u, 5u}) {
    EXPECT_EQ(pixi.channels[i].bits_per_channel, 10u);
    EXPECT_EQ(pixi.channels[i].channel_idc, 1u);
    EXPECT_FALSE(pixi.channels[i].subsampling_flag);
  }

  // xlayer 2: 8-bit 4:0:0 depth -> channel_idc = 6 (Depth)
  EXPECT_EQ(pixi.channels[6].bits_per_channel, 8u);
  EXPECT_EQ(pixi.channels[6].channel_idc, 6u);
  EXPECT_FALSE(pixi.channels[6].subsampling_flag);

  std::remove(fname.c_str());
}

TEST(AV2ISOBMFFTest, SyncSampleStssAndDefaultColrFallback) {
  // Single keyframe without CI OBU: all samples are sync samples -> 'stss' omitted, 'colr' present (2/2/2/0)
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };
  append(make_td_obu());
  append(make_seq_header_obu(0, 0, 1, 0, CHROMA_FORMAT_420, 1, 176, 144));
  append(make_clk_obu(0, 0));

  const std::string in_obu = "test_all_sync.obu";
  const std::string out_mp4 = "test_all_sync.mp4";
  write_bytes_to_file(in_obu, stream);

  OBUParser in_parser;
  ASSERT_TRUE(in_parser.parse_file(in_obu));
  UserOptions user_opts;
  user_opts.frame_rate = 30.0;
  MuxStrategy strategy = determine_strategy(in_parser.get_statistics(), user_opts);
  Av2Muxer muxer(in_obu, strategy);
  ASSERT_TRUE(muxer.package(in_parser, out_mp4));

  auto mp4_bytes = read_bytes_from_file(out_mp4);
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "colr"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "nclx"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "pixi"));
  // Per #sync-sample Note: when every sample in a track is a sync sample, 'stss' is omitted
  EXPECT_FALSE(contains_fourcc(mp4_bytes, "stss"));

  std::remove(in_obu.c_str());
  std::remove(out_mp4.c_str());
}

TEST(AV2ISOBMFFTest, MultiCVSOutOfBandRoundTrip) {
  // Two CVS segments with different SequenceHeaderOBU dimensions -> multiple 'av2E' sample entries
  std::vector<uint8_t> stream;
  auto append = [&](const std::vector<uint8_t>& b) {
    stream.insert(stream.end(), b.begin(), b.end());
  };
  // CVS 0 (176x144)
  append(make_td_obu());
  append(make_seq_header_obu(0, 0, 1, 0, CHROMA_FORMAT_420, 1, 176, 144));
  append(make_clk_obu(0, 0));
  // CVS 1 (160x120)
  append(make_td_obu());
  append(make_seq_header_obu(0, 0, 1, 0, CHROMA_FORMAT_420, 1, 160, 120));
  append(make_clk_obu(0, 0));

  const std::string in_obu = "test_multi_cvs.obu";
  const std::string out_mp4 = "test_multi_cvs.mp4";
  const std::string out_obu = "test_multi_cvs_demux.obu";
  write_bytes_to_file(in_obu, stream);

  OBUParser in_parser;
  ASSERT_TRUE(in_parser.parse_file(in_obu));
  UserOptions user_opts;
  user_opts.frame_rate = 30.0;
  MuxStrategy strategy = determine_strategy(in_parser.get_statistics(), user_opts);
  Av2Muxer muxer(in_obu, strategy);
  ASSERT_TRUE(muxer.package(in_parser, out_mp4));

  auto mp4_bytes = read_bytes_from_file(out_mp4);
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2E"));
  EXPECT_TRUE(contains_fourcc(mp4_bytes, "av2C"));

  ASSERT_TRUE(Av2Demuxer().demux(out_mp4, out_obu));
  OBUParser demux_parser;
  ASSERT_TRUE(demux_parser.parse_file(out_obu));
  ASSERT_EQ(demux_parser.temporal_units().size(), 2u);
  // Both TUs are random access points with their respective SequenceHeaderOBU reconstructed
  ASSERT_EQ(demux_parser.temporal_units()[0].obus().size(), 2u);
  EXPECT_EQ(demux_parser.temporal_units()[0].obus()[0]->type(), OBUType::SEQUENCE_HEADER);
  const auto* sh0 =
      dynamic_cast<const SequenceHeaderOBU*>(demux_parser.temporal_units()[0].obus()[0]);
  ASSERT_NE(sh0, nullptr);
  EXPECT_EQ(sh0->sequence_header().max_frame_width_minus_1, 175u);
  EXPECT_EQ(sh0->sequence_header().max_frame_height_minus_1, 143u);

  ASSERT_EQ(demux_parser.temporal_units()[1].obus().size(), 2u);
  EXPECT_EQ(demux_parser.temporal_units()[1].obus()[0]->type(), OBUType::SEQUENCE_HEADER);
  const auto* sh1 =
      dynamic_cast<const SequenceHeaderOBU*>(demux_parser.temporal_units()[1].obus()[0]);
  ASSERT_NE(sh1, nullptr);
  EXPECT_EQ(sh1->sequence_header().max_frame_width_minus_1, 159u);
  EXPECT_EQ(sh1->sequence_header().max_frame_height_minus_1, 119u);

  std::remove(in_obu.c_str());
  std::remove(out_mp4.c_str());
  std::remove(out_obu.c_str());
}

}  // namespace
}  // namespace av2_obu
