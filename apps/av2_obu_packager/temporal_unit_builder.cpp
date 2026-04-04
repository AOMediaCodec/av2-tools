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

#include "temporal_unit_builder.h"

#include <spdlog/spdlog.h>

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

size_t TemporalUnit::get_total_size() const {
  size_t total = 0;
  for (const auto* obu : obus_) {
    // Annex B format: size_field + header + payload
    total +=
      obu->position().size_field_len + obu->position().header_len + obu->position().payload_size;
  }
  return total;
}

std::vector<TemporalUnit> TemporalUnitBuilder::build(
  const std::vector<std::unique_ptr<BaseOBU>>& obus, const PackagingStrategy& strategy) {
  spdlog::debug("Building temporal units from {} OBUs", obus.size());
  return build_td_based(obus, strategy.drop_temporal_delimiters);
}

std::vector<TemporalUnit> TemporalUnitBuilder::build_td_based(
  const std::vector<std::unique_ptr<BaseOBU>>& obus, bool drop_tds) {
  spdlog::debug("Building TUs using temporal delimiter boundaries");

  std::vector<TemporalUnit> temporal_units;
  TemporalUnit current_tu;

  for (const auto& obu : obus) {
    // Skip config OBUs (they go in sample entry, not samples)
    if (obu->type() == OBUType::SEQUENCE_HEADER ||
        obu->type() == OBUType::LAYER_CONFIGURATION_RECORD ||
        obu->type() == OBUType::OPERATING_POINT_SET) {
      continue;
    }

    // Temporal delimiter marks start of new TU
    if (obu->type() == OBUType::TEMPORAL_DELIMITER) {
      // Finalize previous TU if not empty
      if (!current_tu.empty()) {
        spdlog::debug("  TU #{}: {} OBUs, {} bytes, keyframe: {}", temporal_units.size(),
                      current_tu.obu_count(), current_tu.get_total_size(),
                      current_tu.is_keyframe());
        temporal_units.push_back(std::move(current_tu));
        current_tu = TemporalUnit();
      }

      // Add TD to new TU (unless dropping)
      if (!drop_tds) {
        current_tu.add_obu(obu.get());
      }
    } else {
      // Add OBU to current TU
      current_tu.add_obu(obu.get());
    }
  }

  // Finalize last TU
  if (!current_tu.empty()) {
    spdlog::debug("  TU #{}: {} OBUs, {} bytes, keyframe: {}", temporal_units.size(),
                  current_tu.obu_count(), current_tu.get_total_size(), current_tu.is_keyframe());
    temporal_units.push_back(std::move(current_tu));
  }

  spdlog::info("Built {} temporal units (TD-based)", temporal_units.size());
  return temporal_units;
}

bool TemporalUnitBuilder::should_include_in_sample(const BaseOBU* obu,
                                                   const PackagingStrategy& strategy) const {
  // Never include config OBUs in samples
  if (obu->type() == OBUType::SEQUENCE_HEADER ||
      obu->type() == OBUType::LAYER_CONFIGURATION_RECORD ||
      obu->type() == OBUType::OPERATING_POINT_SET) {
    return false;
  }

  // Drop TDs if requested
  if (obu->type() == OBUType::TEMPORAL_DELIMITER && strategy.drop_temporal_delimiters) {
    return false;
  }

  return true;
}

}  // namespace av2_obu
