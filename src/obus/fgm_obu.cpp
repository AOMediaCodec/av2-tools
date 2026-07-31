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

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/core/logging.h>
#include <av2_obu/obus/fgm_obu.h>

namespace av2_obu {

namespace {

constexpr uint32_t kFgmIdBits = 3;
constexpr uint32_t kMaxFgmNum = 1u << kFgmIdBits;  // 8

// film_grain_model() — Source: av2/decoder/obu_fgm.c
FGMOBU::GrainModel parse_film_grain_model(BitstreamReader& br, uint64_t fgm_chroma_idc) {
  FGMOBU::GrainModel m;

  uint32_t num_channels = (fgm_chroma_idc == CHROMA_FORMAT_400) ? 1 : 3;
  m.fgm_scale_from_channel0_flag = (num_channels > 1) ? br.read_bit() : 0;
  uint32_t num_scaling_channels = m.fgm_scale_from_channel0_flag ? 1 : num_channels;

  for (uint32_t c = 0; c < num_scaling_channels; c++) {
    m.fgm_points[c] = br.read_bits(4);
    if (m.fgm_points[c]) {
      uint32_t bits_incr = br.read_bits(3) + 1;
      uint32_t bits_scal = br.read_bits(2) + 5;
      m.fgm_scaling_bits_increment[c] = bits_incr;
      m.fgm_scaling_bits_scaling[c] = bits_scal;
      uint32_t prev_x = 0;
      for (uint32_t i = 0; i < m.fgm_points[c]; i++) {
        uint32_t increment = static_cast<uint32_t>(br.read_bits(bits_incr));
        uint32_t x = (i == 0) ? increment : prev_x + increment;
        prev_x = x;
        uint32_t scale = static_cast<uint32_t>(br.read_bits(bits_scal));
        m.fgm_scaling_points[c].emplace_back(x, scale);
      }
    }
  }

  m.scaling_shift = br.read_bits(2) + 8;
  m.ar_coeff_lag = br.read_bits(2);

  uint32_t num_pos_luma = 2 * m.ar_coeff_lag * (m.ar_coeff_lag + 1);
  uint32_t num_pos_chroma = num_pos_luma;

  if (m.fgm_points[0]) {
    num_pos_chroma += 1;
    m.ar_coeff_bits_y = br.read_bits(2) + 5;
    int32_t mid_y = 1 << (m.ar_coeff_bits_y - 1);
    for (uint32_t i = 0; i < num_pos_luma; i++)
      m.ar_coeffs_y.push_back(static_cast<int32_t>(br.read_bits(m.ar_coeff_bits_y)) - mid_y);
  }

  if (m.fgm_points[1] || m.fgm_scale_from_channel0_flag) {
    m.ar_coeff_bits_cb = br.read_bits(2) + 5;
    int32_t mid_cb = 1 << (m.ar_coeff_bits_cb - 1);
    for (uint32_t i = 0; i < num_pos_chroma; i++)
      m.ar_coeffs_cb.push_back(static_cast<int32_t>(br.read_bits(m.ar_coeff_bits_cb)) - mid_cb);
  }

  if (m.fgm_points[2] || m.fgm_scale_from_channel0_flag) {
    m.ar_coeff_bits_cr = br.read_bits(2) + 5;
    int32_t mid_cr = 1 << (m.ar_coeff_bits_cr - 1);
    for (uint32_t i = 0; i < num_pos_chroma; i++)
      m.ar_coeffs_cr.push_back(static_cast<int32_t>(br.read_bits(m.ar_coeff_bits_cr)) - mid_cr);
  }

  m.ar_coeff_shift = br.read_bits(2) + 6;
  m.grain_scale_shift = br.read_bits(2);

  if (m.fgm_points[1] > 0) {
    m.cb_mult = br.read_bits(8);
    m.cb_luma_mult = br.read_bits(8);
    m.cb_offset = br.read_bits(9);
  }

  if (m.fgm_points[2] > 0) {
    m.cr_mult = br.read_bits(8);
    m.cr_luma_mult = br.read_bits(8);
    m.cr_offset = br.read_bits(9);
  }

  m.overlap_flag = br.read_bit();
  m.clip_to_restricted_range = br.read_bit();
  m.mc_identity = m.clip_to_restricted_range ? br.read_bit() : 0;
  m.block_size = br.read_bit();

  return m;
}

}  // namespace

bool FGMOBU::parse_payload(std::ifstream& ifs) {
  LIB_DEBUG("Parsing FGM payload ({} bytes)", position_.payload_size);

  // Read raw payload
  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    LIB_ERROR("Failed to read FGM payload");
    return false;
  }

  try {
    BitstreamReader br(raw_payload_);

    fgm_bit_map_ = static_cast<uint32_t>(br.read_bits(kMaxFgmNum));
    fgm_chroma_idc_ = br.read_uvlc();

    for (uint32_t fgm_id = 0; fgm_id < kMaxFgmNum; fgm_id++) {
      if (fgm_bit_map_ & (1u << fgm_id)) {
        GrainModel model = parse_film_grain_model(br, fgm_chroma_idc_);
        model.fgm_id = fgm_id;
        models_.push_back(std::move(model));
      }
    }

    if (!parse_obu_trailing_bits(br))
      return false;

  } catch (const std::runtime_error& e) {
    LIB_ERROR("FGM parse error: {}", e.what());
    return false;
  }

  LIB_DEBUG("FGM: {} model(s), fgm_chroma_idc={}", models_.size(), fgm_chroma_idc_);
  return true;
}

json FGMOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "FGM";

  if (!parsed_)
    return j;

  j["fgm_bit_map"] = fgm_bit_map_;
  j["fgm_chroma_idc"] = fgm_chroma_idc_;

  json models = json::array();
  for (const auto& m : models_) {
    json jm;
    jm["fgm_id"] = m.fgm_id;
    jm["fgm_scale_from_channel0_flag"] = m.fgm_scale_from_channel0_flag;
    jm["fgm_points"] = {m.fgm_points[0], m.fgm_points[1], m.fgm_points[2]};

    json scaling_points = json::array();
    for (int c = 0; c < 3; c++) {
      json points = json::array();
      for (const auto& [x, scale] : m.fgm_scaling_points[c])
        points.push_back({x, scale});
      scaling_points.push_back(points);
    }
    jm["fgm_scaling_points"] = scaling_points;
    jm["fgm_scaling_bits_increment"] = {m.fgm_scaling_bits_increment[0],
                                        m.fgm_scaling_bits_increment[1],
                                        m.fgm_scaling_bits_increment[2]};
    jm["fgm_scaling_bits_scaling"] = {m.fgm_scaling_bits_scaling[0], m.fgm_scaling_bits_scaling[1],
                                      m.fgm_scaling_bits_scaling[2]};

    jm["scaling_shift"] = m.scaling_shift;
    jm["ar_coeff_lag"] = m.ar_coeff_lag;
    jm["ar_coeffs_y"] = m.ar_coeffs_y;
    jm["ar_coeffs_cb"] = m.ar_coeffs_cb;
    jm["ar_coeffs_cr"] = m.ar_coeffs_cr;
    jm["ar_coeff_bits_y"] = m.ar_coeff_bits_y;
    jm["ar_coeff_bits_cb"] = m.ar_coeff_bits_cb;
    jm["ar_coeff_bits_cr"] = m.ar_coeff_bits_cr;
    jm["ar_coeff_shift"] = m.ar_coeff_shift;
    jm["grain_scale_shift"] = m.grain_scale_shift;
    jm["cb_mult"] = m.cb_mult;
    jm["cb_luma_mult"] = m.cb_luma_mult;
    jm["cb_offset"] = m.cb_offset;
    jm["cr_mult"] = m.cr_mult;
    jm["cr_luma_mult"] = m.cr_luma_mult;
    jm["cr_offset"] = m.cr_offset;
    jm["overlap_flag"] = m.overlap_flag;
    jm["clip_to_restricted_range"] = m.clip_to_restricted_range;
    jm["mc_identity"] = m.mc_identity;
    jm["block_size"] = m.block_size;

    models.push_back(jm);
  }
  j["models"] = models;

  return j;
}

}  // namespace av2_obu
