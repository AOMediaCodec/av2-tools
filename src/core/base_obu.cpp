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

#include <iomanip>

#include <av2_obu/core/base_obu.h>
#include <av2_obu/obus/atlas_segment_obu.h>
#include <av2_obu/obus/bridge_frame_obu.h>
#include <av2_obu/obus/buffer_removal_timing_obu.h>
#include <av2_obu/obus/clk_obu.h>
#include <av2_obu/obus/content_interpretation_obu.h>
#include <av2_obu/obus/fgm_obu.h>
#include <av2_obu/obus/layer_configuration_record_obu.h>
#include <av2_obu/obus/leading_sef_obu.h>
#include <av2_obu/obus/leading_tile_group_obu.h>
#include <av2_obu/obus/leading_tip_obu.h>
#include <av2_obu/obus/metadata_obu.h>
#include <av2_obu/obus/metadata_obu_impl.h>
#include <av2_obu/obus/metadata_obu_wrapper.h>
#include <av2_obu/obus/msdo_obu.h>
#include <av2_obu/obus/multi_frame_header_obu.h>
#include <av2_obu/obus/olk_obu.h>
#include <av2_obu/obus/operating_point_set_obu.h>
#include <av2_obu/obus/padding_obu.h>
#include <av2_obu/obus/qm_obu.h>
#include <av2_obu/obus/ras_frame_obu.h>
#include <av2_obu/obus/regular_sef_obu.h>
#include <av2_obu/obus/regular_tile_group_obu.h>
#include <av2_obu/obus/regular_tip_obu.h>
#include <av2_obu/obus/sequence_header_obu.h>
#include <av2_obu/obus/switch_obu.h>
#include <av2_obu/obus/temporal_delimiter_obu.h>
#include <av2_obu/obus/tile_group_obu.h>
#include <av2_obu/obus/unknown_obu.h>

namespace av2_obu {

// ==================== OBUPosition ====================

json OBUPosition::to_json() const {
  // Calculate OBU size (header + payload, excluding Annex B size field)
  uint32_t obu_size = header_len + payload_size;

  return json{{"file_offset", static_cast<long long>(start_pos)},
              {"size_field_bytes", size_field_len},
              {"obu_size", obu_size},
              {"header_size", header_len},
              {"payload_size", payload_size}};
}

// ==================== OBUHeader ====================

bool OBUHeader::parse(std::ifstream& ifs) {
  char byte1;
  if (!ifs.read(&byte1, 1)) {
    spdlog::error("Failed to read OBU header byte");
    return false;
  }

  // OBU header format:
  // byte 1: [extension_flag(1) | obu_type(5) | tlayer_id(2)]
  obu_extension_flag_ = (byte1 >> 7) & 0x1;
  obu_type_ = (byte1 >> 2) & 0x1F;
  obu_tlayer_id_ = byte1 & 0x3;

  spdlog::debug("OBU header: type={}, tlayer={}, ext={}", obu_type_, obu_tlayer_id_,
                obu_extension_flag_);

  if (obu_extension_flag_) {
    char byte2;
    if (!ifs.read(&byte2, 1)) {
      spdlog::error("Failed to read OBU extension byte");
      return false;
    }
    // byte 2: [mlayer_id(3) | xlayer_id(5)]
    obu_mlayer_id_ = (byte2 >> 5) & 0x07;
    obu_xlayer_id_ = byte2 & 0x1F;
    spdlog::debug("  Extension: mlayer={}, xlayer={}", obu_mlayer_id_, obu_xlayer_id_);
  }

  if (!is_valid_obu_type(obu_type_)) {
    spdlog::warn("Unknown OBU type: {}", obu_type_);
  }

  return ifs.good();
}

json OBUHeader::to_json() const {
  return {{"obu_type", static_cast<int>(obu_type_)},
          {"tlayer_id", obu_tlayer_id_},
          {"mlayer_id", obu_mlayer_id_},
          {"xlayer_id", obu_xlayer_id_},
          {"extension_flag", obu_extension_flag_}};
}

// ==================== BaseOBU ====================

std::unique_ptr<BaseOBU> BaseOBU::create(std::ifstream& ifs, const OBUPosition& pos) {
  spdlog::debug("Creating OBU at position {}", static_cast<long long>(pos.start_pos));

  std::unique_ptr<BaseOBU> obu;

  // Read header to determine type
  OBUHeader temp_header;
  ifs.seekg(pos.header_pos);
  if (!temp_header.parse(ifs)) {
    spdlog::error("Failed to parse OBU header at position {}",
                  static_cast<long long>(pos.header_pos));
    return nullptr;
  }

  // Create appropriate derived class based on type
  OBUType type = temp_header.get_obu_type();

  switch (type) {
    case OBUType::SEQUENCE_HEADER:
      obu = std::make_unique<SequenceHeaderOBU>(pos);
      break;
    case OBUType::TEMPORAL_DELIMITER:
      obu = std::make_unique<TemporalDelimiterOBU>(pos);
      break;
    case OBUType::MULTI_FRAME_HEADER:
      obu = std::make_unique<MultiFrameHeaderOBU>(pos);
      break;
    case OBUType::CLK:
      obu = std::make_unique<CLKOBU>(pos);
      break;
    case OBUType::OLK:
      obu = std::make_unique<OLKOBU>(pos);
      break;
    case OBUType::LEADING_TILE_GROUP:
      obu = std::make_unique<LeadingTileGroupOBU>(pos);
      break;
    case OBUType::REGULAR_TILE_GROUP:
      obu = std::make_unique<RegularTileGroupOBU>(pos);
      break;
    case OBUType::METADATA:
      obu = std::make_unique<MetadataOBUWrapper>(pos);
      break;
    case OBUType::METADATA_GROUP:
      obu = std::make_unique<MetadataGroupOBU>(pos);
      break;
    case OBUType::SWITCH:
      obu = std::make_unique<SwitchOBU>(pos);
      break;
    case OBUType::LEADING_SEF:
      obu = std::make_unique<LeadingSEFOBU>(pos);
      break;
    case OBUType::REGULAR_SEF:
      obu = std::make_unique<RegularSEFOBU>(pos);
      break;
    case OBUType::LEADING_TIP:
      obu = std::make_unique<LeadingTIPOBU>(pos);
      break;
    case OBUType::REGULAR_TIP:
      obu = std::make_unique<RegularTIPOBU>(pos);
      break;
    case OBUType::BUFFER_REMOVAL_TIMING:
      obu = std::make_unique<BufferRemovalTimingOBU>(pos);
      break;
    case OBUType::LAYER_CONFIGURATION_RECORD:
      obu = std::make_unique<LayerConfigurationRecordOBU>(pos);
      break;
    case OBUType::ATLAS_SEGMENT:
      obu = std::make_unique<AtlasSegmentOBU>(pos);
      break;
    case OBUType::OPERATING_POINT_SET:
      obu = std::make_unique<OperatingPointSetOBU>(pos);
      break;
    case OBUType::BRIDGE_FRAME:
      obu = std::make_unique<BridgeFrameOBU>(pos);
      break;
    case OBUType::MSDO:
      obu = std::make_unique<MSDOOBU>(pos);
      break;
    case OBUType::RAS_FRAME:
      obu = std::make_unique<RASFrameOBU>(pos);
      break;
    case OBUType::QM:
      obu = std::make_unique<QMOBU>(pos);
      break;
    case OBUType::FGM:
      obu = std::make_unique<FGMOBU>(pos);
      break;
    case OBUType::CONTENT_INTERPRETATION:
      obu = std::make_unique<ContentInterpretationOBU>(pos);
      break;
    case OBUType::PADDING:
      obu = std::make_unique<PaddingOBU>(pos);
      break;
    default:
      spdlog::warn("Creating UnknownOBU for type {}", to_string(type));
      obu = std::make_unique<UnknownOBU>(pos);
      break;
  }

  // Parse the OBU
  ifs.seekg(pos.header_pos);
  if (!obu->parse(ifs)) {
    spdlog::error("Failed to parse {} at position {}", to_string(type),
                  static_cast<long long>(pos.header_pos));
    return nullptr;
  }

  return obu;
}

bool BaseOBU::parse(std::ifstream& ifs) {
  // Parse header
  if (!header_.parse(ifs)) {
    return false;
  }

  // Seek to payload
  ifs.seekg(position_.payload_pos);

  // Parse payload (implemented by derived classes)
  if (!parse_payload(ifs)) {
    spdlog::error("Failed to parse payload for {}", type_name());
    return false;
  }

  spdlog::debug("Successfully parsed {} (size={})", type_name(), position_.payload_size);
  return true;
}

json BaseOBU::to_json() const {
  return json{
    {"type_name", type_name()}, {"position", position_.to_json()}, {"header", header_.to_json()}};
}

std::string BaseOBU::type_name() const {
  return to_string(header_.get_obu_type());
}

bool BaseOBU::skip_payload(std::ifstream& ifs) {
  ifs.seekg(position_.end_pos);
  return ifs.good();
}

}  // namespace av2_obu
