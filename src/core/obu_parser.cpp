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

#include <cstring>
#include <iomanip>
#include <iostream>

#include <av2_obu/core/obu_parser.h>

namespace av2_obu {

bool OBUParser::parse_file(const std::string& filename) {
  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs) {
    spdlog::error("Failed to open file: {}", filename);
    return false;
  }

  // Get file size
  ifs.seekg(0, std::ios::end);
  std::streampos file_size = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  spdlog::debug("Parsing AV2 file: {} ({} bytes)", filename, static_cast<long long>(file_size));

  // Clear previous state
  obus_.clear();
  temporal_units_.clear();
  current_file_ = filename;

  // Scan the file
  if (!scan_file(ifs)) {
    spdlog::error("Failed to parse file: {}", filename);
    return false;
  }

  build_temporal_units();

  spdlog::debug("Successfully parsed {} OBUs, {} temporal units", obus_.size(),
                temporal_units_.size());
  return true;
}

bool OBUParser::scan_file(std::ifstream& ifs) {
  ifs.clear();
  ifs.seekg(0, std::ios::beg);

  size_t obu_index = 0;

  while (ifs.good()) {
    std::streampos record_begin = ifs.tellg();

    // Check if we've reached EOF
    ifs.peek();
    if (ifs.eof()) {
      break;
    }

    spdlog::debug("--- OBU {} at position {} ---", obu_index, static_cast<long long>(record_begin));

    OBUPosition pos;
    pos.start_pos = record_begin;

    // Annex B format: size field first, then header, then payload
    uint32_t total_size_val;
    uint32_t size_field_len = read_annex_b_size(ifs, total_size_val);
    if (size_field_len == 0) {
      spdlog::error("Failed to read size field at position {}",
                    static_cast<long long>(record_begin));
      return false;
    }
    pos.size_field_len = size_field_len;

    pos.header_pos = ifs.tellg();

    // Temporarily parse header to get header length
    OBUHeader temp_header;
    std::streampos header_start = ifs.tellg();
    if (!temp_header.parse(ifs)) {
      spdlog::error("Failed to parse header at position {}", static_cast<long long>(header_start));
      return false;
    }
    pos.header_len = static_cast<uint32_t>(ifs.tellg() - header_start);

    // Calculate positions
    pos.payload_pos = pos.header_pos + static_cast<std::streamoff>(pos.header_len);

    // In AV2, the size field includes the header
    if (total_size_val < pos.header_len) {
      spdlog::error("Total size ({}) is less than header length ({})", total_size_val,
                    pos.header_len);
      return false;
    }
    pos.payload_size = total_size_val - pos.header_len;
    pos.end_pos = pos.payload_pos + static_cast<std::streamoff>(pos.payload_size);

    spdlog::debug("Position info: start={}, header={}, payload={}, end={}, payload_size={}",
                  static_cast<long long>(pos.start_pos), static_cast<long long>(pos.header_pos),
                  static_cast<long long>(pos.payload_pos), static_cast<long long>(pos.end_pos),
                  pos.payload_size);

    // Create OBU object
    auto obu = BaseOBU::create(ifs, pos);
    if (!obu) {
      spdlog::error("Failed to create OBU at position {}", static_cast<long long>(record_begin));
      return false;
    }

    obus_.push_back(std::move(obu));

    // Seek to next OBU
    ifs.seekg(pos.end_pos);
    obu_index++;
  }

  return true;
}

json OBUParser::to_json() const {
  json j = {{"file", current_file_}, {"obu_count", obus_.size()}, {"obus", json::array()}};

  for (const auto& obu : obus_) {
    j["obus"].push_back(obu->to_json());
  }

  return j;
}

void OBUParser::dump() const {
  std::cout << "=== OBU Parser ===" << std::endl;
  std::cout << "File: " << current_file_ << std::endl;
  std::cout << "OBU count: " << obus_.size() << std::endl;
  std::cout << std::endl;

  for (size_t i = 0; i < obus_.size(); ++i) {
    const auto& obu = obus_[i];
    std::cout << "--- OBU #" << i << " ---" << std::endl;
    std::cout << "Type: " << obu->type_name() << std::endl;
    std::cout << "Position: " << static_cast<long long>(obu->position().start_pos) << std::endl;
    std::cout << "Header length: " << obu->position().header_len << std::endl;
    std::cout << "Payload size: " << obu->position().payload_size << std::endl;
    std::cout << std::endl;
  }
}

void OBUParser::dump_json(std::ostream& os, int indent) const {
  os << to_json().dump(indent) << std::endl;
}

OBUParser::Statistics OBUParser::get_statistics() const {
  Statistics stats;
  stats.total_obus = obus_.size();

  // Track sequence headers for comparison
  std::vector<const BaseOBU*> sequence_headers;

  for (size_t i = 0; i < obus_.size(); ++i) {
    const auto& obu = obus_[i];
    auto type = obu->type();

    // Basic statistics
    stats.total_bytes += obu->position().header_len + obu->position().payload_size;
    stats.obu_type_counts[type]++;

    // Layer analysis
    stats.layers.mlayer_ids.insert(obu->header().get_mlayer_id());
    stats.layers.xlayer_ids.insert(obu->header().get_xlayer_id());
    stats.layers.tlayer_ids.insert(obu->header().get_tlayer_id());

    // Type-specific analysis
    switch (type) {
      case OBUType::SEQUENCE_HEADER:
        stats.sequence_headers.count++;
        sequence_headers.push_back(obu.get());
        break;

      case OBUType::TEMPORAL_DELIMITER:
        stats.temporal.has_temporal_delimiters = true;
        stats.temporal.td_count++;
        break;

      case OBUType::CLK:
        stats.frames.total_frames++;
        stats.frames.keyframe_count++;
        break;

      case OBUType::OLK:
      case OBUType::REGULAR_TILE_GROUP:
      case OBUType::LEADING_TILE_GROUP:
        stats.frames.total_frames++;
        break;

      case OBUType::METADATA:
      case OBUType::METADATA_GROUP:
        stats.metadata.has_metadata = true;
        // TODO: Extract metadata type and set metadata_types
        // TODO: Determine if global vs per-frame
        break;

      case OBUType::LAYER_CONFIGURATION_RECORD:
        stats.config.has_layer_config = true;
        break;

      case OBUType::OPERATING_POINT_SET:
        stats.config.has_operating_point_set = true;
        break;

      default:
        break;
    }
  }

  // Determine if single layer
  stats.layers.is_single_layer =
    (stats.layers.mlayer_ids.size() == 1 && stats.layers.xlayer_ids.size() == 1);

  // Check if sequence headers change
  if (sequence_headers.size() > 1) {
    // Compare sequence headers by raw payload bytes
    const auto& first_payload = sequence_headers[0]->raw_payload();

    for (size_t i = 1; i < sequence_headers.size(); ++i) {
      const auto& current_payload = sequence_headers[i]->raw_payload();

      // Compare sizes and content
      if (first_payload.size() != current_payload.size() ||
          std::memcmp(first_payload.data(), current_payload.data(), first_payload.size()) != 0) {
        stats.sequence_headers.has_changes = true;

        // Find the OBU index for this sequence header
        for (size_t j = 0; j < obus_.size(); ++j) {
          if (obus_[j].get() == sequence_headers[i]) {
            stats.sequence_headers.change_positions.push_back(j);
            break;
          }
        }
      }
    }
  }

  return stats;
}

uint32_t OBUParser::read_annex_b_size(std::ifstream& ifs, uint32_t& value) {
  value = 0;
  uint32_t byte_count = 0;

  for (uint32_t i = 0; i < 8; ++i) {
    char byte;
    if (!ifs.read(&byte, 1)) {
      return 0;
    }

    value |= (static_cast<uint32_t>(byte & 0x7F) << (i * 7));
    ++byte_count;

    if (!(byte & 0x80)) {
      break;
    }
  }

  return byte_count;
}

void OBUParser::build_temporal_units() {
  temporal_units_.clear();

  auto mode = tu_options_.mode;
  if (mode == TemporalUnitOptions::Mode::kAuto) {
    bool has_tds = std::any_of(
      obus_.begin(), obus_.end(),
      [](const auto& obu) { return obu->type() == OBUType::TEMPORAL_DELIMITER; });

    mode = has_tds ? TemporalUnitOptions::Mode::kTemporalDelimiter
                   : TemporalUnitOptions::Mode::kFrameHeuristic;

    spdlog::debug("Auto-detected TU mode: {}",
                  mode == TemporalUnitOptions::Mode::kTemporalDelimiter ? "TD-based"
                                                                          : "frame-based");
  }

  switch (mode) {
    case TemporalUnitOptions::Mode::kTemporalDelimiter:
      build_tus_td_based();
      break;
    case TemporalUnitOptions::Mode::kFrameHeuristic:
      build_tus_frame_based();
      break;
    case TemporalUnitOptions::Mode::kOrderHint:
      spdlog::warn("order_hint mode not yet implemented, falling back to frame-based");
      build_tus_frame_based();
      break;
    default:
      break;
  }
}

void OBUParser::build_tus_td_based() {
  TemporalUnit current_tu;
  current_tu.index_ = 0;

  for (const auto& obu : obus_) {
    if (is_config_obu(obu.get())) {
      continue;
    }

    if (obu->type() == OBUType::TEMPORAL_DELIMITER) {
      if (!current_tu.empty()) {
        current_tu.is_keyframe_ = current_tu.is_keyframe();
        temporal_units_.push_back(std::move(current_tu));
        current_tu = TemporalUnit();
        current_tu.index_ = temporal_units_.size();
      }

      if (tu_options_.include_temporal_delimiters) {
        current_tu.obus_.push_back(obu.get());
      }
    } else {
      current_tu.obus_.push_back(obu.get());
    }
  }

  if (!current_tu.empty()) {
    current_tu.is_keyframe_ = current_tu.is_keyframe();
    temporal_units_.push_back(std::move(current_tu));
  }

  spdlog::debug("Built {} temporal units (TD-based)", temporal_units_.size());
}

void OBUParser::build_tus_frame_based() {
  TemporalUnit current_tu;
  current_tu.index_ = 0;

  auto is_frame_obu = [](const BaseOBU* obu) {
    auto type = obu->type();
    return type == OBUType::CLK || type == OBUType::OLK || type == OBUType::REGULAR_TILE_GROUP ||
           type == OBUType::LEADING_TILE_GROUP;
  };

  for (const auto& obu : obus_) {
    if (is_config_obu(obu.get())) {
      continue;
    }

    if (obu->type() == OBUType::TEMPORAL_DELIMITER && !tu_options_.include_temporal_delimiters) {
      continue;
    }

    if (is_frame_obu(obu.get()) && !current_tu.empty()) {
      current_tu.is_keyframe_ = current_tu.is_keyframe();
      temporal_units_.push_back(std::move(current_tu));
      current_tu = TemporalUnit();
      current_tu.index_ = temporal_units_.size();
    }

    current_tu.obus_.push_back(obu.get());
  }

  if (!current_tu.empty()) {
    current_tu.is_keyframe_ = current_tu.is_keyframe();
    temporal_units_.push_back(std::move(current_tu));
  }

  spdlog::debug("Built {} temporal units (frame-based)", temporal_units_.size());
}

bool OBUParser::is_config_obu(const BaseOBU* obu) const {
  auto type = obu->type();
  return type == OBUType::SEQUENCE_HEADER || type == OBUType::LAYER_CONFIGURATION_RECORD ||
         type == OBUType::OPERATING_POINT_SET;
}

}  // namespace av2_obu
