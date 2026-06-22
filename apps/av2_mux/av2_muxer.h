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

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "colr_info.h"
#include "display_order_lifter.h"
#include "mp4_writer.h"
#include "mux_strategy.h"

namespace av2_obu {

class OBUParser;
class TemporalUnit;
class SequenceHeaderOBU;
class BaseOBU;

// Drives mux of one elementary AV2 bitstream into one ISOBMFF .mp4.
class Av2Muxer {
public:
  Av2Muxer(const std::string& input_path, const MuxStrategy& strategy);

  bool package(const OBUParser& parser, const std::string& output_path);

private:
  bool setup_video_track(const OBUParser& parser);
  bool check_doh_lifter_supported(const OBUParser& parser);
  bool write_tu(const TemporalUnit& tu, int32_t composition_offset);
  bool assemble_sample_bytes(const TemporalUnit& tu, std::vector<uint8_t>& out);
  std::vector<int32_t> compute_composition_offsets(const std::vector<TemporalUnit>& tus,
                                                   uint32_t start, uint32_t end);

  static const SequenceHeaderOBU* find_first_sequence_header(const OBUParser& parser);

  std::string input_path_;
  MuxStrategy strategy_;
  std::ifstream input_ifs_;
  Mp4Writer writer_;
  std::unique_ptr<DisplayOrderLifter> doh_lifter_;
  std::optional<ColrInfo> last_colr_;
};

void log_stream_summary(const class OBUParser& parser);

}  // namespace av2_obu
