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

#include "ref_frame_buffer.h"

namespace av2_obu {

class BaseOBU;
struct AV2SequenceHeader;
struct FrameHeaderInfo;

// Lifts each frame's OrderHintLsbs into a relative DispOrderHint, tracking the
// reference-frame buffer (driven by refresh_frame_flags) so maxDisp stays
// correct across CVS boundaries. process() returns the lifted DOH relative to
// the first lifted output (anchor = 0) and also updates the ref buffer.
class DisplayOrderLifter {
 public:
  explicit DisplayOrderLifter(uint32_t order_hint_bits);

  // Lift this frame's OrderHintLsbs and apply its refresh_frame_flags to refs.
  int64_t process(const BaseOBU& obu, const FrameHeaderInfo& fh,
                  const AV2SequenceHeader& sh, RefFrameBuffer& refs);

 private:
  uint32_t order_hint_bits_;
  uint64_t mod_;
  bool anchored_ = false;
  uint64_t anchor_ = 0;
};

}  // namespace av2_obu
