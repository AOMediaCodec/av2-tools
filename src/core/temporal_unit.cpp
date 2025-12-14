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

#include <av2_obu/core/temporal_unit.h>

#include <av2_obu/core/base_obu.h>

namespace av2_obu {

bool TemporalUnit::is_keyframe() const {
  for (const auto* obu : obus_) {
    auto type = obu->type();
    if (type == OBUType::CLK || type == OBUType::OLK) {
      return true;
    }
  }
  return false;
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
  j["is_keyframe"] = is_keyframe();
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
