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
#include <av2_obu/obus/buffer_removal_timing_obu.h>

namespace av2_obu {

bool BufferRemovalTimingOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing BUFFER_REMOVAL_TIMING payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read BUFFER_REMOVAL_TIMING payload");
    return false;
  }

  BitstreamReader br(raw_payload_);

  // buffer_removal_timing_obu()
  br_ops_dependent_flag_ = br.read_bit();

  if (br_ops_dependent_flag_) {
    br_ops_id_ = br.read_bits(4);
    br_ops_cnt_ = br.read_bits(3);

    op_timings_.resize(br_ops_cnt_);
    for (uint32_t i = 0; i < br_ops_cnt_; i++) {
      op_timings_[i].decoder_model_present = br.read_bit();
      if (op_timings_[i].decoder_model_present) {
        op_timings_[i].br_time_op = br.read_rg(4);
      }
    }
  } else {
    br_time_ = br.read_rg(4);
  }

  // Parse trailing_bits (non-extensible OBU)
  if (!parse_obu_trailing_bits(br))
    return false;

  LIB_DEBUG("BRT: ops_dependent={}, br_time={}", br_ops_dependent_flag_, br_time_);
  return true;
}

json BufferRemovalTimingOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "BUFFER_REMOVAL_TIMING";
  if (!parsed_) return j;

  j["br_ops_dependent_flag"] = br_ops_dependent_flag_;

  if (br_ops_dependent_flag_) {
    j["br_ops_id"] = br_ops_id_;
    j["br_ops_cnt"] = br_ops_cnt_;
    json ops = json::array();
    for (const auto& ot : op_timings_) {
      json entry;
      entry["br_decoder_model_present_op_flag"] = ot.decoder_model_present;
      if (ot.decoder_model_present) {
        entry["br_time_op"] = ot.br_time_op;
      }
      ops.push_back(entry);
    }
    j["op_timings"] = ops;
  } else {
    j["br_time"] = br_time_;
  }

  return j;
}

}  // namespace av2_obu
