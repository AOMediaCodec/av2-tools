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

#include <array>
#include <cstdint>

namespace av2_obu {

constexpr uint32_t kNumRefSlots = 16;
constexpr uint64_t kRestrictedOrderHint = 0xFFFF'FFFF'FFFF'FFFFull;

// Per-slot reference state mirroring the spec arrays RefValid, RefOrderHint,
// RefMLayerId, RefTLayerId, RefCounter. Only what the DOH lifter needs.
struct RefSlot {
  bool valid = false;
  uint64_t order_hint = 0;       // lifted DOH (kRestrictedOrderHint = invalid for ranking)
  uint8_t mlayer_id = 0;
  uint8_t tlayer_id = 0;
  uint32_t counter = 0;          // increments per refresh; powers first_slot_with_ref
};

// Stream-level reference state. Tracks slot updates driven by refresh_frame_flags.
class RefFrameBuffer {
 public:
  RefSlot& slot(size_t i) { return slots_[i]; }
  const RefSlot& slot(size_t i) const { return slots_[i]; }
  size_t size() const { return slots_.size(); }

  uint32_t allocate_counter() { return ++next_counter_; }
  void invalidate_all();

 private:
  std::array<RefSlot, kNumRefSlots> slots_{};
  uint32_t next_counter_ = 0;
};

}  // namespace av2_obu
