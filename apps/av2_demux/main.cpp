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

#include <CLI/CLI.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "av2_demuxer.h"
#include <av2_obu/version.h>

int main(int argc, char** argv) {
  CLI::App app{"av2_demux — demux MP4/ISOBMFF back into an AV2 elementary bitstream"};
  app.set_version_flag("--version", av2_obu::build_version());

  std::string input;
  std::string output;
  bool verbose = false;

  app.add_option("input", input, "Input MP4 file (must contain an 'av02' video track)")->required();
  app.add_option("-o,--output", output, "Output AV2 elementary bitstream (.obu)")->required();
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  CLI11_PARSE(app, argc, argv);

  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  av2_obu::Av2Demuxer demuxer;
  if (!demuxer.demux(input, output)) {
    spdlog::error("Demuxing failed");
    return 1;
  }
  return 0;
}
