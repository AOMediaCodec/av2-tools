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
#include <av2_obu/core/tile_group_header.h>

namespace av2_obu {

// Leading Tile Group OBU (OBU_LEADING_TILE_GROUP)
// Uses tile_group_obu() syntax, same as REGULAR_TILE_GROUP.
class LeadingTileGroupOBU : public BaseOBU {
public:
  explicit LeadingTileGroupOBU(const OBUPosition& pos) : BaseOBU(pos) {}

  json to_json() const override;
  std::string type_name() const override { return "LEADING_TILE_GROUP"; }

  const TileGroupHeader& tile_group_header() const { return tile_group_header_; }
  const FrameHeaderInfo& frame_header() const { return tile_group_header_.frame_header; }

protected:
  bool parse_payload(std::ifstream& ifs) override;

private:
  TileGroupHeader tile_group_header_;
};

}  // namespace av2_obu
