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

namespace av2_obu {

// Multi-Frame Header OBU (OBU_MULTI_FRAME_HEADER)
class MultiFrameHeaderOBU : public BaseOBU {
public:
  explicit MultiFrameHeaderOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "MULTI_FRAME_HEADER"; }

  // Parsed fields
  uint32_t mfh_seq_header_id() const { return mfh_seq_header_id_; }
  uint32_t mfh_id() const { return mfh_id_; }
  bool has_frame_size() const { return mfh_frame_size_present_flag_; }
  uint32_t frame_width() const { return mfh_frame_width_; }
  uint32_t frame_height() const { return mfh_frame_height_; }
  bool loop_filter_update() const { return mfh_loop_filter_update_; }
  bool seg_info_present() const { return mfh_seg_info_present_flag_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t mfh_seq_header_id_ = 0;
  uint32_t mfh_id_ = 0;

  bool mfh_frame_size_present_flag_ = false;
  uint32_t mfh_frame_width_ = 0;
  uint32_t mfh_frame_height_ = 0;

  bool mfh_loop_filter_update_ = false;
  uint32_t mfh_apply_loop_filter_[4] = {};

  bool mfh_seg_info_present_flag_ = false;

  bool parsed_ = false;
};

}  // namespace av2_obu
