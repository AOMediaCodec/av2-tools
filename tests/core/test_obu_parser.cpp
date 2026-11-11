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
#include <av2_obu/core/obu_parser.h>

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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
