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
#include <string>
#include <vector>

#include "mp4_writer.h"
#include "packaging_strategy.h"

namespace av2_obu {

class OBUParser;
class TemporalUnit;
class SequenceHeaderOBU;
class BaseOBU;

// Av2Packager turns a parsed AV2 elementary bitstream into an ISOBMFF .mp4
// using the bound PackagingStrategy. Owns the input ifstream and the Mp4Writer.
class Av2Packager {
public:
  Av2Packager(const std::string& input_path, const PackagingStrategy& strategy);

  // Drive the whole flow: install sample entry from the first SH, write each
  // TU as a sample, finalize.
  bool package(const OBUParser& parser, const std::string& output_path);

private:
  bool setup_video_track(const OBUParser& parser);
  bool write_tu(const TemporalUnit& tu);
  bool assemble_sample_bytes(const TemporalUnit& tu, std::vector<uint8_t>& out);

  static const SequenceHeaderOBU* find_first_sequence_header(const OBUParser& parser);

  std::string input_path_;
  PackagingStrategy strategy_;
  std::ifstream input_ifs_;
  Mp4Writer writer_;
};

// Verbose-only log of stream characteristics. Does not influence packaging.
void log_stream_summary(const class OBUParser& parser);

}  // namespace av2_obu
