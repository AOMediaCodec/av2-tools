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

#include <cstdint>
#include <utility>
#include <vector>

#include <av2_obu/core/base_obu.h>

namespace av2_obu {

// FGM OBU
class FGMOBU : public BaseOBU {
public:
  explicit FGMOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "FGM"; }

  // film_grain_model() — one decoded grain model, per set bit in fgm_bit_map.
  struct GrainModel {
    uint32_t fgm_id = 0;
    uint32_t fgm_scale_from_channel0_flag = 0;
    uint32_t fgm_points[3] = {0, 0, 0};                                // per channel: Y, Cb, Cr
    std::vector<std::pair<uint32_t, uint32_t>> fgm_scaling_points[3];  // (x, scale) per channel
    uint32_t fgm_scaling_bits_increment[3] = {0, 0, 0};
    uint32_t fgm_scaling_bits_scaling[3] = {0, 0, 0};
    uint32_t scaling_shift = 0;
    uint32_t ar_coeff_lag = 0;
    std::vector<int32_t> ar_coeffs_y;
    std::vector<int32_t> ar_coeffs_cb;
    std::vector<int32_t> ar_coeffs_cr;
    uint32_t ar_coeff_bits_y = 0;
    uint32_t ar_coeff_bits_cb = 0;
    uint32_t ar_coeff_bits_cr = 0;
    uint32_t ar_coeff_shift = 0;
    uint32_t grain_scale_shift = 0;
    uint32_t cb_mult = 0, cb_luma_mult = 0, cb_offset = 0;
    uint32_t cr_mult = 0, cr_luma_mult = 0, cr_offset = 0;
    uint32_t overlap_flag = 0;
    uint32_t clip_to_restricted_range = 0;
    uint32_t mc_identity = 0;
    uint32_t block_size = 0;
  };

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  uint32_t fgm_bit_map_ = 0;
  uint64_t fgm_chroma_idc_ = 0;
  std::vector<GrainModel> models_;
};

}  // namespace av2_obu
