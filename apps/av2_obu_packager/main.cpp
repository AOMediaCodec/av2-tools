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

#include "packaging_strategy.h"
#include <av2_obu/av2_obu.h>

// libisomedia headers
#include <fstream>
#include <iostream>

#include <ISOMovies.h>
#include <MP4Movies.h>

using namespace av2_obu;

void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  av2_obu::set_log_level(verbose ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char** argv) {
  CLI::App app{"AV2 OBU Packager - Package AV2 bitstreams into MP4 containers"};

  // Global options
  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  // Input/output
  std::string input;
  std::string output;

  app.add_option("input", input, "Input AV2 bitstream (.bin, .obu, .av2)")->required();
  app.add_option("-o,--output", output, "Output MP4 file")->required();

  // Packaging options
  double frame_rate = 30.0;
  bool drop_tds = true;
  bool force_td_mode = false;
  bool force_frame_hack = false;

  app.add_option("--fps", frame_rate, "Frame rate (default: 30.0)");
  app.add_flag("--keep-td,!--drop-td", drop_tds,
               "Keep temporal delimiters in samples (default: drop)");
  app.add_flag("--force-td-mode", force_td_mode, "Force TD-based temporal unit detection");
  app.add_flag("--force-frame-hack", force_frame_hack,
               "Force frame-based HACK mode (one frame = one TU)");

  CLI11_PARSE(app, argc, argv);

  setup_logging(verbose);

  spdlog::info("=== AV2 OBU Packager ===");
  spdlog::info("Input: {}", input);
  spdlog::info("Output: {}", output);
  spdlog::info("");

  // ========================================================================
  // PHASE 1: Parse bitstream and build temporal units
  // ========================================================================
  spdlog::info("Phase 1: Parsing bitstream...");

  OBUParser parser;

  OBUParser::TemporalUnitOptions tu_opts;
  if (force_td_mode) {
    tu_opts.mode = OBUParser::TemporalUnitOptions::Mode::kTemporalDelimiter;
  } else if (force_frame_hack) {
    tu_opts.mode = OBUParser::TemporalUnitOptions::Mode::kFrameHeuristic;
  } else {
    tu_opts.mode = OBUParser::TemporalUnitOptions::Mode::kAuto;
  }
  tu_opts.include_temporal_delimiters = !drop_tds;

  parser.set_temporal_unit_options(tu_opts);

  if (!parser.parse_file(input)) {
    spdlog::error("Failed to parse bitstream");
    return 1;
  }

  spdlog::info("  Parsed {} OBUs", parser.obu_count());
  spdlog::info("  Built {} temporal units", parser.temporal_unit_count());
  spdlog::info("");

  // ========================================================================
  // PHASE 2: Analyze bitstream characteristics
  // ========================================================================
  spdlog::info("Phase 2: Analyzing bitstream...");
  auto stats = parser.get_statistics();

  spdlog::info("  Total OBUs: {}", stats.total_obus);
  spdlog::info("  Total bytes: {}", stats.total_bytes);
  spdlog::info("  Sequence headers: {}{}", stats.sequence_headers.count,
               stats.sequence_headers.has_changes ? " (with changes)" : "(identical)");
  spdlog::info("  Temporal delimiters: {}", stats.temporal.has_temporal_delimiters ? "yes" : "no");
  spdlog::info("  Frames: {} ({} keyframes)", stats.frames.total_frames,
               stats.frames.keyframe_count);
  spdlog::info("  Single layer: {}", stats.layers.is_single_layer ? "yes" : "no");
  spdlog::info("");

  // ========================================================================
  // PHASE 3: Access temporal units from parser
  // ========================================================================
  spdlog::info("Phase 3: Processing temporal units...");

  const auto& temporal_units = parser.temporal_units();

  if (temporal_units.empty()) {
    spdlog::error("No temporal units created!");
    return 1;
  }

  size_t keyframe_count = 0;
  for (const auto& tu : temporal_units) {
    if (tu.is_keyframe()) {
      keyframe_count++;
    }
  }

  spdlog::info("  {} temporal units (MP4 samples)", temporal_units.size());
  spdlog::info("  {} keyframe samples", keyframe_count);
  spdlog::info("");

  // ========================================================================
  // PHASE 4: Write MP4 (TODO: Implement fully)
  // ========================================================================
  spdlog::info("Phase 4: Writing MP4...");
  spdlog::warn("MP4 writing not yet fully implemented!");
  spdlog::warn("TODO:");
  spdlog::warn("  - Create sample entry with av2C box");
  spdlog::warn("  - Write temporal units as samples");
  spdlog::warn("  - Handle sync samples (keyframes)");
  spdlog::warn("  - Set correct timing");

  // For now, just create an empty MP4 to test libisomedia
  MP4Movie moov;
  MP4Err err = MP4NewMovie(&moov, 1, 0xff, 0xff, 0xff, 0xff, 0xff);
  if (err != MP4NoErr) {
    spdlog::error("Failed to create MP4 movie, error code: {}", err);
    return 1;
  }

  // Set brands
  ISOSetMovieBrand(moov, ISOISOBrand, 1);
  ISOSetMovieCompatibleBrand(moov, ISOISOBrand);
  ISOSetMovieCompatibleBrand(moov, MP4_FOUR_CHAR_CODE('a', 'v', '0', '2'));

  spdlog::info("Writing placeholder MP4 to: {}", output);
  err = MP4WriteMovieToFile(moov, output.c_str());
  if (err != MP4NoErr) {
    spdlog::error("Failed to write MP4 file, error code: {}", err);
    MP4DisposeMovie(moov);
    return 1;
  }

  MP4DisposeMovie(moov);

  spdlog::info("");
  spdlog::info("Success! (Note: MP4 writing is incomplete - placeholder file created)");
  spdlog::info("Next steps: Implement full MP4 track and sample writing");

  return 0;
}
