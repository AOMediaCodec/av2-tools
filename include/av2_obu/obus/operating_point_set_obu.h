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

// Operating Point Set OBU (OBU_OPERATING_POINT_SET)
class OperatingPointSetOBU : public BaseOBU {
public:
  explicit OperatingPointSetOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "OPERATING_POINT_SET"; }

  // Per operating point info (lightweight: top-level fields only)
  struct OperatingPoint {
    uint32_t ops_data_size = 0;
    uint32_t ops_xlayer_map = 0;  // 31-bit bitmask (global OPS only)
  };

  uint32_t ops_reset_flag() const { return ops_reset_flag_; }
  uint32_t ops_id() const { return ops_id_; }
  uint32_t ops_cnt() const { return ops_cnt_; }
  uint32_t ops_priority() const { return ops_priority_; }
  uint32_t ops_intent() const { return ops_intent_; }
  const std::vector<OperatingPoint>& operating_points() const { return operating_points_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t ops_reset_flag_ = 0;
  uint32_t ops_id_ = 0;
  uint32_t ops_cnt_ = 0;
  uint32_t ops_priority_ = 0;
  uint32_t ops_intent_ = 0;
  uint32_t ops_intent_present_flag_ = 0;
  uint32_t ops_ptl_present_flag_ = 0;
  uint32_t ops_color_info_present_flag_ = 0;
  uint32_t ops_mlayer_info_idc_ = 0;
  std::vector<OperatingPoint> operating_points_;
  bool parsed_ = false;
};

}  // namespace av2_obu
