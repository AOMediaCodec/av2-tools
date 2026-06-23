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

#include "av2_codec_config.h"
#include "mux_log.h"

#include <spdlog/spdlog.h>

#include <av2_obu/core/bitstream_writer.h>

namespace av2_obu {

AV2CodecConfigurationBox& AV2CodecConfigurationBox::set_from_sequence_header(
  const AV2SequenceHeader& sh) {
  configurationVersion = 1;
  seq_profile_idc = sh.seq_profile_idc;
  seq_level_idx = sh.seq_level_idx;
  seq_tier = sh.seq_tier;
  chroma_format_idc = static_cast<uint32_t>(sh.chroma_format_idc);
  bit_depth_idc = static_cast<uint32_t>(sh.bit_depth_idc);
  monotonic_output_order_flag = sh.monotonic_output_order_flag;
  still_picture = sh.still_picture;
  film_grain_params_present = sh.film_grain_params_present;
  seq_initial_display_delay_present_flag = sh.seq_initial_display_delay_present_flag;
  seq_initial_display_delay_minus_1 = sh.seq_initial_display_delay_minus_1;
  max_frame_width_minus_1 = static_cast<uint32_t>(sh.max_frame_width_minus_1);
  max_frame_height_minus_1 = static_cast<uint32_t>(sh.max_frame_height_minus_1);
  return *this;
}

bool AV2CodecConfigurationBox::append_config_obu(const BaseOBU& obu) {
  if (!input_file_) {
    MUX_ERROR("append_config_obu: no input file bound");
    return false;
  }

  const auto& pos = obu.position();
  size_t total = pos.size_field_len + pos.header_len + pos.payload_size;

  auto saved_pos = input_file_->tellg();

  input_file_->clear();
  input_file_->seekg(pos.start_pos);
  if (!*input_file_) {
    MUX_ERROR("Failed to seek to OBU at offset {}", static_cast<long long>(pos.start_pos));
    input_file_->clear();
    input_file_->seekg(saved_pos);
    return false;
  }

  ConfigEntry entry;
  entry.obu = &obu;
  entry.bytes.resize(total);
  if (!input_file_->read(reinterpret_cast<char*>(entry.bytes.data()),
                         static_cast<std::streamsize>(total))) {
    MUX_ERROR("Failed to read {} bytes for OBU at offset {}", total,
                  static_cast<long long>(pos.start_pos));
    input_file_->clear();
    input_file_->seekg(saved_pos);
    return false;
  }

  config_obus.push_back(std::move(entry));
  input_file_->clear();
  input_file_->seekg(saved_pos);
  return true;
}

std::vector<uint8_t> AV2CodecConfigurationBox::serialize() const {
  BitstreamWriter w;
  w.write_bits(configurationVersion, 8);
  w.write_bits(seq_profile_idc, 5);
  w.write_bits(seq_level_idx, 5);
  w.write_bits(seq_tier, 1);
  w.write_bits(chroma_format_idc, 3);
  w.write_bits(bit_depth_idc, 3);
  w.write_bits(monotonic_output_order_flag, 1);
  w.write_bits(still_picture, 1);
  w.write_bits(film_grain_params_present, 1);
  w.write_bits(seq_initial_display_delay_present_flag, 1);
  w.write_bits(seq_initial_display_delay_minus_1, 4);
  w.write_bits(0, 7);  // reserved1
  w.write_bits(max_frame_width_minus_1, 16);
  w.write_bits(max_frame_height_minus_1, 16);

  std::vector<uint8_t> out = w.take();
  if (out.size() != 9) {
    MUX_ERROR("av2C prefix size = {} bytes (expected 9)", out.size());
    return {};
  }

  for (const auto& entry : config_obus) {
    out.insert(out.end(), entry.bytes.begin(), entry.bytes.end());
  }
  MUX_DEBUG("Serialized av2C: {} bytes (9 prefix + {} configOBUs)", out.size(),
                config_obus.size());
  return out;
}

}  // namespace av2_obu
