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
#include <av2_obu/core/frame_header_info.h>

namespace av2_obu {

// Regular Show Existing Frame OBU (OBU_REGULAR_SEF)
// Re-outputs a previously decoded frame from a reference buffer.
// No new decode data — just frame_to_show_map_idx and order_hint.
class RegularSEFOBU : public BaseOBU {
public:
  explicit RegularSEFOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "REGULAR_SEF"; }

  const FrameHeaderInfo& frame_header() const { return frame_header_; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  FrameHeaderInfo frame_header_;
};

}  // namespace av2_obu
