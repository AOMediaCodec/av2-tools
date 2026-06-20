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

#include "av2_packager.h"
#include "packaging_strategy.h"
#include <av2_obu/av2_obu.h>
#include <av2_obu/version.h>

using namespace av2_obu;

static void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  av2_obu::set_log_level(verbose ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char** argv) {
  CLI::App app{"AV2 OBU Packager — Package AV2 bitstreams into MP4 containers"};
  app.set_version_flag("--version", av2_obu::build_version());

  std::string input;
  std::string output;
  UserOptions opts;
  bool verbose = false;

  app.add_option("input", input, "Input AV2 bitstream (.bin, .obu, .av2)")->required();
  app.add_option("-o,--output", output, "Output MP4 file")->required();
  app.add_option("--fps", opts.frame_rate, "Frame rate (default: 30.0)");
  app.add_option("--samples-per-chunk", opts.samples_per_chunk,
                 "Samples per chunk in stsc (default: 30)");
  app.add_option("--start-tu", opts.start_tu,
                 "Start packaging at this TU index (0-based; should be a sync sample)");
  app.add_option("--num-samples", opts.num_samples,
                 "Maximum samples to write (0 = all from start)");
  app.add_flag("--keep-td,!--drop-td", opts.drop_temporal_delimiters,
               "Keep temporal delimiters in samples (default: drop)");
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  CLI11_PARSE(app, argc, argv);
  setup_logging(verbose);

  spdlog::info("=== AV2 OBU Packager ===");
  spdlog::info("Input:  {}", input);
  spdlog::info("Output: {}", output);

  // --- Parse the bitstream ---
  OBUParser parser;
  parser.set_include_temporal_delimiters(!opts.drop_temporal_delimiters);
  if (!parser.parse_file(input)) {
    spdlog::error("Failed to parse bitstream");
    return 1;
  }

  // --- Derive packaging strategy from stats + user options ---
  const PackagingStrategy strategy = determine_strategy(parser.get_statistics(), opts);
  log_stream_summary(parser);  // verbose-only

  // --- Pre-flight check ---
  if (parser.temporal_units().empty()) {
    spdlog::error("No temporal units; nothing to package");
    return 1;
  }

  // --- Package ---
  Av2Packager packager(input, strategy);
  if (!packager.package(parser, output)) {
    spdlog::error("Packaging failed");
    return 1;
  }

  spdlog::info("Done: {}", output);
  return 0;
}
