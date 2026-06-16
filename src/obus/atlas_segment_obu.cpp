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

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/atlas_segment_obu.h>

namespace av2_obu {

namespace {

static const char* mode_name(AtlasSegmentOBU::ModeIdc m) {
  switch (m) {
    case AtlasSegmentOBU::ModeIdc::ENHANCED_ATLAS:        return "ENHANCED_ATLAS";
    case AtlasSegmentOBU::ModeIdc::BASIC_ATLAS:           return "BASIC_ATLAS";
    case AtlasSegmentOBU::ModeIdc::SINGLE_ATLAS:          return "SINGLE_ATLAS";
    case AtlasSegmentOBU::ModeIdc::MULTISTREAM_ATLAS:     return "MULTISTREAM_ATLAS";
    case AtlasSegmentOBU::ModeIdc::MULTISTREAM_ALPHA_ATLAS: return "MULTISTREAM_ALPHA_ATLAS";
    default:                                               return "UNKNOWN";
  }
}

// ats_region_info() — returns NumRegionsInAtlas
static uint32_t parse_region_info(BitstreamReader& br,
                                  AtlasSegmentOBU::EnhancedAtlasInfo& info) {
  info.num_region_columns_minus1 = br.read_uvlc();
  info.num_region_rows_minus1    = br.read_uvlc();
  info.uniform_spacing_flag      = br.read_bit();

  if (!info.uniform_spacing_flag) {
    uint32_t num_cols = info.num_region_columns_minus1 + 1;
    uint32_t num_rows = info.num_region_rows_minus1 + 1;
    info.column_width_minus1.resize(num_cols);
    info.row_height_minus1.resize(num_rows);
    for (uint32_t i = 0; i < num_cols; i++)
      info.column_width_minus1[i] = br.read_uvlc();
    for (uint32_t i = 0; i < num_rows; i++)
      info.row_height_minus1[i] = br.read_uvlc();
  } else {
    info.region_width_minus1  = br.read_uvlc();
    info.region_height_minus1 = br.read_uvlc();
  }
  return (info.num_region_columns_minus1 + 1) * (info.num_region_rows_minus1 + 1);
}

// ats_region_to_segment_mapping() — returns numSegments
static uint32_t parse_region_to_segment_mapping(BitstreamReader& br,
                                                  AtlasSegmentOBU::EnhancedAtlasInfo& info,
                                                  uint32_t num_regions) {
  info.single_region_per_segment_flag = br.read_bit();
  if (!info.single_region_per_segment_flag) {
    info.num_atlas_segments_minus1 = br.read_uvlc();
    uint32_t n = info.num_atlas_segments_minus1 + 1;
    info.segment_regions.resize(n);
    for (uint32_t i = 0; i < n; i++) {
      info.segment_regions[i].top_left_col         = br.read_uvlc();
      info.segment_regions[i].top_left_row         = br.read_uvlc();
      info.segment_regions[i].bottom_right_col_off = br.read_uvlc();
      info.segment_regions[i].bottom_right_row_off = br.read_uvlc();
    }
    return n;
  } else {
    info.num_atlas_segments_minus1 = num_regions - 1;
    return num_regions;
  }
}

// ats_label_segment_info()
static void parse_label_segment_info(BitstreamReader& br, AtlasSegmentOBU::LabelSegmentInfo& label,
                                     uint32_t num_segments) {
  label.signaled_ids_flag = br.read_bit();
  if (label.signaled_ids_flag) {
    label.segment_ids.resize(num_segments);
    for (uint32_t i = 0; i < num_segments; i++)
      label.segment_ids[i] = br.read_bits(8);
  }
}

}  // namespace

bool AtlasSegmentOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing ATLAS_SEGMENT payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read ATLAS_SEGMENT payload");
    return false;
  }

  try {
    BitstreamReader br(raw_payload_);

    atlas_segment_id_ = br.read_bits(3);
    mode_idc_         = static_cast<ModeIdc>(br.read_uvlc());

    uint32_t num_segments = 0;

    switch (mode_idc_) {
      case ModeIdc::ENHANCED_ATLAS: {
        uint32_t num_regions = parse_region_info(br, enhanced_);
        num_segments = parse_region_to_segment_mapping(br, enhanced_, num_regions);
        break;
      }
      case ModeIdc::BASIC_ATLAS: {
        basic_stream_id_present_ = br.read_bit();
        basic_width_             = br.read_uvlc();
        basic_height_            = br.read_uvlc();
        uint32_t n = br.read_uvlc() + 1;  // ats_num_atlas_segments_minus1 + 1
        num_segments = n;
        segments_.resize(n);
        for (uint32_t i = 0; i < n; i++) {
          if (basic_stream_id_present_)
            segments_[i].input_stream_id = br.read_bits(5);
          segments_[i].top_left_x = br.read_uvlc();
          segments_[i].top_left_y = br.read_uvlc();
          segments_[i].width      = br.read_uvlc();
          segments_[i].height     = br.read_uvlc();
        }
        break;
      }
      case ModeIdc::SINGLE_ATLAS: {
        nominal_width_minus1_  = br.read_uvlc();
        nominal_height_minus1_ = br.read_uvlc();
        num_segments = 1;
        break;
      }
      case ModeIdc::MULTISTREAM_ATLAS:
      case ModeIdc::MULTISTREAM_ALPHA_ATLAS: {
        bool with_alpha = (mode_idc_ == ModeIdc::MULTISTREAM_ALPHA_ATLAS);
        msi_width_  = br.read_uvlc();
        msi_height_ = br.read_uvlc();
        uint32_t n  = br.read_uvlc() + 1;  // ats_msi_num_atlas_segments_minus1 + 1
        num_segments = n;
        if (with_alpha)
          msi_alpha_segments_present_ = br.read_bit();
        msi_background_present_ = br.read_bit();
        if (msi_background_present_) {
          msi_bg_r_ = br.read_bits(8);
          msi_bg_g_ = br.read_bits(8);
          msi_bg_b_ = br.read_bits(8);
        }
        segments_.resize(n);
        for (uint32_t i = 0; i < n; i++) {
          segments_[i].input_stream_id = br.read_bits(5);
          segments_[i].top_left_x      = br.read_uvlc();
          segments_[i].top_left_y      = br.read_uvlc();
          segments_[i].width           = br.read_uvlc();
          segments_[i].height          = br.read_uvlc();
          if (with_alpha && msi_alpha_segments_present_ && i != n - 1)
            segments_[i].alpha_segment_flag = br.read_bit();
        }
        break;
      }
      default:
        spdlog::warn("ATLAS_SEGMENT: unknown mode_idc={}", static_cast<uint32_t>(mode_idc_));
        return false;
    }

    parse_label_segment_info(br, label_, num_segments);

    if (!parse_obu_trailing_bits(br))
      return false;

  } catch (const std::runtime_error& e) {
    spdlog::error("ATLAS_SEGMENT parse error: {}", e.what());
    return false;
  }

  spdlog::debug("ATLAS_SEGMENT: id={}, mode={}", atlas_segment_id_, mode_name(mode_idc_));
  return true;
}

json AtlasSegmentOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "ATLAS_SEGMENT";

  if (!parsed_) return j;

  j["atlas_segment_id"]            = atlas_segment_id_;
  j["ats_atlas_segment_mode_idc"]  = static_cast<uint32_t>(mode_idc_);
  j["ats_atlas_segment_mode_name"] = mode_name(mode_idc_);

  switch (mode_idc_) {
    case ModeIdc::ENHANCED_ATLAS: {
      json e;
      e["ats_num_region_columns_minus_1"] = enhanced_.num_region_columns_minus1;
      e["ats_num_region_rows_minus_1"]    = enhanced_.num_region_rows_minus1;
      e["ats_uniform_spacing_flag"]      = enhanced_.uniform_spacing_flag;
      if (!enhanced_.uniform_spacing_flag) {
        e["ats_column_width_minus_1"] = enhanced_.column_width_minus1;
        e["ats_row_height_minus_1"]   = enhanced_.row_height_minus1;
      } else {
        e["ats_region_width_minus_1"]  = enhanced_.region_width_minus1;
        e["ats_region_height_minus_1"] = enhanced_.region_height_minus1;
      }
      e["ats_single_region_per_atlas_segment_flag"] = enhanced_.single_region_per_segment_flag;
      e["ats_num_atlas_segments_minus_1"]            = enhanced_.num_atlas_segments_minus1;
      if (!enhanced_.single_region_per_segment_flag) {
        json regions = json::array();
        for (const auto& r : enhanced_.segment_regions) {
          regions.push_back({{"ats_top_left_region_column", r.top_left_col},
                             {"ats_top_left_region_row", r.top_left_row},
                             {"ats_bottom_right_region_column_off", r.bottom_right_col_off},
                             {"ats_bottom_right_region_row_off", r.bottom_right_row_off}});
        }
        e["segment_regions"] = regions;
      }
      j["enhanced_atlas_info"] = e;
      break;
    }
    case ModeIdc::BASIC_ATLAS: {
      j["ats_stream_id_present"]          = basic_stream_id_present_;
      j["ats_width"]                      = basic_width_;
      j["ats_height"]                     = basic_height_;
      j["ats_num_atlas_segments_minus_1"]  = segments_.empty() ? 0 : (uint32_t)segments_.size() - 1;
      json segs = json::array();
      for (const auto& s : segments_) {
        json seg;
        if (basic_stream_id_present_)
          seg["ats_input_stream_id"] = s.input_stream_id;
        seg["ats_segment_top_left_pos_x"] = s.top_left_x;
        seg["ats_segment_top_left_pos_y"] = s.top_left_y;
        seg["ats_segment_width"]          = s.width;
        seg["ats_segment_height"]         = s.height;
        segs.push_back(seg);
      }
      j["segments"] = segs;
      break;
    }
    case ModeIdc::SINGLE_ATLAS:
      j["ats_nominal_width_minus_1"]  = nominal_width_minus1_;
      j["ats_nominal_height_minus_1"] = nominal_height_minus1_;
      break;
    case ModeIdc::MULTISTREAM_ATLAS:
    case ModeIdc::MULTISTREAM_ALPHA_ATLAS: {
      j["ats_msi_width"]  = msi_width_;
      j["ats_msi_height"] = msi_height_;
      j["ats_msi_num_atlas_segments_minus_1"] =
          segments_.empty() ? 0 : (uint32_t)segments_.size() - 1;
      if (mode_idc_ == ModeIdc::MULTISTREAM_ALPHA_ATLAS)
        j["ats_msi_alpha_segments_present_flag"] = msi_alpha_segments_present_;
      j["ats_msi_background_info_present_flag"] = msi_background_present_;
      if (msi_background_present_) {
        j["ats_msi_background_red_value"]   = msi_bg_r_;
        j["ats_msi_background_green_value"] = msi_bg_g_;
        j["ats_msi_background_blue_value"]  = msi_bg_b_;
      }
      json segs = json::array();
      for (const auto& s : segments_) {
        json seg;
        seg["ats_msi_input_stream_id"]        = s.input_stream_id;
        seg["ats_msi_segment_top_left_pos_x"] = s.top_left_x;
        seg["ats_msi_segment_top_left_pos_y"] = s.top_left_y;
        seg["ats_msi_segment_width"]          = s.width;
        seg["ats_msi_segment_height"]         = s.height;
        if (mode_idc_ == ModeIdc::MULTISTREAM_ALPHA_ATLAS)
          seg["ats_msi_alpha_segment_flag"] = s.alpha_segment_flag;
        segs.push_back(seg);
      }
      j["segments"] = segs;
      break;
    }
    default:
      break;
  }

  json label;
  label["ats_signaled_atlas_segment_ids_flag"] = label_.signaled_ids_flag;
  if (label_.signaled_ids_flag)
    label["ats_atlas_segment_ids"] = label_.segment_ids;
  j["label_segment_info"] = label;

  return j;
}

}  // namespace av2_obu
