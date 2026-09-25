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
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "av2_codec_config.h"
#include "colr_info.h"
#include "display_order_lifter.h"
#include "mp4_writer.h"
#include "mux_strategy.h"
#include "ref_frame_buffer.h"

namespace av2_obu {

class OBUParser;
class TemporalUnit;
class SequenceHeaderOBU;
class BaseOBU;

// One entry per distinct av2C configuration (CVS) observed in the stream.
struct SampleEntryRecord {
  AV2CodecConfigurationBox av2c;
  std::optional<ColrInfo> colr;
  uint32_t desc_idx = 0;  // 1-based index assigned by Mp4Writer
  std::vector<uint8_t> config_obu_bytes;  // serialized, for equality checks
};

// Drives mux of one elementary AV2 bitstream into one ISOBMFF .mp4.
class Av2Muxer {
public:
  Av2Muxer(const std::string& input_path, const MuxStrategy& strategy);

  bool package(const OBUParser& parser, const std::string& output_path);

private:
  bool setup_video_track(const OBUParser& parser);
  bool check_doh_lifter_supported(const OBUParser& parser);
  bool write_tu(const TemporalUnit& tu, uint32_t tu_index, int32_t composition_offset);
  bool assemble_sample_bytes(const TemporalUnit& tu, std::vector<uint8_t>& out);
  std::vector<int32_t> compute_composition_offsets(const OBUParser& parser,
                                                   const std::vector<TemporalUnit>& tus,
                                                   uint32_t start, uint32_t end);

  static const SequenceHeaderOBU* find_first_sequence_header(const OBUParser& parser);
  static const SequenceHeaderOBU* find_sh_in_tu(const TemporalUnit& tu, uint32_t xlayer_id);
  static void check_cmaf_invariants(const std::vector<SampleEntryRecord>& entries);

  std::string input_path_;
  MuxStrategy strategy_;
  std::ifstream input_ifs_;
  Mp4Writer writer_;
  std::unique_ptr<DisplayOrderLifter> doh_lifter_;
  RefFrameBuffer ref_buffer_;
  std::optional<ColrInfo> last_colr_;

  // One entry per distinct av2C configuration (CVS) registered with the writer.
  std::vector<SampleEntryRecord> sample_entries_;

  // Maps TU index -> desc_idx, populated during setup_video_track().
  std::vector<uint32_t> tu_desc_idx_;

  // Lowest non-global obu_xlayer_id among frame OBUs (all layers share timing info)
  uint32_t base_xlayer_id_ = 0;
};

void log_stream_summary(const class OBUParser& parser);

}  // namespace av2_obu
