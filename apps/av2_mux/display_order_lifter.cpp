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

namespace av2_obu {

DisplayOrderLifter::DisplayOrderLifter(uint32_t order_hint_bits) {
  mod_ = (order_hint_bits == 0) ? 1 : (int64_t{1} << order_hint_bits);
  half_ = mod_ >> 1;
}

void DisplayOrderLifter::reset_cvs() {
  // Wrap state is implicit in max_seen_; nothing to reset across CVS joins.
}

int64_t DisplayOrderLifter::lift(uint32_t order_hint_lsbs) {
  int64_t oh = static_cast<int64_t>(order_hint_lsbs);
  int64_t candidate = oh + wrap_;
  if (any_) {
    while (candidate < max_seen_ - half_) {
      candidate += mod_;
      wrap_ += mod_;
    }
    while (candidate > max_seen_ + half_) {
      candidate -= mod_;
      wrap_ -= mod_;
    }
  }
  if (!anchored_) {
    anchor_ = candidate;
    anchored_ = true;
  }
  if (!any_ || candidate > max_seen_) max_seen_ = candidate;
  any_ = true;
  return candidate - anchor_;
}

}  // namespace av2_obu
