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

// Buffer Removal Timing OBU (OBU_BUFFER_REMOVAL_TIMING)
class BufferRemovalTimingOBU : public BaseOBU {
public:
  explicit BufferRemovalTimingOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "BUFFER_REMOVAL_TIMING"; }

  // Per operating point timing entry
  struct OpTiming {
    uint32_t decoder_model_present = 0;
    uint32_t br_time_op = 0;
  };

  uint32_t br_ops_dependent_flag() const { return br_ops_dependent_flag_; }
  uint32_t br_ops_id() const { return br_ops_id_; }
  uint32_t br_ops_cnt() const { return br_ops_cnt_; }
  uint32_t br_time() const { return br_time_; }
  const std::vector<OpTiming>& op_timings() const { return op_timings_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t br_ops_dependent_flag_ = 0;
  uint32_t br_ops_id_ = 0;
  uint32_t br_ops_cnt_ = 0;
  uint32_t br_time_ = 0;
  std::vector<OpTiming> op_timings_;
};

}  // namespace av2_obu
