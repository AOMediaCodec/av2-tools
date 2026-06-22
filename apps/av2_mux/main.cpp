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

#include "av2_muxer.h"
#include "mux_log.h"
#include "mux_strategy.h"
#include <av2_obu/av2_obu.h>
#include <av2_obu/version.h>

using namespace av2_obu;

// -v flips only the named "av2_mux" logger to debug; the default logger
// (used by libav2_obu's per-bit-field tracing) stays at info to keep the packaging-step trace readable.
static void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  av2_obu::set_log_level(spdlog::level::info);

  mux_log().set_level(verbose ? spdlog::level::debug : spdlog::level::info);

  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  mux_log().set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

int main(int argc, char** argv) {
  CLI::App app{"av2_mux — mux AV2 elementary bitstreams into MP4/ISOBMFF containers"};
  app.set_version_flag("--version", av2_obu::build_version());

  std::string input;
  std::string output;
  UserOptions opts;
  bool verbose = false;

  app.add_option("input", input, "Input AV2 bitstream (.bin, .obu, .av2)")->required();
  app.add_option("-o,--output", output, "Output MP4 file")->required();
  CLI::Option* fps_opt =
    app.add_option("--fps", opts.frame_rate,
                   "Frame rate override (precedence: --fps > CI timing_info > default 30)");
  app.add_option("--samples-per-chunk", opts.samples_per_chunk,
                 "Samples per chunk in stsc (default: 30)");
  app.add_option("--start-tu", opts.start_tu,
                 "Start packaging at this TU index (0-based; should be a sync sample)");
  app.add_option("--num-samples", opts.num_samples,
                 "Maximum samples to write (0 = all from start)");
  bool keep_td = false;
  app.add_flag(
    "--keep-td", keep_td,
    "Keep Temporal Delimiter OBUs inside samples. NOT RECOMMENDED — av2-isobmff says TDs "
    "SHOULD NOT appear in samples; the sample boundary is the TU boundary.");
  const std::string colr_help =
    "Force CICP color metadata (per ITU-T H.273 / ISO/IEC 23091-2) into the colr/nclx box. "
    "Accepts either a profile name [" + supported_colr_profile_names() + "] or a raw "
    "'cp:tc:mc:fr' tuple where cp=ColourPrimaries, tc=TransferCharacteristics, "
    "mc=MatrixCoefficients, fr=VideoFullRangeFlag (0=video range, 1=full range).";
  app.add_option("--colr-override", opts.colr_override, colr_help);
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  CLI11_PARSE(app, argc, argv);
  setup_logging(verbose);
  opts.frame_rate_explicit = (fps_opt->count() > 0);
  opts.drop_temporal_delimiters = !keep_td;
  if (keep_td) {
    MUX_WARN(
      "--keep-td set: each sample will start with a Temporal Delimiter OBU. The output is still "
      "decodable, but av2-isobmff recommends against this — the TU boundary is meant to be the "
      "sample boundary.");
  }
  if (!opts.colr_override.empty() && !parse_colr_override(opts.colr_override)) {
    return 2;  // parse_colr_override already logged the diagnostic
  }

  MUX_DEBUG("Muxing {} -> {}", input, output);
  OBUParser parser;
  parser.set_include_temporal_delimiters(!opts.drop_temporal_delimiters);
  if (!parser.parse_file(input)) {
    MUX_ERROR("Failed to parse bitstream: {}", input);
    return 1;
  }

  const MuxStrategy strategy = determine_strategy(parser.get_statistics(), opts);
  log_stream_summary(parser);

  if (parser.temporal_units().empty()) {
    MUX_ERROR("No temporal units; nothing to package");
    return 1;
  }

  Av2Muxer muxer(input, strategy);
  if (!muxer.package(parser, output)) {
    MUX_ERROR("Muxing failed");
    return 1;
  }
  return 0;
}
