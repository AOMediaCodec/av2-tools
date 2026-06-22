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

#include "frame_header_helpers.h"

#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/obus/bridge_frame_obu.h>
#include <av2_obu/obus/clk_obu.h>
#include <av2_obu/obus/leading_sef_obu.h>
#include <av2_obu/obus/leading_tile_group_obu.h>
#include <av2_obu/obus/leading_tip_obu.h>
#include <av2_obu/obus/olk_obu.h>
#include <av2_obu/obus/ras_frame_obu.h>
#include <av2_obu/obus/regular_sef_obu.h>
#include <av2_obu/obus/regular_tile_group_obu.h>
#include <av2_obu/obus/regular_tip_obu.h>
#include <av2_obu/obus/switch_obu.h>

namespace av2_obu {

const FrameHeaderInfo* frame_header_of(const BaseOBU* obu) {
  if (!obu) return nullptr;
  switch (obu->type()) {
    case OBUType::CLK:
      if (auto* p = dynamic_cast<const CLKOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::OLK:
      if (auto* p = dynamic_cast<const OLKOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::RAS_FRAME:
      if (auto* p = dynamic_cast<const RASFrameOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::SWITCH:
      if (auto* p = dynamic_cast<const SwitchOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::BRIDGE_FRAME:
      if (auto* p = dynamic_cast<const BridgeFrameOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::REGULAR_TILE_GROUP:
      if (auto* p = dynamic_cast<const RegularTileGroupOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::LEADING_TILE_GROUP:
      if (auto* p = dynamic_cast<const LeadingTileGroupOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::REGULAR_TIP:
      if (auto* p = dynamic_cast<const RegularTIPOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::LEADING_TIP:
      if (auto* p = dynamic_cast<const LeadingTIPOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::REGULAR_SEF:
      if (auto* p = dynamic_cast<const RegularSEFOBU*>(obu)) return &p->frame_header();
      return nullptr;
    case OBUType::LEADING_SEF:
      if (auto* p = dynamic_cast<const LeadingSEFOBU*>(obu)) return &p->frame_header();
      return nullptr;
    default:
      return nullptr;
  }
}

const BaseOBU* find_output_frame(const TemporalUnit& tu) {
  for (const auto* obu : tu.obus()) {
    const FrameHeaderInfo* fh = frame_header_of(obu);
    if (fh && fh->is_output_frame) return obu;
  }
  return nullptr;
}

}  // namespace av2_obu
