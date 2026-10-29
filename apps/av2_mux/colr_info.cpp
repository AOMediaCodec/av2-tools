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

#include "colr_info.h"

#include <algorithm>
#include <map>
#include <memory>

#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/obus/content_interpretation_obu.h>
#include <av2_obu/obus/layer_configuration_record_obu.h>
#include <av2_obu/obus/operating_point_set_obu.h>

namespace av2_obu {

namespace {

// Normalizes both parser.obus() (unique_ptr<BaseOBU>) and TemporalUnit::obus()
// (raw const BaseOBU*) to a raw pointer for shared iteration logic below.
const BaseOBU* as_raw(const std::unique_ptr<BaseOBU>& obu) { return obu.get(); }
const BaseOBU* as_raw(const BaseOBU* obu) { return obu; }

// We can collect a few well-known CICP presets here
const std::map<std::string, ColrInfo>& named_colr_profiles() {
  static const std::map<std::string, ColrInfo> kProfiles = {
    {"bt709",      ColrInfo{1, 1, 1, 0}},   // HD SDR, video range
    {"bt601-ntsc", ColrInfo{6, 6, 6, 0}},   // SD NTSC (SMPTE 170M), video range
    {"bt2020-pq",  ColrInfo{9, 16, 9, 0}},  // HDR10: BT.2020 + PQ + BT.2020 NCL
    {"bt2020-hlg", ColrInfo{9, 18, 9, 0}},  // HLG: BT.2020 + ARIB STD-B67 + BT.2020 NCL
  };
  return kProfiles;
}

ColrInfo make(uint32_t cp, uint32_t tc, uint32_t mc, uint32_t fr) {
  return ColrInfo{cp, tc, mc, fr};
}

template <typename Range>
std::optional<ColrInfo> from_ci(const Range& obus) {
  for (const auto& item : obus) {
    const BaseOBU* obu = as_raw(item);
    if (obu->type() != OBUType::CONTENT_INTERPRETATION) continue;
    auto* ci = dynamic_cast<const ContentInterpretationOBU*>(obu);
    if (!ci || !ci->has_color_description()) continue;
    return make(ci->color_primaries(), ci->transfer_characteristics(), ci->matrix_coefficients(),
                ci->full_range_flag());
  }
  return std::nullopt;
}

template <typename Range>
std::optional<ColrInfo> from_lcr(const Range& obus) {
  std::optional<ColrInfo> best;
  uint32_t best_xid = std::numeric_limits<uint32_t>::max();
  for (const auto& item : obus) {
    const BaseOBU* obu = as_raw(item);
    if (obu->type() != OBUType::LAYER_CONFIGURATION_RECORD) continue;
    auto* lcr = dynamic_cast<const LayerConfigurationRecordOBU*>(obu);
    if (!lcr) continue;
    const auto& gp = lcr->global_payloads();
    const auto& ids = lcr->xlayer_ids();
    for (size_t i = 0; i < gp.size() && i < ids.size(); ++i) {
      const auto& xi = gp[i].xlayer_info;
      if (!xi.color_info_present) continue;
      if (ids[i] < best_xid) {
        best_xid = ids[i];
        best = make(xi.color_info.color_primaries, xi.color_info.transfer_characteristics,
                    xi.color_info.matrix_coefficients, xi.color_info.full_range_flag);
      }
    }
    if (!best && lcr->local_xlayer_info().color_info_present) {
      const auto& ci = lcr->local_xlayer_info().color_info;
      best = make(ci.color_primaries, ci.transfer_characteristics, ci.matrix_coefficients,
                  ci.full_range_flag);
    }
  }
  return best;
}

template <typename Range>
std::optional<ColrInfo> from_ops(const Range& obus) {
  for (const auto& item : obus) {
    const BaseOBU* obu = as_raw(item);
    if (obu->type() != OBUType::OPERATING_POINT_SET) continue;
    auto* ops = dynamic_cast<const OperatingPointSetOBU*>(obu);
    if (!ops) continue;
    for (const auto& op : ops->operating_points()) {
      if (!op.has_color_info) continue;
      return make(op.color_info.color_primaries, op.color_info.transfer_characteristics,
                  op.color_info.matrix_coefficients, op.color_info.full_range_flag);
    }
  }
  return std::nullopt;
}

template <typename Range>
std::optional<ColrInfo> extract_colr_info_impl(const Range& obus) {
  if (auto v = from_ci(obus)) return v;
  if (auto v = from_lcr(obus)) return v;
  if (auto v = from_ops(obus)) return v;
  return std::nullopt;
}

}  // namespace

std::optional<ColrInfo> extract_colr_info(const OBUParser& parser) {
  return extract_colr_info_impl(parser.obus());
}

std::optional<ColrInfo> extract_colr_info_from_tu(const TemporalUnit& tu) {
  return extract_colr_info_impl(tu.obus());
}

std::optional<ColrInfo> colr_profile_by_name(const std::string& name) {
  const auto& m = named_colr_profiles();
  if (auto it = m.find(name); it != m.end()) return it->second;
  return std::nullopt;
}

std::string supported_colr_profile_names() {
  std::string out;
  bool first = true;
  for (const auto& kv : named_colr_profiles()) {
    if (!first) out += ", ";
    out += kv.first;
    first = false;
  }
  return out;
}

}  // namespace av2_obu

