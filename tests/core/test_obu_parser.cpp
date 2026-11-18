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

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>  // std::remove
#include <fstream>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/core/frame_header_info.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/obus/sequence_header_obu.h>

using namespace av2_obu;

class AV2OBUParserTest : public ::testing::Test {
protected:
  void SetUp() override { parser = std::make_unique<OBUParser>(); }
  void TearDown() override { parser.reset(); }

  // Helper to create a temporary file with test data
  void createTestFile(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    file.close();
  }

  std::unique_ptr<OBUParser> parser;
};

// Test parsing stable AV2 OBU header format
TEST_F(AV2OBUParserTest, ParseStableOBUHeaders) {
  const std::vector<uint8_t> data = {
    0x01, 0x08,             // size=1, TEMPORAL_DELIMITER no extension, no payload
    0x06, 0xFB, 0x23,       // size=6, UNKNOWN_OBU(30) with extension: tlayer=3, mlayer=1, xlayer=3
    0xDE, 0xAD, 0xBE, 0xEF  // DEADBEEF payload (4 bytes)
  };

  const char* fname = "test_headers.obu";
  createTestFile(fname, data);

  ASSERT_TRUE(parser->parse_file(fname));
  const auto& obus = parser->obus();
  ASSERT_EQ(obus.size(), 2u) << "Expected exactly 2 OBUs";

  // --- First OBU: TEMPORAL_DELIMITER (type=2) ---
  EXPECT_EQ(obus[0]->type(), OBUType::TEMPORAL_DELIMITER);
  EXPECT_EQ(obus[0]->header().get_extension_flag(), 0u) << "TD has no extension";
  EXPECT_EQ(obus[0]->header().get_obu_type_raw(), 2u) << "TD type is 2";
  EXPECT_EQ(obus[0]->header().get_tlayer_id(), 0u);
  EXPECT_EQ(obus[0]->position().size_field_len, 1u) << "LEB128 size = 0x01";
  EXPECT_EQ(obus[0]->position().header_len, 1u) << "1-byte header (no extension)";
  EXPECT_EQ(obus[0]->position().payload_size, 0u) << "TD has no payload";

  // --- Second OBU: UNKNOWN (type=30) with extension ---
  EXPECT_EQ(obus[1]->type(), OBUType::UNKNOWN) << "Type 30 is unknown/reserved";
  EXPECT_EQ(obus[1]->header().get_extension_flag(), 1u) << "Has extension byte";
  EXPECT_EQ(obus[1]->header().get_obu_type_raw(), 30u) << "OBU type = 30";
  EXPECT_EQ(obus[1]->header().get_tlayer_id(), 3u) << "Temporal layer = 3";
  EXPECT_EQ(obus[1]->header().get_mlayer_id(), 1u) << "Spatial layer = 1";
  EXPECT_EQ(obus[1]->header().get_xlayer_id(), 3u) << "Quality layer = 3";
  EXPECT_EQ(obus[1]->position().size_field_len, 1u) << "LEB128 size = 0x06";
  EXPECT_EQ(obus[1]->position().header_len, 2u) << "2-byte header (with extension)";
  EXPECT_EQ(obus[1]->position().payload_size, 4u) << "4-byte DEADBEEF payload";

  // Cleanup
  std::remove(fname);
}

// Test default layer ids of OBUs without the extension byte
TEST_F(AV2OBUParserTest, DefaultLayerIdsWithoutExtension) {
  const std::vector<uint8_t> data = {
    0x01, 0x08,        // TEMPORAL_DELIMITER, no extension
    0x02, 0x64, 0x80,  // PADDING, no extension, 1 byte payload
  };
  const char* fname = "test_default_layers.obu";
  createTestFile(fname, data);
  ASSERT_TRUE(parser->parse_file(fname));
  const auto& obus = parser->obus();
  ASSERT_EQ(obus.size(), 2u);
  EXPECT_EQ(obus[0]->header().get_xlayer_id(), GLOBAL_XLAYER_ID) << "TD without extension is global";
  EXPECT_EQ(obus[0]->header().get_mlayer_id(), 0u);
  EXPECT_EQ(obus[1]->header().get_xlayer_id(), 0u) << "other OBUs default to xlayer 0";
  EXPECT_EQ(obus[1]->header().get_mlayer_id(), 0u);
  std::remove(fname);
}

TEST_F(AV2OBUParserTest, ToJsonHasSchemaVersion) {
  const std::vector<uint8_t> data = {0x01, 0x08};
  const char* fname = "test_schema_version.obu";
  createTestFile(fname, data);
  ASSERT_TRUE(parser->parse_file(fname));
  const auto j = parser->to_json();
  ASSERT_TRUE(j.contains("schema_version"));
  EXPECT_EQ(j["schema_version"].get<int>(), 1);
  EXPECT_EQ(j["obus"][0]["header"]["xlayer_id"].get<int>(), static_cast<int>(GLOBAL_XLAYER_ID));
  std::remove(fname);
}

// color_description_idc presets are expanded to primaries, transfer and matrix
TEST_F(AV2OBUParserTest, ColorDescriptionPresetsAreExpanded) {
  uint32_t cp = 0, tc = 0, mc = 0;
  EXPECT_FALSE(color_description_preset(0, cp, tc, mc)) << "0 means explicit";
  EXPECT_FALSE(color_description_preset(6, cp, tc, mc)) << "reserved";
  ASSERT_TRUE(color_description_preset(1, cp, tc, mc));
  EXPECT_EQ(std::vector<uint32_t>({cp, tc, mc}), std::vector<uint32_t>({1, 1, 1}));
  ASSERT_TRUE(color_description_preset(2, cp, tc, mc));
  EXPECT_EQ(std::vector<uint32_t>({cp, tc, mc}), std::vector<uint32_t>({9, 16, 9}));
  ASSERT_TRUE(color_description_preset(3, cp, tc, mc));
  EXPECT_EQ(std::vector<uint32_t>({cp, tc, mc}), std::vector<uint32_t>({9, 18, 9}));
  ASSERT_TRUE(color_description_preset(4, cp, tc, mc));
  EXPECT_EQ(std::vector<uint32_t>({cp, tc, mc}), std::vector<uint32_t>({1, 13, 0}));
  ASSERT_TRUE(color_description_preset(5, cp, tc, mc));
  EXPECT_EQ(std::vector<uint32_t>({cp, tc, mc}), std::vector<uint32_t>({1, 13, 5}));
}

TEST_F(AV2OBUParserTest, ContentInterpretationPresetInJson) {
  const std::vector<uint8_t> data = {
    0x01, 0x08,                    // TEMPORAL_DELIMITER
    0x03, 0x60, 0x20, 0x28,        // CONTENT_INTERPRETATION: color description present, idc 1, full_range 0
  };
  const char* fname = "test_ci_preset.obu";
  createTestFile(fname, data);
  ASSERT_TRUE(parser->parse_file(fname));
  const auto j = parser->to_json();
  const auto& cd = j["obus"][1]["color_description"];
  EXPECT_EQ(cd["color_description_idc"].get<int>(), 1);
  EXPECT_EQ(cd["color_primaries"].get<int>(), 1);
  EXPECT_EQ(cd["transfer_characteristics"].get<int>(), 1);
  EXPECT_EQ(cd["matrix_coefficients"].get<int>(), 1);
  EXPECT_EQ(cd["full_range_flag"].get<int>(), 0);
  std::remove(fname);
}

// Global LCR (xlayers 0-2) and local LCR (xlayer 1) with every optional syntax element enabled
TEST_F(AV2OBUParserTest, LcrAllOptionalFieldsAreParsedAndDumped) {
  const std::vector<uint8_t> data = {
    0x01, 0x08, 0x63, 0xC0, 0x1F, 0xA0, 0x00, 0x00, 0x01, 0xFE, 0xAB, 0xC0,
    0x86, 0x39, 0x1A, 0x88, 0x22, 0xE8, 0x2B, 0x08, 0x26, 0xF0, 0x03, 0xC0,
    0x80, 0x10, 0xE7, 0x68, 0xA7, 0x12, 0x2E, 0xAC, 0x40, 0x0B, 0xF0, 0x10,
    0x20, 0x30, 0x00, 0x18, 0x30, 0x40, 0x50, 0x60, 0x10, 0x20, 0x44, 0xD8,
    0x01, 0x40, 0x80, 0x78, 0x40, 0x10, 0x70, 0x80, 0x90, 0x00, 0x2B, 0x10,
    0xF0, 0x01, 0xE0, 0x40, 0x08, 0x73, 0xB4, 0x53, 0x89, 0x17, 0x56, 0x60,
    0x07, 0x08, 0x09, 0xF8, 0x18, 0xFC, 0x00, 0xF0, 0x20, 0x04, 0x39, 0xDA,
    0x29, 0xC4, 0x8B, 0xAA, 0x02, 0x44, 0x02, 0x60, 0x01, 0x10, 0xA0, 0xB0,
    0xC0, 0x10, 0x00, 0x40, 0x18, 0x80, 0x1A, 0xC0, 0x01, 0x73, 0x3D, 0x30,
    0xA0, 0xF0, 0x03, 0xC0, 0x80, 0x10, 0xE7, 0x68, 0xA7, 0x12, 0x2E, 0xAA,
    0x80, 0x01, 0xF0, 0x10, 0x20, 0x30, 0x00, 0x08, 0x80,
  };
  const char* fname = "test_lcr_all_fields.obu";
  createTestFile(fname, data);
  ASSERT_TRUE(parser->parse_file(fname));
  const auto j = parser->to_json();
  ASSERT_EQ(j["obus"].size(), 3u);

  const auto& g = j["obus"][1];
  EXPECT_EQ(g["lcr_global_config_record_id"].get<int>(), 5);
  EXPECT_EQ(g["lcr_xlayer_map"].get<int>(), 7);
  EXPECT_EQ(g["lcr_global_atlas_id"].get<int>(), 6);
  EXPECT_EQ(g["lcr_global_reserved_zero_5bits"].get<int>(), 0);
  EXPECT_EQ(g["aggregate_info"]["config_idc"].get<int>(), 33);
  EXPECT_EQ(g["aggregate_info"]["aggregate_level_idx"].get<int>(), 17);
  EXPECT_EQ(g["aggregate_info"]["max_interop"].get<int>(), 9);
  ASSERT_EQ(g["xlayer_ptl"].size(), 3u);
  EXPECT_EQ(g["xlayer_ptl"][2]["max_level_idx"].get<int>(), 12);

  const auto& p0 = g["global_payloads"][0];
  EXPECT_FALSE(p0.contains("num_dependent_xlayer_map"));
  const auto& ci0 = p0["xlayer_info"]["color_info"];
  EXPECT_EQ(ci0["color_description_idc"].get<int>(), 4);
  EXPECT_EQ(ci0["transfer_characteristics"].get<int>(), 13);
  const auto& rep = p0["xlayer_info"]["rep_info"];
  EXPECT_EQ(rep["max_pic_width"].get<int>(), 1920);
  EXPECT_EQ(rep["crop_bottom"].get<int>(), 10);
  const auto& l = p0["xlayer_info"]["embedded_layers"];
  ASSERT_EQ(l.size(), 3u);
  EXPECT_EQ(l[0]["atlas_segment_id"].get<int>(), 1);
  EXPECT_EQ(l[0]["rendering_method"].get<int>(), 3);
  EXPECT_FALSE(l[0].contains("dependent_layer_map"));
  EXPECT_TRUE(l[0]["same_sh_max_resolution"].get<bool>());
  EXPECT_EQ(l[1]["mlayer_id"].get<int>(), 1);
  EXPECT_EQ(l[1]["priority_order"].get<int>(), 5);
  EXPECT_EQ(l[1]["auxiliary_type"].get<int>(), 2);
  EXPECT_EQ(l[1]["view_id"].get<int>(), 77);
  EXPECT_EQ(l[1]["dependent_layer_map"].get<int>(), 1);
  EXPECT_EQ(l[1]["max_expected_width"].get<int>(), 640);
  EXPECT_EQ(l[2]["mlayer_id"].get<int>(), 3);
  EXPECT_EQ(l[2]["dependent_layer_map"].get<int>(), 5);

  const auto& p1 = g["global_payloads"][1];
  EXPECT_EQ(p1["num_dependent_xlayer_map"].get<int>(), 1);
  EXPECT_EQ(p1["remaining_payload_bits"].get<int>(), 8);
  EXPECT_EQ(p1["xlayer_info"]["color_info"]["color_description_idc"].get<int>(), 5);
  EXPECT_FALSE(p1["xlayer_info"].contains("embedded_layers"));
  EXPECT_EQ(p1["xlayer_info"]["atlas_segment_id"].get<int>(), 7);
  EXPECT_EQ(p1["xlayer_info"]["priority_order"].get<int>(), 8);
  EXPECT_EQ(p1["xlayer_info"]["rendering_method"].get<int>(), 9);

  const auto& p2 = g["global_payloads"][2];
  EXPECT_EQ(p2["num_dependent_xlayer_map"].get<int>(), 3);
  EXPECT_FALSE(p2.contains("remaining_payload_bits"));
  EXPECT_EQ(p2["xlayer_info"]["color_info"]["color_primaries"].get<int>(), 9);
  EXPECT_EQ(p2["xlayer_info"]["embedded_layers"][0]["atlas_segment_id"].get<int>(), 10);

  const auto& loc = j["obus"][2];
  EXPECT_EQ(loc["lcr_local_atlas_id"].get<int>(), 5);
  EXPECT_EQ(loc["local_ptl"]["seq_profile_idc"].get<int>(), 7);
  EXPECT_EQ(loc["xlayer_info"]["color_info"]["color_description_idc"].get<int>(), 2);
  EXPECT_EQ(loc["xlayer_info"]["embedded_layers"][0]["atlas_segment_id"].get<int>(), 1);
  std::remove(fname);
}

// frame_size_with_refs needs NumTotalRefs, unknown in lightweight mode with an implicit ref map
TEST(FrameHeaderInfoTest, TipWithImplicitRefMapAndSizeOverrideDoesNotOverread) {
  AV2SequenceHeader sh;
  sh.inter_config.OrderHintBits = 7;
  sh.inter_config.NumRefFrames = 8;
  sh.inter_config.enable_short_refresh_frame_flags = 1;
  sh.inter_config.explicit_ref_frame_map = 0;
  sh.monotonic_output_order_flag = 1;
  sh.frame_width_bits_minus_1 = 9;
  sh.frame_height_bits_minus_1 = 8;
  sh.max_frame_width_minus_1 = 639;
  sh.max_frame_height_minus_1 = 479;

  const std::vector<uint8_t> payload = {0xF0, 0x8A, 0xEA, 0xA0};
  BitstreamReader br(payload);
  FrameHeaderInfo fh;
  ASSERT_TRUE(fh.parse_lightweight(br, OBUType::REGULAR_TIP, sh));
  EXPECT_TRUE(fh.parsed);
  EXPECT_EQ(fh.order_hint, 4u);
  EXPECT_EQ(fh.immediate_output_frame, 1u);
  EXPECT_EQ(fh.refresh_frame_flags, 4u);
  EXPECT_TRUE(fh.is_output_frame);
  EXPECT_EQ(fh.FrameWidth, 640u);
}

// Each plane is indexed by restoration type up to RESTORE_WIENER_NONSEP
TEST_F(AV2OBUParserTest, SequenceHeaderLoopRestorationToolsAreInBounds) {
  const std::vector<uint8_t> data = {
    0x01, 0x08,  // TEMPORAL_DELIMITER
    0x19, 0x84, 0x00, 0x88, 0x0C, 0x80, 0x87, 0xBF, 0xE4, 0x86, 0x18, 0x00, 0x00, 0x07,
    0x9B, 0xB9, 0xA0, 0x2E, 0x6F, 0xCC, 0xEF, 0x96, 0x9B, 0x9C, 0x56, 0x84,  // SEQUENCE_HEADER
  };
  const char* fname = "test_sh_lr_tools.obu";
  createTestFile(fname, data);
  ASSERT_TRUE(parser->parse_file(fname));
  ASSERT_EQ(parser->obus().size(), 2u);
  const auto* sh_obu = dynamic_cast<const SequenceHeaderOBU*>(parser->obus()[1].get());
  ASSERT_NE(sh_obu, nullptr);
  const auto& fc = sh_obu->sequence_header().filter_config;
  ASSERT_EQ(fc.enable_restoration, 1u);
  ASSERT_EQ(fc.lr_tools_disable.size(), 2u);
  for (const auto& plane : fc.lr_tools_disable) {
    EXPECT_GE(plane.size(), static_cast<size_t>(RESTORE_WIENER_NONSEP) + 1);
  }
  EXPECT_EQ(fc.lr_tools_disable[1][RESTORE_PC_WIENER], 1u);
  if (!fc.lr_tools_uv_present) {
    EXPECT_EQ(fc.lr_tools_disable[1][RESTORE_WIENER_NONSEP],
              fc.lr_tools_disable[0][RESTORE_WIENER_NONSEP]);
  }
  std::remove(fname);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
