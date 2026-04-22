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

#include <iostream>

#include "bitstream_switcher.h"
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/version.h>

using namespace av2_obu;

void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  av2_obu::set_log_level(verbose ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char** argv) {
  CLI::App app{"AV2 OBU Switcher - Bitstream switching simulation tool"};
  app.set_version_flag("--version", av2_obu::build_version());

  // Global options
  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  // Input streams
  std::vector<std::string> streams;
  std::string output;
  double fps = 30.0;
  std::vector<double> timestamps;
  int obu_type = 4;  // TILE_GROUP

  app.add_option("streams", streams, "Input bitstreams (2 or more)")->required()->expected(2, 100);
  app.add_option("-o,--output", output, "Output bitstream file")->required();
  app.add_option("-f,--fps", fps, "Frame rate (frames per second)")->default_val(30.0);
  app.add_option("-t,--timestamps", timestamps,
                 "Switch timestamps in seconds (e.g., 1.0 2.5 3.75)");
  app.add_option("--obu-type", obu_type, "OBU type to switch on (default: 4=TILE_GROUP)")
    ->default_val(4);

  CLI11_PARSE(app, argc, argv);

  setup_logging(verbose);

  // Validate inputs
  if (streams.size() < 2) {
    spdlog::error("Need at least 2 streams for switching");
    return 1;
  }

  if (timestamps.size() != streams.size() - 1) {
    spdlog::error("Number of timestamps must be exactly (streams - 1)");
    spdlog::error("Got {} streams and {} timestamps", streams.size(), timestamps.size());
    return 1;
  }

  // Create BitstreamSwitcher
  BitstreamSwitcher switcher(fps);

  // Load streams
  spdlog::info("Loading {} streams at {} fps", streams.size(), fps);
  if (!switcher.attach_streams(streams)) {
    spdlog::error("Failed to load streams");
    return 1;
  }

  // Build switch points
  std::vector<BitstreamSwitcher::SwitchPoint> switch_points;
  for (size_t i = 0; i < timestamps.size(); ++i) {
    BitstreamSwitcher::SwitchPoint sp;
    sp.timestamp_sec = timestamps[i];
    sp.on_obu_type = static_cast<OBUType>(obu_type);
    switch_points.push_back(sp);
    spdlog::info("Switch point {}: time={:.3f}s, type={}", i, sp.timestamp_sec,
                 to_string(sp.on_obu_type));
  }

  // Build stream order (0, 1, 2, ...)
  std::vector<size_t> stream_order;
  for (size_t i = 0; i < streams.size(); ++i) {
    stream_order.push_back(i);
  }

  // Perform stitching
  spdlog::info("Stitching streams to: {}", output);
  if (!switcher.stitch(stream_order, switch_points, output)) {
    spdlog::error("Failed to stitch streams");
    return 1;
  }

  spdlog::info("Successfully created switched bitstream: {}", output);
  return 0;
}
