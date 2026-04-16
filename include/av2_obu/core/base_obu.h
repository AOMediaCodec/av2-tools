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
#include <memory>
#include <vector>

#include <av2_obu/core/av2_types.h>

using json = nlohmann::ordered_json;

namespace av2_obu {

// Position information for an OBU in a file (Annex B framing)
//
// File structure:
//   [    LEB128 size    ][   OBU Header    ][   OBU Payload   ]
//   ^-- start_pos        ^-- header_pos     ^-- payload_pos   ^-- end_pos
//   |<- size_field_len->|<-  header_len  ->|<- payload_size ->|
//                        |<----------- obu_size ------------->|
//   |<--------------------- total size ---------------------->|
//
struct OBUPosition {
  std::streampos start_pos{};    // Start of Annex B frame (size field)
  std::streampos header_pos{};   // Start of OBU header
  std::streampos payload_pos{};  // Start of OBU payload
  std::streampos end_pos{};      // End of OBU (one-past-the-end)

  uint32_t size_field_len = 0;  // LEB128 size field length (Annex B specific)
  uint32_t header_len = 0;      // OBU header length
  uint32_t payload_size = 0;    // OBU payload length

  json to_json() const;
};

// OBU Header (AV2 format)
class OBUHeader {
public:
  OBUHeader() = default;

  // Parse AV2 header from stream
  bool parse(std::ifstream& ifs);

  // Getters
  OBUType get_obu_type() const { return to_obu_type(obu_type_); }
  uint32_t get_obu_type_raw() const { return obu_type_; }
  uint32_t get_tlayer_id() const { return obu_tlayer_id_; }
  uint32_t get_mlayer_id() const { return obu_mlayer_id_; }
  uint32_t get_xlayer_id() const { return obu_xlayer_id_; }
  uint32_t get_extension_flag() const { return obu_extension_flag_; }

  // JSON serialization
  json to_json() const;

private:
  uint32_t obu_type_ = 0;
  uint32_t obu_tlayer_id_ = 0;
  uint32_t obu_mlayer_id_ = 0;
  uint32_t obu_xlayer_id_ = 0;
  uint32_t obu_extension_flag_ = 0;
};

// Forward declaration
struct AV2SequenceHeader;

// Base class for all OBUs
class BaseOBU {
public:
  virtual ~BaseOBU() = default;

  // Factory method to create appropriate OBU type
  static std::unique_ptr<BaseOBU> create(std::ifstream& ifs, const OBUPosition& pos,
                                         ParseMode mode = ParseMode::kDeep,
                                         const AV2SequenceHeader* seq_header = nullptr);

  // Parse the OBU (header + payload)
  // Returns true on success, false on error
  virtual bool parse(std::ifstream& ifs);

  // Serialize to JSON
  virtual json to_json() const;

  // Accessors
  const OBUHeader& header() const { return header_; }
  const OBUPosition& position() const { return position_; }
  OBUType type() const { return header_.get_obu_type(); }
  const std::vector<uint8_t>& raw_payload() const { return raw_payload_; }
  ParseMode parse_mode() const { return parse_mode_; }
  void set_parse_mode(ParseMode mode) { parse_mode_ = mode; }
  const AV2SequenceHeader* active_sequence_header() const { return active_seq_header_; }
  void set_active_sequence_header(const AV2SequenceHeader* sh) { active_seq_header_ = sh; }

  // Type name for logging/display
  virtual std::string type_name() const;

protected:
  // Constructor (used by derived classes)
  explicit BaseOBU(const OBUPosition& pos) : position_(pos) {}

  // Parse payload - override in derived classes
  virtual bool parse_payload(std::ifstream& ifs) = 0;

  // Helper: skip payload without parsing
  bool skip_payload(std::ifstream& ifs);

  OBUHeader header_;
  OBUPosition position_;
  ParseMode parse_mode_ = ParseMode::kDeep;
  const AV2SequenceHeader* active_seq_header_ = nullptr;
  std::vector<uint8_t> raw_payload_;  // Store raw bytes if needed
};

}  // namespace av2_obu
