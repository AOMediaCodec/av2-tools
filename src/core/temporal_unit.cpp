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

#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/temporal_unit.h>

#include <set>

namespace av2_obu {

namespace {

// Coded frame OBU types per AV2 spec (anything carrying frame data for an extended layer)
bool is_coded_frame_obu(OBUType t) {
  return is_tile_group(t) || is_tip_frame(t) || is_sef(t) || t == OBUType::BRIDGE_FRAME;
}

}  // namespace

bool TemporalUnit::is_sync_sample() const {
  // Per av2-isobmff this TU is a sync sample if every coded extended layer unit present is a CLK
  // We rely on AV2 TU ordering: within an xlayer's CLU, frames come in
  // ascending mlayer order, so the FIRST coded frame OBU we encounter per
  // xlayer in bitstream order IS the first frame at that xlayer's lowest present mlayer 
  std::set<uint8_t> seen_xlayers;
  bool any_frame = false;
  for (const auto* obu : obus_) {
    if (!is_coded_frame_obu(obu->type())) continue;
    uint8_t xid = obu->header().get_xlayer_id();
    if (!seen_xlayers.insert(xid).second) continue;  // already saw this xlayer's first frame
    if (obu->type() != OBUType::CLK) return false;
    any_frame = true;
  }
  return any_frame;
}

std::vector<const BaseOBU*> TemporalUnit::sample_obus(bool keep_td) const {
  std::vector<const BaseOBU*> out;
  for (const auto* obu : obus_) {
    auto t = obu->type();
    if (is_config_obu(t)) continue;         // carried in configOBUs, not samples
    if (t == OBUType::PADDING) continue;     // forbidden in samples per av2-isobmff
    if (t == OBUType::TEMPORAL_DELIMITER && !keep_td) continue;  // sample boundary == TU boundary
    out.push_back(obu);
  }
  return out;
}

size_t TemporalUnit::total_size() const {
  size_t total = 0;
  for (const auto* obu : obus_) {
    const auto& pos = obu->position();
    total += pos.size_field_len + pos.header_len + pos.payload_size;
  }
  return total;
}

nlohmann::json TemporalUnit::to_json() const {
  nlohmann::json j;
  j["index"] = index_;
  j["obu_count"] = obu_count();
  j["is_sync_sample"] = is_sync_sample();
  j["display_order"] = display_order_;
  j["total_size"] = total_size();

  nlohmann::json obu_array = nlohmann::json::array();
  for (const auto* obu : obus_) {
    obu_array.push_back(to_string(obu->type()));
  }
  j["obus"] = obu_array;

  return j;
}

}  // namespace av2_obu
