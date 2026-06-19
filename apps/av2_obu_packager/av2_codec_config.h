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
#include <vector>

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/base_obu.h>

namespace av2_obu {

// AV2CodecConfigurationBox ('av2C') is the codec config box for AV2 in ISOBMFF.
struct AV2CodecConfigurationBox {
  uint32_t configurationVersion = 1;

  uint32_t seq_profile_idc = 0;                          // serialized as 5 bits
  uint32_t seq_level_idx = 0;                            // 5 bits
  uint32_t seq_tier = 0;                                 // 1 bit

  uint32_t chroma_format_idc = 0;                        // 3 bits
  uint32_t bit_depth_idc = 0;                            // 3 bits

  uint32_t monotonic_output_order_flag = 0;              // 1 bit
  uint32_t still_picture = 0;                            // 1 bit
  uint32_t film_grain_params_present = 0;                // 1 bit
  uint32_t seq_initial_display_delay_present_flag = 0;   // 1 bit
  uint32_t seq_initial_display_delay_minus_1 = 0;        // 4 bits
                                                         // reserved1 (7 bits) is implicit

  uint32_t max_frame_width_minus_1 = 0;                  // 16 bits
  uint32_t max_frame_height_minus_1 = 0;                 // 16 bits

  // One entry per OBU embedded in configOBUs[]. Holds the OBU pointer for
  // inspection (type, position, JSON) and the pre-read Annex B-framed bytes.
  struct ConfigEntry {
    const BaseOBU* obu = nullptr;
    std::vector<uint8_t> bytes;  // leb128 size + header + payload
  };

  // Ordered list. The first entry must be the active Sequence Header OBU
  // per av2-isobmff. Non-owning OBU pointers — caller keeps OBUParser alive.
  std::vector<ConfigEntry> config_obus;

  // Default constructor for tests; append_config_obu() requires bind_input_file()
  // to have been called first (or use the file-binding constructor below).
  AV2CodecConfigurationBox() = default;

  // Bind to an input bitstream. The ifstream must outlive this object.
  explicit AV2CodecConfigurationBox(std::ifstream& input_file) : input_file_(&input_file) {}

  void bind_input_file(std::ifstream& input_file) { input_file_ = &input_file; }

  // Populate scalar fields from a parsed sequence header. The SH OBU and any
  // additional configOBUs entries are appended separately via append_config_obu.
  AV2CodecConfigurationBox& set_from_sequence_header(const AV2SequenceHeader& sh);

  // Read obu's Annex B-framed bytes from the bound input file (preserving
  // stream position) and append a ConfigEntry. Returns false on I/O error or
  // if no input file has been bound.
  bool append_config_obu(const BaseOBU& obu);

  // Bit-pack scalars + concatenate config_obus[].bytes. Pure const operation.
  std::vector<uint8_t> serialize() const;

private:
  std::ifstream* input_file_ = nullptr;
};

}  // namespace av2_obu
