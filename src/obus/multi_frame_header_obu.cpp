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
#include <av2_obu/obus/multi_frame_header_obu.h>

namespace av2_obu {

bool MultiFrameHeaderOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing MULTI_FRAME_HEADER OBU payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read MULTI_FRAME_HEADER payload");
    return false;
  }

  BitstreamReader br(raw_payload_);

  // multi_frame_header_obu() syntax
  mfh_seq_header_id_ = br.read_uvlc();
  uint32_t mfh_id_minus_1 = br.read_uvlc();
  mfh_id_ = mfh_id_minus_1 + 1;

  // Optional frame size override
  mfh_frame_size_present_flag_ = br.read_bit();
  if (mfh_frame_size_present_flag_) {
    uint32_t w_bits = br.read_bits(4) + 1;
    uint32_t h_bits = br.read_bits(4) + 1;
    mfh_frame_width_ = br.read_bits(w_bits) + 1;
    mfh_frame_height_ = br.read_bits(h_bits) + 1;
  }

  // Loop filter update flag
  mfh_loop_filter_update_ = br.read_bit();
  if (mfh_loop_filter_update_) {
    for (int i = 0; i < 4; i++) {
      mfh_apply_loop_filter_[i] = br.read_bit();
    }
  }

  // Segmentation info presence flag
  mfh_seg_info_present_flag_ = br.read_bit();
  if (mfh_seg_info_present_flag_) {
    // TODO: Parse seg_info() for deep mode
    // For lightweight mode, just note presence — seg_info() is complex
    // and not needed for packaging.
    spdlog::debug("MFH: seg_info present but not parsed (lightweight mode)");
  }

  spdlog::debug("MFH: id={}, seq_header_id={}, frame_size={}",
                mfh_id_, mfh_seq_header_id_,
                mfh_frame_size_present_flag_ ? std::to_string(mfh_frame_width_) + "x" +
                                                   std::to_string(mfh_frame_height_)
                                             : "default");
  return true;
}

json MultiFrameHeaderOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "MULTI_FRAME_HEADER";
  if (parsed_) {
    j["mfh_seq_header_id"] = mfh_seq_header_id_;
    j["mfh_id"] = mfh_id_;
    j["mfh_frame_size_present_flag"] = mfh_frame_size_present_flag_;
    if (mfh_frame_size_present_flag_) {
      j["mfh_frame_width"] = mfh_frame_width_;
      j["mfh_frame_height"] = mfh_frame_height_;
    }
    j["mfh_loop_filter_update"] = mfh_loop_filter_update_;
    if (mfh_loop_filter_update_) {
      j["mfh_apply_loop_filter"] = {mfh_apply_loop_filter_[0], mfh_apply_loop_filter_[1],
                                    mfh_apply_loop_filter_[2], mfh_apply_loop_filter_[3]};
    }
    j["mfh_seg_info_present_flag"] = mfh_seg_info_present_flag_;
  }
  return j;
}

}  // namespace av2_obu
