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

namespace av2_obu {

class BaseOBU;
class TemporalUnit;
struct FrameHeaderInfo;

// Returns the FrameHeaderInfo of any coded-frame OBU, or nullptr.
const FrameHeaderInfo* frame_header_of(const BaseOBU* obu);

// First output-frame OBU in tu, or nullptr.
const BaseOBU* find_output_frame(const TemporalUnit& tu);

}  // namespace av2_obu
