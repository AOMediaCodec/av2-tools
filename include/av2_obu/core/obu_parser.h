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
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/temporal_unit.h>

using json = nlohmann::ordered_json;

namespace av2_obu {

class OBUParser {
public:
  OBUParser() = default;

  // Parse a bitstream file and build index
  // Returns true on success, false on error
  bool parse_file(const std::string& filename);

  // Get all parsed OBUs
  const std::vector<std::unique_ptr<BaseOBU>>& obus() const { return obus_; }

  // Temporal unit access
  const std::vector<TemporalUnit>& temporal_units() const { return temporal_units_; }
  size_t temporal_unit_count() const { return temporal_units_.size(); }
  const TemporalUnit& temporal_unit(size_t index) const { return temporal_units_[index]; }

  // Configure temporal unit detection
  struct TemporalUnitOptions {
    enum class Mode {
      kAuto,
      kTemporalDelimiter,
      kOrderHint,
      kFrameHeuristic
    };
    Mode mode = Mode::kAuto;
    bool include_temporal_delimiters = true;
  };

  void set_temporal_unit_options(const TemporalUnitOptions& opts) { tu_options_ = opts; }
  const TemporalUnitOptions& temporal_unit_options() const { return tu_options_; }

  // Export all OBUs to JSON
  json to_json() const;

  // Dump OBU information (human-readable)
  void dump() const;

  // Dump OBU information as JSON
  void dump_json(std::ostream& os, int indent = 2) const;

  // Accessors
  const std::string& current_file() const { return current_file_; }
  size_t obu_count() const { return obus_.size(); }

  // Get statistics
  struct Statistics {
    // Basic statistics
    size_t total_obus = 0;
    size_t total_bytes = 0;
    std::map<OBUType, size_t> obu_type_counts;

    // Sequence header analysis
    struct SequenceHeaderInfo {
      size_t count = 0;
      bool has_changes = false;              // Do sequence headers differ?
      std::vector<size_t> change_positions;  // OBU indices where SH changes
    } sequence_headers;

    // Temporal structure
    struct TemporalInfo {
      bool has_temporal_delimiters = false;
      size_t td_count = 0;
    } temporal;

    // Frame analysis
    struct FrameInfo {
      size_t total_frames = 0;
      size_t keyframe_count = 0;
    } frames;

    // Layer analysis
    struct LayerInfo {
      bool is_single_layer = true;
      std::set<uint8_t> mlayer_ids;
      std::set<uint8_t> xlayer_ids;
      std::set<uint8_t> tlayer_ids;
    } layers;

    // Metadata analysis
    struct MetadataInfo {
      bool has_metadata = false;
      // TODO: Implement proper global vs per-frame detection
      bool has_global_metadata = false;
      bool has_per_frame_metadata = false;
      std::set<MetadataType> metadata_types;
    } metadata;

    // Config OBUs
    struct ConfigInfo {
      bool has_layer_config = false;
      bool has_operating_point_set = false;
    } config;
  };
  Statistics get_statistics() const;

private:
  // Scan the file and create OBU objects
  bool scan_file(std::ifstream& ifs);

  // Read LEB128 size field (Annex B framing)
  // Returns number of bytes read (0 on error)
  uint32_t read_annex_b_size(std::ifstream& ifs, uint32_t& value);

  void build_temporal_units();
  void build_tus_td_based();
  void build_tus_frame_based();
  bool is_config_obu(const BaseOBU* obu) const;

  std::string current_file_;
  std::vector<std::unique_ptr<BaseOBU>> obus_;
  std::vector<TemporalUnit> temporal_units_;
  TemporalUnitOptions tu_options_;
};

}  // namespace av2_obu
