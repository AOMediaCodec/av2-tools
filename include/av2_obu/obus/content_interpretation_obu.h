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

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/base_obu.h>

namespace av2_obu {

// Aspect ratio tables from AV2 spec
constexpr uint32_t ASPECT_RATIO_WIDTH[17] = {0,  1,  12, 10, 16,  40, 24, 20, 32,
                                             80, 18, 15, 64, 160, 4,  3,  2};

constexpr uint32_t ASPECT_RATIO_HEIGHT[17] = {0,  1,  11, 11, 11, 33, 11, 11, 11,
                                              33, 11, 11, 33, 99, 3,  2,  1};

// Content Interpretation OBU
class ContentInterpretationOBU : public BaseOBU {
public:
  explicit ContentInterpretationOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "CONTENT_INTERPRETATION"; }

  // Getters for parsed fields
  uint32_t get_scan_type_idc() const { return scan_type_idc_; }
  bool has_color_description() const { return color_description_present_flag_ != 0; }
  bool has_chroma_sample_position() const { return chroma_sample_position_present_flag_ != 0; }
  bool has_aspect_ratio_info() const { return aspect_ratio_info_present_flag_ != 0; }
  bool has_timing_info() const { return timing_info_present_flag_ != 0; }
  const TimingInfo& get_timing_info() const { return timing_info_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  // Main flags
  uint32_t scan_type_idc_ = 0;
  uint32_t color_description_present_flag_ = 0;
  uint32_t chroma_sample_position_present_flag_ = 0;
  uint32_t aspect_ratio_info_present_flag_ = 0;
  uint32_t timing_info_present_flag_ = 0;
  uint32_t reserved_2bit_ = 0;

  // Color description fields
  uint32_t color_description_idc_ = 0;
  uint32_t color_primaries_ = CP_UNSPECIFIED;
  uint32_t transfer_characteristics_ = TC_UNSPECIFIED;
  uint32_t matrix_coefficients_ = MC_UNSPECIFIED;
  uint32_t full_range_flag_ = 0;

  // Chroma sample position fields
  uint64_t chroma_sample_position_top_ = CSP_UNSPECIFIED;
  uint64_t chroma_sample_position_bottom_ = CSP_UNSPECIFIED;

  // Aspect ratio fields
  uint32_t aspect_ratio_idc_ = 0;
  uint64_t sar_width_ = 0;
  uint64_t sar_height_ = 0;

  // Timing info
  TimingInfo timing_info_;
};

}  // namespace av2_obu
