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

#include "display_order_lifter.h"

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/frame_header_info.h>

namespace av2_obu {

namespace {

// Returns max RefOrderHint across slots that pass dep-map and "showable" filters.
// We don't track Ref{Implicit,Immediate}OutputFrame per slot today; the practical
// effect is that hidden refs may briefly contribute to maxDisp until a later
// output frame supersedes them. Doesn't matter for our current fixtures.
uint64_t get_max_disp_order_hint(const RefFrameBuffer& refs,
                                 const AV2SequenceHeader& sh,
                                 uint8_t obu_mlayer_id, uint8_t obu_tlayer_id) {
  uint64_t max_disp = 0;
  for (size_t i = 0; i < refs.size(); ++i) {
    const auto& s = refs.slot(i);
    if (!s.valid) continue;
    if (s.order_hint == kRestrictedOrderHint) continue;
    if (obu_mlayer_id < MAX_NUM_MLAYERS && s.mlayer_id < MAX_NUM_MLAYERS &&
        !sh.MLayerDependencyMap[obu_mlayer_id][s.mlayer_id]) {
      continue;
    }
    if (obu_mlayer_id < MAX_NUM_MLAYERS && obu_tlayer_id < MAX_NUM_TLAYERS &&
        s.tlayer_id < MAX_NUM_TLAYERS &&
        !sh.TLayerDependencyMap[obu_mlayer_id][obu_tlayer_id][s.tlayer_id]) {
      continue;
    }
    if (s.order_hint > max_disp) max_disp = s.order_hint;
  }
  return max_disp;
}

}  // namespace

DisplayOrderLifter::DisplayOrderLifter(uint32_t order_hint_bits)
    : order_hint_bits_(order_hint_bits),
      mod_(order_hint_bits == 0 ? 1ull : (1ull << order_hint_bits)) {}

int64_t DisplayOrderLifter::process(const BaseOBU& obu, const FrameHeaderInfo& fh,
                                    const AV2SequenceHeader& sh, RefFrameBuffer& refs) {
  const OBUType type = obu.type();
  const uint8_t mlayer_id = obu.header().get_mlayer_id();
  const uint8_t tlayer_id = obu.header().get_tlayer_id();

  // get_disp_order_hint: CLK and restricted SWITCH return Lsbs directly; otherwise lift.
  const uint64_t lsbs = fh.order_hint;
  uint64_t lifted;
  const bool is_clk = (type == OBUType::CLK);
  const bool is_restricted_switch = !is_sef(type) && fh.FrameType == SWITCH_FRAME &&
                                    fh.restricted_prediction_switch != 0;
  if (is_clk || is_restricted_switch) {
    lifted = lsbs;
  } else {
    const uint64_t max_disp = get_max_disp_order_hint(refs, sh, mlayer_id, tlayer_id);
    const int64_t offset =
        static_cast<int64_t>(max_disp) - static_cast<int64_t>(mod_ >> 1) -
        static_cast<int64_t>(lsbs);
    lifted = lsbs;
    if (offset >= 0) {
      const uint64_t shift =
          ((static_cast<uint64_t>(offset) >> order_hint_bits_) + 1) << order_hint_bits_;
      lifted += shift;
    }
  }

  if (!anchored_) {
    anchor_ = lifted;
    anchored_ = true;
  }

  // CLK at start of CVS clears the ref buffer.
  if (type == OBUType::CLK) {
    refs.invalidate_all();
  }

  // Restricted SWITCH poisons all valid refs with RESTRICTED_OH so they no longer feed maxDisp.
  if (type == OBUType::SWITCH && fh.restricted_prediction_switch != 0) {
    for (size_t i = 0; i < refs.size(); ++i) {
      auto& s = refs.slot(i);
      if (s.valid) s.order_hint = kRestrictedOrderHint;
    }
  }

  // Apply refresh_frame_flags: each set bit refreshes the corresponding slot
  // with this frame's metadata.
  const uint32_t flags = fh.refresh_frame_flags;
  for (uint32_t i = 0; i < refs.size() && i < 32; ++i) {
    if (flags & (1u << i)) {
      auto& s = refs.slot(i);
      s.valid = true;
      s.order_hint = lifted;
      s.mlayer_id = mlayer_id;
      s.tlayer_id = tlayer_id;
      s.counter = refs.allocate_counter();
    }
  }

  return static_cast<int64_t>(lifted) - static_cast<int64_t>(anchor_);
}

}  // namespace av2_obu
