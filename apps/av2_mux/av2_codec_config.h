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

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "colr_info.h"
#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/obu_parser.h>

namespace av2_obu {

// OutOfBandAV2CodecConfigurationBox ('av2C') per av2-isobmff:
//   aligned(8) class OutOfBandAV2CodecConfigurationBox extends AV2CodecConfigurationBox('av2C') {
//     unsigned int(8) reserved1 = 0;
//     unsigned int(8) config_obus_count_minus1;
//     for (i = 0; i < config_obus_count_minus1 + 1; ++i) {
//       unsigned int(8) config_obu[];
//     }
//   }
struct OutOfBandAV2CodecConfigurationBox {
  uint8_t reserved1 = 0;

  // One entry per OBU in config_obu[]: keeps the parsed OBU pointer (when built
  // from OBUParser) plus its Annex B length-delimited bytes.
  struct ConfigEntry {
    const BaseOBU* obu = nullptr;
    OBUType obu_type = OBUType::UNKNOWN;
    uint8_t xlayer_id = GLOBAL_XLAYER_ID;
    uint8_t mlayer_id = 0;
    std::vector<uint8_t> bytes;
  };

  std::vector<ConfigEntry> config_obus;

  OutOfBandAV2CodecConfigurationBox() = default;
  explicit OutOfBandAV2CodecConfigurationBox(std::ifstream& input_file)
      : input_file_(&input_file) {}
  void bind_input_file(std::ifstream& input_file) { input_file_ = &input_file; }

  bool append_config_obu(const BaseOBU& obu);
  bool append_raw_config_obu(std::vector<uint8_t> bytes);

  // Sorts config_obus into canonical AV2 Temporal Unit order per av2-isobmff #av2c.
  void sort_in_tu_order();

  // Serializes the 2-byte prefix (reserved1, config_obus_count_minus1) + config_obu[].
  std::vector<uint8_t> serialize() const;

  // Concatenated config_obu[] bytes without the 2-byte box header prefix.
  std::vector<uint8_t> concatenated_config_obus() const;

  // Parses an 'av2C' payload (starting at reserved1) and validates each Annex B OBU.
  static bool parse(const uint8_t* data, size_t size, OutOfBandAV2CodecConfigurationBox& out);

private:
  std::ifstream* input_file_ = nullptr;
};

inline bool operator==(const OutOfBandAV2CodecConfigurationBox& a,
                       const OutOfBandAV2CodecConfigurationBox& b) {
  return a.serialize() == b.serialize();
}
inline bool operator!=(const OutOfBandAV2CodecConfigurationBox& a,
                       const OutOfBandAV2CodecConfigurationBox& b) {
  return !(a == b);
}

// InBandAV2CodecConfigurationBox ('av2c') per av2-isobmff:
//   aligned(8) class InBandAV2CodecConfigurationBox extends AV2CodecConfigurationBox('av2c') {
//     unsigned int(3) reserved1 = 0;
//     unsigned int(2) max_num_streams_minus_1;
//     unsigned int(5) max_profile_idc;
//     unsigned int(5) max_level_idx;
//     unsigned int(1) max_tier;
//   }
struct InBandAV2CodecConfigurationBox {
  uint8_t reserved1 = 0;
  uint8_t max_num_streams_minus_1 = 0;
  uint8_t max_profile_idc = 0;
  uint8_t max_level_idx = 0;
  uint8_t max_tier = 0;

  std::vector<uint8_t> serialize() const;
  static bool parse(const uint8_t* data, size_t size, InBandAV2CodecConfigurationBox& out);
  static InBandAV2CodecConfigurationBox from_parser(const OBUParser& parser);
};

// Extended PixelInformationBox ('pixi', version = 0, px_flags = 1) per ISOBMFF 9th Ed. §12.1.11.
struct PixiChannel {
  uint8_t bits_per_channel = 8;
  uint8_t channel_idc = 2;          // Table 8: 0=Unused, 1=Unspecified, 2..4=Colour 1..3, 5=Alpha, 6=Depth, 7=Colour 4
  uint8_t component_format = 0;     // ISO/IEC 23001-17: 0 = unsigned integer
  bool subsampling_flag = false;
  uint8_t subsampling_type = 0;     // Table 9: 0=4:4:4, 1=4:2:2, 2=4:2:0
  uint8_t subsampling_location = 0; // Table 9: 0..5
  std::string channel_label;        // Present iff !channel_label.empty()
};

struct PixiBox {
  uint8_t version = 0;
  uint32_t flags = 1;  // Least significant bit of px_flags enabled per av2-isobmff
  std::vector<PixiChannel> channels;

  // Serializes FullBox payload starting with version (1 byte) + flags (3 bytes).
  std::vector<uint8_t> serialize_payload() const;
  static bool parse_payload(const uint8_t* data, size_t size, PixiBox& out);
  static PixiBox from_parser(const OBUParser& parser);
};

// Builds the RFC 6381 Codecs parameter string per av2-isobmff #codecsparam.
std::string build_codecs_string(const std::string& sample_entry_4cc,
                                const AV2SequenceHeader& first_sh,
                                const PixiBox& pixi,
                                const ColrInfo& colr);

}  // namespace av2_obu
