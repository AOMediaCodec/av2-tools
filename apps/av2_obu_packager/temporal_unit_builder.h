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

#include <memory>
#include <vector>

#include "packaging_strategy.h"
#include <av2_obu/core/base_obu.h>

namespace av2_obu {

// Represents one temporal unit (one MP4 sample)
class TemporalUnit {
public:
  void add_obu(const BaseOBU* obu) { obus_.push_back(obu); }

  const std::vector<const BaseOBU*>& obus() const { return obus_; }
  size_t obu_count() const { return obus_.size(); }
  bool empty() const { return obus_.empty(); }

  // Check if this TU contains a keyframe
  bool is_keyframe() const;

  // Get total size of all OBUs in Annex B format
  size_t get_total_size() const;

private:
  std::vector<const BaseOBU*> obus_;  // Non-owning pointers
};

// Groups parsed OBUs into temporal units
class TemporalUnitBuilder {
public:
  // Build temporal units from parsed OBUs
  std::vector<TemporalUnit> build(const std::vector<std::unique_ptr<BaseOBU>>& obus,
                                  const PackagingStrategy& strategy);

private:
  // Build using temporal delimiter boundaries
  std::vector<TemporalUnit> build_td_based(const std::vector<std::unique_ptr<BaseOBU>>& obus,
                                           bool drop_tds);

  // Build using simple frame-based hack (one frame = one TU)
  std::vector<TemporalUnit> build_frame_based(const std::vector<std::unique_ptr<BaseOBU>>& obus,
                                              bool drop_tds);

  // Helper: check if OBU is a frame type
  bool is_frame_obu(const BaseOBU* obu) const;

  // Helper: check if OBU should be included in samples
  bool should_include_in_sample(const BaseOBU* obu, const PackagingStrategy& strategy) const;
};

}  // namespace av2_obu
