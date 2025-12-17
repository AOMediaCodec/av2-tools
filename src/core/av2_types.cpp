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

#include <algorithm>
#include <cctype>

#include <av2_obu/core/av2_types.h>

namespace av2_obu {

// ========== LOGGING CONTROL ==========

void set_log_level(spdlog::level::level_enum level) {
  spdlog::set_level(level);
}

spdlog::level::level_enum get_log_level() {
  return spdlog::get_level();
}

// Initialize library with default log level (warn)
namespace {
struct LogInit {
  LogInit() { spdlog::set_level(spdlog::level::warn); }
};
static LogInit log_init;
}  // namespace

// ========== TYPE MAPPINGS ==========

namespace {

const std::map<OBUType, std::string> OBU_TYPE_NAMES = {
  {OBUType::SEQUENCE_HEADER, "OBU_SEQUENCE_HEADER"},
  {OBUType::TEMPORAL_DELIMITER, "OBU_TEMPORAL_DELIMITER"},
  {OBUType::MULTI_FRAME_HEADER, "OBU_MULTI_FRAME_HEADER"},
  {OBUType::CLK, "OBU_CLK"},
  {OBUType::OLK, "OBU_OLK"},
  {OBUType::LEADING_TILE_GROUP, "OBU_LEADING_TILE_GROUP"},
  {OBUType::REGULAR_TILE_GROUP, "OBU_REGULAR_TILE_GROUP"},
  {OBUType::METADATA, "OBU_METADATA"},
  {OBUType::METADATA_GROUP, "OBU_METADATA_GROUP"},
  {OBUType::SWITCH, "OBU_SWITCH"},
  {OBUType::LEADING_SEF, "OBU_LEADING_SEF"},
  {OBUType::REGULAR_SEF, "OBU_REGULAR_SEF"},
  {OBUType::LEADING_TIP, "OBU_LEADING_TIP"},
  {OBUType::REGULAR_TIP, "OBU_REGULAR_TIP"},
  {OBUType::BUFFER_REMOVAL_TIMING, "OBU_BUFFER_REMOVAL_TIMING"},
  {OBUType::LAYER_CONFIGURATION_RECORD, "OBU_LAYER_CONFIGURATION_RECORD"},
  {OBUType::ATLAS_SEGMENT, "OBU_ATLAS_SEGMENT"},
  {OBUType::OPERATING_POINT_SET, "OBU_OPERATING_POINT_SET"},
  {OBUType::BRIDGE_FRAME, "OBU_BRIDGE_FRAME"},
  {OBUType::MSDO, "OBU_MSDO"},
  {OBUType::RAS_FRAME, "OBU_RAS_FRAME"},
  {OBUType::QM, "OBU_QM"},
  {OBUType::FGM, "OBU_FGM"},
  {OBUType::CONTENT_INTERPRETATION, "OBU_CONTENT_INTERPRETATION"},
  {OBUType::PADDING, "OBU_PADDING"},
  {OBUType::UNKNOWN, "UNKNOWN_OBU"}};

const std::map<uint32_t, OBUType> VALUE_TO_OBU_TYPE = {{1, OBUType::SEQUENCE_HEADER},
                                                       {2, OBUType::TEMPORAL_DELIMITER},
                                                       {3, OBUType::MULTI_FRAME_HEADER},
                                                       {4, OBUType::CLK},
                                                       {5, OBUType::OLK},
                                                       {6, OBUType::LEADING_TILE_GROUP},
                                                       {7, OBUType::REGULAR_TILE_GROUP},
                                                       {8, OBUType::METADATA},
                                                       {9, OBUType::METADATA_GROUP},
                                                       {10, OBUType::SWITCH},
                                                       {11, OBUType::LEADING_SEF},
                                                       {12, OBUType::REGULAR_SEF},
                                                       {13, OBUType::LEADING_TIP},
                                                       {14, OBUType::REGULAR_TIP},
                                                       {15, OBUType::BUFFER_REMOVAL_TIMING},
                                                       {16, OBUType::LAYER_CONFIGURATION_RECORD},
                                                       {17, OBUType::ATLAS_SEGMENT},
                                                       {18, OBUType::OPERATING_POINT_SET},
                                                       {19, OBUType::BRIDGE_FRAME},
                                                       {20, OBUType::MSDO},
                                                       {21, OBUType::RAS_FRAME},
                                                       {22, OBUType::QM},
                                                       {23, OBUType::FGM},
                                                       {24, OBUType::CONTENT_INTERPRETATION},
                                                       {25, OBUType::PADDING}};

const std::map<MetadataType, std::string> METADATA_TYPE_NAMES = {
  {MetadataType::RESERVED, "METADATA_RESERVED"},
  {MetadataType::HDR_CLL, "METADATA_TYPE_HDR_CLL"},
  {MetadataType::HDR_MDCV, "METADATA_TYPE_HDR_MDCV"},
  {MetadataType::SCALABILITY, "METADATA_TYPE_SCALABILITY"},
  {MetadataType::ITUT_T35, "METADATA_TYPE_ITUT_T35"},
  {MetadataType::TIMECODE, "METADATA_TYPE_TIMECODE"},
  {MetadataType::HASH, "METADATA_HASH"},
  {MetadataType::BANDING_HINTS, "METADATA_BANDING_HINTS"},
  {MetadataType::ICC_PROFILE, "METADATA_ICC_PROFILE"},
  {MetadataType::SCAN_TYPE, "METADATA_SCAN_TYPE"},
  {MetadataType::TEMPORAL_POINT_INFO, "METADATA_TEMPORAL_POINT_INFO"},
  {MetadataType::UNKNOWN_METADATA, "UNKNOWN_METADATA"}};

const std::map<uint32_t, MetadataType> VALUE_TO_METADATA_TYPE = {
  {0, MetadataType::RESERVED},    {1, MetadataType::HDR_CLL},       {2, MetadataType::HDR_MDCV},
  {3, MetadataType::SCALABILITY}, {4, MetadataType::ITUT_T35},      {5, MetadataType::TIMECODE},
  {6, MetadataType::HASH},        {7, MetadataType::BANDING_HINTS}, {8, MetadataType::ICC_PROFILE},
  {9, MetadataType::SCAN_TYPE},   {10, MetadataType::TEMPORAL_POINT_INFO}};
}  // namespace

std::string to_string(OBUType type) {
  auto it = OBU_TYPE_NAMES.find(type);
  return (it != OBU_TYPE_NAMES.end()) ? it->second : "UNKNOWN_OBU";
}

OBUType to_obu_type(uint32_t value) {
  auto it = VALUE_TO_OBU_TYPE.find(value);
  return (it != VALUE_TO_OBU_TYPE.end()) ? it->second : OBUType::UNKNOWN;
}

std::ostream& operator<<(std::ostream& os, OBUType type) {
  return os << to_string(type);
}

bool is_valid_obu_type(uint32_t value) {
  return VALUE_TO_OBU_TYPE.find(value) != VALUE_TO_OBU_TYPE.end();
}

std::string to_string(MetadataType type) {
  auto it = METADATA_TYPE_NAMES.find(type);
  return (it != METADATA_TYPE_NAMES.end()) ? it->second : "UNKNOWN_METADATA";
}

MetadataType to_metadata_type(uint32_t value) {
  auto it = VALUE_TO_METADATA_TYPE.find(value);
  return (it != VALUE_TO_METADATA_TYPE.end()) ? it->second : MetadataType::UNKNOWN_METADATA;
}

std::ostream& operator<<(std::ostream& os, MetadataType type) {
  return os << to_string(type);
}

bool is_valid_metadata_type(uint32_t value) {
  return VALUE_TO_METADATA_TYPE.find(value) != VALUE_TO_METADATA_TYPE.end();
}

}  // namespace av2_obu
