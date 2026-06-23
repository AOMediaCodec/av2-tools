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
#include <av2_obu/core/logging.h>

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/content_interpretation_obu.h>

namespace av2_obu {

bool ContentInterpretationOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing CONTENT_INTERPRETATION payload ({} bytes)", position_.payload_size);

  // Read raw payload into memory for bitstream parsing
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read CONTENT_INTERPRETATION payload");
    return false;
  }

  // Create bitstream reader from payload
  BitstreamReader br(raw_payload_);

  try {
    // Parse main flags
    scan_type_idc_ = br.read_bits(2);
    color_description_present_flag_ = br.read_bit();
    chroma_sample_position_present_flag_ = br.read_bit();
    aspect_ratio_info_present_flag_ = br.read_bit();
    timing_info_present_flag_ = br.read_bit();
    reserved_2bit_ = br.read_bits(2);

    LIB_DEBUG("  scan_type_idc = {}", scan_type_idc_);
    LIB_DEBUG("  color_description_present_flag = {}", color_description_present_flag_);
    LIB_DEBUG("  chroma_sample_position_present_flag = {}",
                  chroma_sample_position_present_flag_);
    LIB_DEBUG("  aspect_ratio_info_present_flag = {}", aspect_ratio_info_present_flag_);
    LIB_DEBUG("  timing_info_present_flag = {}", timing_info_present_flag_);

    // Initialize defaults
    color_primaries_ = CP_UNSPECIFIED;
    transfer_characteristics_ = TC_UNSPECIFIED;
    matrix_coefficients_ = MC_UNSPECIFIED;

    // Parse color description if present
    if (color_description_present_flag_) {
      color_description_idc_ = br.read_rg(2);
      LIB_DEBUG("  color_description_idc = {}", color_description_idc_);

      if (color_description_idc_ == 0) {
        color_primaries_ = br.read_bits(8);
        transfer_characteristics_ = br.read_bits(8);
        matrix_coefficients_ = br.read_bits(8);

        LIB_DEBUG("  color_primaries = {}", color_primaries_);
        LIB_DEBUG("  transfer_characteristics = {}", transfer_characteristics_);
        LIB_DEBUG("  matrix_coefficients = {}", matrix_coefficients_);
      }

      full_range_flag_ = br.read_bit();
      LIB_DEBUG("  full_range_flag = {}", full_range_flag_);
    }

    // Parse chroma sample position if present
    if (chroma_sample_position_present_flag_) {
      chroma_sample_position_top_ = br.read_uvlc();
      LIB_DEBUG("  chroma_sample_position_top = {}", chroma_sample_position_top_);

      if (scan_type_idc_ != 1) {
        chroma_sample_position_bottom_ = br.read_uvlc();
        LIB_DEBUG("  chroma_sample_position_bottom = {}", chroma_sample_position_bottom_);
      } else {
        chroma_sample_position_bottom_ = chroma_sample_position_top_;
        LIB_DEBUG("  chroma_sample_position_bottom = {} (copied from top)",
                      chroma_sample_position_bottom_);
      }
    } else {
      chroma_sample_position_top_ = CSP_UNSPECIFIED;
      chroma_sample_position_bottom_ = CSP_UNSPECIFIED;
    }

    // Parse aspect ratio info if present
    if (aspect_ratio_info_present_flag_) {
      aspect_ratio_idc_ = br.read_bits(8);
      LIB_DEBUG("  aspect_ratio_idc = {}", aspect_ratio_idc_);

      if (aspect_ratio_idc_ == 255) {
        // Extended SAR - explicit width and height
        sar_width_ = br.read_uvlc();
        sar_height_ = br.read_uvlc();
        LIB_DEBUG("  sar_width = {}", sar_width_);
        LIB_DEBUG("  sar_height = {}", sar_height_);
      } else if (aspect_ratio_idc_ < 17) {
        // Predefined aspect ratio from table
        sar_width_ = ASPECT_RATIO_WIDTH[aspect_ratio_idc_];
        sar_height_ = ASPECT_RATIO_HEIGHT[aspect_ratio_idc_];
        LIB_DEBUG("  sar_width = {} (from table)", sar_width_);
        LIB_DEBUG("  sar_height = {} (from table)", sar_height_);
      } else {
        LIB_WARN("  Invalid aspect_ratio_idc = {} (expected 0-16 or 255)", aspect_ratio_idc_);
        sar_width_ = 0;
        sar_height_ = 0;
      }
    }

    // Parse timing info if present
    if (timing_info_present_flag_) {
      if (!timing_info_.parse(br)) {
        LIB_ERROR("Failed to parse timing_info");
        return false;
      }
    }

    // Parse obu_extension_flag + trailing_bits (extensible OBU)
    if (!parse_obu_trailing_bits(br))
      return false;

    LIB_DEBUG("Successfully parsed CONTENT_INTERPRETATION OBU");
    return true;

  } catch (const std::exception& e) {
    LIB_ERROR("Error parsing CONTENT_INTERPRETATION payload: {}", e.what());
    return false;
  }
}

json ContentInterpretationOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "CONTENT_INTERPRETATION";

  // Main flags
  j["scan_type_idc"] = scan_type_idc_;
  j["color_description_present_flag"] = color_description_present_flag_;
  j["chroma_sample_position_present_flag"] = chroma_sample_position_present_flag_;
  j["aspect_ratio_info_present_flag"] = aspect_ratio_info_present_flag_;
  j["timing_info_present_flag"] = timing_info_present_flag_;

  // Color description
  if (color_description_present_flag_) {
    json color_desc;
    color_desc["color_description_idc"] = color_description_idc_;
    color_desc["color_primaries"] = color_primaries_;
    color_desc["transfer_characteristics"] = transfer_characteristics_;
    color_desc["matrix_coefficients"] = matrix_coefficients_;
    color_desc["full_range_flag"] = full_range_flag_;
    j["color_description"] = color_desc;
  }

  // Chroma sample position
  if (chroma_sample_position_present_flag_) {
    json chroma_pos;
    chroma_pos["chroma_sample_position_top"] = chroma_sample_position_top_;
    chroma_pos["chroma_sample_position_bottom"] = chroma_sample_position_bottom_;
    j["chroma_sample_position"] = chroma_pos;
  }

  // Aspect ratio
  if (aspect_ratio_info_present_flag_) {
    json aspect_ratio;
    aspect_ratio["aspect_ratio_idc"] = aspect_ratio_idc_;
    aspect_ratio["sar_width"] = sar_width_;
    aspect_ratio["sar_height"] = sar_height_;
    j["aspect_ratio_info"] = aspect_ratio;
  }

  // Timing info
  if (timing_info_present_flag_) {
    j["timing_info"] = timing_info_.to_json();
  }

  return j;
}

}  // namespace av2_obu
