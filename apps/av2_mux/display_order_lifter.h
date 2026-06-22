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

#include <cstdint>

namespace av2_obu {

// Modular-unwrap lifter for OrderHintLsbs → relative DispOrderHint, used to
// compute per-sample ctts offsets.
//
// This is NOT a full implementation of get_disp_order_hint()
// (05_Syntax_structures.bs:2649). It tracks no reference-frame state, ignores
// dependency maps, does not reset at CLK boundaries, and reads only the
// per-frame OrderHintLsbs. It produces correct output exactly when the input
// stream meets all of the following:
//   * Single-layer (one xlayer, one mlayer).
//   * Single CVS (one CLK at the start, no further CLKs that reset DOH).
//   * No derived-DOH SEFs (derive_sef_order_hint must be 0 if any SEFs exist).
//   * No Bridge frames (their DOH is RefOrderHint[bridge_frame_ref_idx],
//     which we cannot compute without ref-state tracking).
//   * Reorder distance between consecutive output frames < (1 << OrderHintBits) / 2.
//
// Av2Muxer enforces the first four with hard errors. The reorder-distance
// bound is the only soft assumption; it holds for hierarchical-B and
// low-delay GoPs at typical OrderHintBits ≥ 4.
//
// A spec-faithful replacement (with reference state and dep maps) is planned
// once Phase 2 (parser cross-check vs avmdec) validates the per-frame fields
// the full algorithm depends on.
class DisplayOrderLifter {
 public:
  explicit DisplayOrderLifter(uint32_t order_hint_bits);

  // Clears wrap state at a CVS boundary; does not re-anchor.
  void reset_cvs();

  // Returns the lifted DOH relative to the first lifted value of the stream.
  int64_t lift(uint32_t order_hint_lsbs);

 private:
  int64_t mod_;
  int64_t half_;
  int64_t wrap_ = 0;
  int64_t max_seen_ = 0;
  bool any_ = false;
  int64_t anchor_ = 0;
  bool anchored_ = false;
};

}  // namespace av2_obu
