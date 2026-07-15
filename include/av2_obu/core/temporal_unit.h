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

#include <nlohmann/json.hpp>

#include <cstddef>
#include <vector>

namespace av2_obu {

class BaseOBU;

class TemporalUnit {
public:
  size_t index() const { return index_; }
  size_t obu_count() const { return obus_.size(); }
  bool empty() const { return obus_.empty(); }

  // True if this TU is a sync sample per av2-isobmff spec
  bool is_sync_sample() const;

  uint32_t display_order() const { return display_order_; }

  // All OBUs in this TU as parsed from the bitstream (no filtering)
  const std::vector<const BaseOBU*>& obus() const { return obus_; }
  const BaseOBU* obu(size_t index) const { return obus_[index]; }

  // OBUs that go into the ISOBMFF sample bytes: frame data only. Configuration
  // OBUs (sequence headers, LCR, OPS, content interpretation) are excluded -
  // they are carried in configOBUs of the sample entry.
  std::vector<const BaseOBU*> sample_obus(bool keep_td = false) const;

  auto begin() const { return obus_.begin(); }
  auto end() const { return obus_.end(); }

  size_t total_size() const;

  nlohmann::json to_json() const;

private:
  friend class OBUParser;

  size_t index_ = 0;
  std::vector<const BaseOBU*> obus_;
  uint32_t display_order_ = 0;
};

}  // namespace av2_obu
