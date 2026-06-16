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

#include <av2_obu/core/base_obu.h>

#include <vector>

namespace av2_obu {

class AtlasSegmentOBU : public BaseOBU {
public:
  explicit AtlasSegmentOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "ATLAS_SEGMENT"; }

  enum class ModeIdc : uint32_t {
    ENHANCED_ATLAS = 0,
    BASIC_ATLAS = 1,
    SINGLE_ATLAS = 2,
    MULTISTREAM_ATLAS = 3,
    MULTISTREAM_ALPHA_ATLAS = 4,
  };

  struct Segment {
    uint32_t input_stream_id = 0;
    uint32_t top_left_x = 0;
    uint32_t top_left_y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t alpha_segment_flag = 0;
  };

  struct EnhancedAtlasInfo {
    uint32_t num_region_columns_minus_1 = 0;
    uint32_t num_region_rows_minus_1 = 0;
    uint32_t uniform_spacing_flag = 0;
    std::vector<uint32_t> column_width_minus_1;
    std::vector<uint32_t> row_height_minus_1;
    uint32_t region_width_minus_1 = 0;
    uint32_t region_height_minus_1 = 0;
    uint32_t single_region_per_segment_flag = 0;
    uint32_t num_atlas_segments_minus_1 = 0;
    struct SegmentRegion {
      uint32_t top_left_col = 0;
      uint32_t top_left_row = 0;
      uint32_t bottom_right_col_off = 0;
      uint32_t bottom_right_row_off = 0;
    };
    std::vector<SegmentRegion> segment_regions;
  };

  struct LabelSegmentInfo {
    uint32_t signaled_ids_flag = 0;
    std::vector<uint32_t> segment_ids;
  };

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t atlas_segment_id_ = 0;
  ModeIdc mode_idc_ = ModeIdc::SINGLE_ATLAS;

  // Mode 0 — ENHANCED_ATLAS
  EnhancedAtlasInfo enhanced_;

  // Mode 1 — BASIC_ATLAS
  uint32_t basic_stream_id_present_ = 0;
  uint32_t basic_width_ = 0;
  uint32_t basic_height_ = 0;

  // Mode 2 — SINGLE_ATLAS
  uint32_t nominal_width_minus_1_ = 0;
  uint32_t nominal_height_minus_1_ = 0;

  // Modes 3 & 4 — MULTISTREAM_ATLAS / MULTISTREAM_ALPHA_ATLAS
  uint32_t msi_width_ = 0;
  uint32_t msi_height_ = 0;
  uint32_t msi_background_present_ = 0;
  uint32_t msi_bg_r_ = 0;
  uint32_t msi_bg_g_ = 0;
  uint32_t msi_bg_b_ = 0;
  uint32_t msi_alpha_segments_present_ = 0;

  // All modes — per-segment data
  std::vector<Segment> segments_;

  // All modes — label info
  LabelSegmentInfo label_;
};

}  // namespace av2_obu
