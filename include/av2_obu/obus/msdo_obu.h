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

// Multi Stream Decoder Operation OBU (OBU_MSDO) (See also LCR)
class MSDOOBU : public BaseOBU {
public:
  explicit MSDOOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "MSDO"; }

  // Per-stream info
  struct StreamInfo {
    uint32_t sub_xlayer_id = 0;
    uint32_t sub_stream_max_profile = 0;
    uint32_t sub_stream_max_level = 0;
    uint32_t sub_stream_max_tier = 0;
  };

  uint32_t num_streams() const { return num_streams_; }
  uint32_t multistream_profile_idc() const { return multistream_profile_idc_; }
  uint32_t multistream_level_idx() const { return multistream_level_idx_; }
  uint32_t multistream_tier() const { return multistream_tier_; }
  const std::vector<StreamInfo>& streams() const { return streams_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t num_streams_ = 0;
  uint32_t multistream_profile_idc_ = 0;
  uint32_t multistream_level_idx_ = 0;
  uint32_t multistream_tier_ = 0;
  uint32_t multistream_even_allocation_flag_ = 0;
  uint32_t multistream_large_picture_idc_ = 0;
  uint32_t multistream_doh_constraint_flag_ = 0;
  std::vector<StreamInfo> streams_;
};

}  // namespace av2_obu
