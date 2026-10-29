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

// AV2CodecConfigurationBox ('av2C'): the codec config box for AV2 in ISOBMFF.
struct AV2CodecConfigurationBox {
  uint32_t configurationVersion = 1;

  uint32_t seq_profile_idc = 0;                          // 5 bits
  uint32_t seq_level_idx = 0;                            // 5 bits
  uint32_t seq_tier = 0;                                 // 1 bit

  uint32_t chroma_format_idc = 0;                        // 3 bits
  uint32_t bit_depth_idc = 0;                            // 3 bits

  uint32_t monotonic_output_order_flag = 0;              // 1 bit
  uint32_t still_picture = 0;                            // 1 bit
  uint32_t film_grain_params_present = 0;                // 1 bit
  uint32_t seq_initial_display_delay_present_flag = 0;   // 1 bit
  uint32_t seq_initial_display_delay_minus_1 = 0;        // 4 bits

  uint32_t max_frame_width_minus_1 = 0;                  // 16 bits
  uint32_t max_frame_height_minus_1 = 0;                 // 16 bits

  // One entry per OBU in configOBUs[]: keeps the parsed OBU pointer plus its
  // pre-read Annex B-framed bytes. OBU pointer is non-owning.
  struct ConfigEntry {
    const BaseOBU* obu = nullptr;
    std::vector<uint8_t> bytes;
  };

  // Per av2-isobmff the first entry must be the active Sequence Header OBU.
  std::vector<ConfigEntry> config_obus;

  AV2CodecConfigurationBox() = default;
  explicit AV2CodecConfigurationBox(std::ifstream& input_file) : input_file_(&input_file) {}
  void bind_input_file(std::ifstream& input_file) { input_file_ = &input_file; }

  AV2CodecConfigurationBox& set_from_sequence_header(const AV2SequenceHeader& sh);
  bool append_config_obu(const BaseOBU& obu);
  std::vector<uint8_t> serialize() const;

private:
  std::ifstream* input_file_ = nullptr;
};

inline bool operator==(const AV2CodecConfigurationBox& a, const AV2CodecConfigurationBox& b) {
  return a.serialize() == b.serialize();
}
inline bool operator!=(const AV2CodecConfigurationBox& a, const AV2CodecConfigurationBox& b) {
  return !(a == b);
}

}  // namespace av2_obu
