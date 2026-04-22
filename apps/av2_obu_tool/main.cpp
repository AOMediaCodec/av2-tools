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

#include <fstream>
#include <iostream>

#include <av2_obu/core/obu_parser.h>
#include <av2_obu/version.h>

using namespace av2_obu;

void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

  // Set log level for both spdlog and av2 library
  auto level = verbose ? spdlog::level::debug : spdlog::level::info;
  spdlog::set_level(level);
  av2_obu::set_log_level(level);
}

int main(int argc, char** argv) {
  CLI::App app{"AV2 OBU Tool - AV2 bitstream analyzer and manipulator"};
  app.set_version_flag("--version", av2_obu::build_version());

  // Global options
  bool verbose = false;
  bool json_output = false;

  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");
  app.add_flag("-j,--json", json_output, "Output in JSON format");

  // Dump subcommand
  auto dump_cmd = app.add_subcommand("dump", "Dump OBU information from a file");
  std::string dump_file;
  std::string dump_output_file;
  dump_cmd->add_option("file", dump_file, "Input OBU file")->required();
  dump_cmd->add_option("-o,--output", dump_output_file, "Output file for JSON (with -j flag)");

  // Stats subcommand
  auto stats_cmd = app.add_subcommand("stats", "Show statistics about OBUs in a file");
  std::string stats_file;
  std::string stats_output_file;
  stats_cmd->add_option("file", stats_file, "Input OBU file")->required();
  stats_cmd->add_option("-o,--output", stats_output_file, "Output file for JSON (with -j flag)");

  CLI11_PARSE(app, argc, argv);

  // Setup logging
  setup_logging(verbose);

  // Handle dump command
  if (dump_cmd->parsed()) {
    OBUParser parser;

    if (!parser.parse_file(dump_file)) {
      spdlog::error("Failed to parse file: {}", dump_file);
      return 1;
    }

    if (json_output) {
      if (!dump_output_file.empty()) {
        // Write JSON to file
        std::ofstream ofs(dump_output_file);
        if (!ofs) {
          spdlog::error("Failed to open output file: {}", dump_output_file);
          return 1;
        }
        parser.dump_json(ofs, 2);
        ofs.close();
        spdlog::info("JSON output written to: {}", dump_output_file);
      } else {
        // Write JSON to stdout
        parser.dump_json(std::cout, 2);
      }
    } else {
      parser.dump();
    }

    return 0;
  }

  // Handle stats command
  if (stats_cmd->parsed()) {
    OBUParser parser;

    if (!parser.parse_file(stats_file)) {
      spdlog::error("Failed to parse file: {}", stats_file);
      return 1;
    }

    auto stats = parser.get_statistics();

    if (json_output) {
      json j;
      j["file"] = stats_file;
      j["total_obus"] = stats.total_obus;
      j["total_bytes"] = stats.total_bytes;

      // OBU type counts
      json type_counts = json::object();
      for (const auto& [type, count] : stats.obu_type_counts) {
        type_counts[to_string(type)] = count;
      }
      j["obu_type_counts"] = type_counts;

      // Sequence headers
      j["sequence_headers"]["count"] = stats.sequence_headers.count;
      j["sequence_headers"]["has_changes"] = stats.sequence_headers.has_changes;
      j["sequence_headers"]["change_positions"] = stats.sequence_headers.change_positions;

      // Temporal structure
      j["temporal"]["td_count"] = stats.temporal.td_count;

      // Frames
      j["frames"]["total_frames"] = stats.frames.total_frames;
      j["frames"]["keyframe_count"] = stats.frames.keyframe_count;

      // Layers
      j["layers"]["is_single_layer"] = stats.layers.is_single_layer;
      j["layers"]["mlayer_ids"] = stats.layers.mlayer_ids;
      j["layers"]["xlayer_ids"] = stats.layers.xlayer_ids;
      j["layers"]["tlayer_ids"] = stats.layers.tlayer_ids;

      // Metadata
      j["metadata"]["has_metadata"] = stats.metadata.has_metadata;

      // Config OBUs
      j["config"]["has_layer_config"] = stats.config.has_layer_config;
      j["config"]["has_operating_point_set"] = stats.config.has_operating_point_set;

      if (!stats_output_file.empty()) {
        // Write JSON to file
        std::ofstream ofs(stats_output_file);
        if (!ofs) {
          spdlog::error("Failed to open output file: {}", stats_output_file);
          return 1;
        }
        ofs << j.dump(2) << std::endl;
        ofs.close();
        spdlog::info("JSON output written to: {}", stats_output_file);
      } else {
        // Write JSON to stdout
        std::cout << j.dump(2) << std::endl;
      }
    } else {
      // Human-readable output
      std::cout << "\n=== OBU Statistics ===" << std::endl;
      std::cout << "File: " << stats_file << std::endl;
      std::cout << "\nBasic Statistics:" << std::endl;
      std::cout << "  Total OBUs: " << stats.total_obus << std::endl;
      std::cout << "  Total bytes: " << stats.total_bytes << std::endl;

      std::cout << "\nOBU Type Distribution:" << std::endl;
      for (const auto& [type, count] : stats.obu_type_counts) {
        std::cout << "  " << to_string(type) << ": " << count << std::endl;
      }

      std::cout << "\nSequence Headers:" << std::endl;
      std::cout << "  Count: " << stats.sequence_headers.count << std::endl;
      std::cout << "  Has changes: " << (stats.sequence_headers.has_changes ? "yes" : "no")
                << std::endl;
      if (stats.sequence_headers.has_changes) {
        std::cout << "  Change positions (OBU indices): ";
        for (size_t pos : stats.sequence_headers.change_positions) {
          std::cout << pos << " ";
        }
        std::cout << std::endl;
      }

      std::cout << "\nTemporal Structure:" << std::endl;
      std::cout << "  TD count: " << stats.temporal.td_count << std::endl;

      std::cout << "\nFrames:" << std::endl;
      std::cout << "  Total frames: " << stats.frames.total_frames << std::endl;
      std::cout << "  Keyframes: " << stats.frames.keyframe_count << std::endl;

      std::cout << "\nLayers:" << std::endl;
      std::cout << "  Single layer: " << (stats.layers.is_single_layer ? "yes" : "no") << std::endl;
      std::cout << "  obu_mlayer_id: ";
      for (auto id : stats.layers.mlayer_ids) {
        std::cout << (int)id << " ";
      }
      std::cout << std::endl;
      std::cout << "  obu_xlayer_id: ";
      for (auto id : stats.layers.xlayer_ids) {
        std::cout << (int)id << " ";
      }
      std::cout << std::endl;
      std::cout << "  obu_tlayer_id: ";
      for (auto id : stats.layers.tlayer_ids) {
        std::cout << (int)id << " ";
      }
      std::cout << std::endl;

      std::cout << "\nMetadata:" << std::endl;
      std::cout << "  Has metadata: " << (stats.metadata.has_metadata ? "yes" : "no") << std::endl;

      std::cout << "\nConfig OBUs:" << std::endl;
      std::cout << "  Has layer config: " << (stats.config.has_layer_config ? "yes" : "no")
                << std::endl;
      std::cout << "  Has operating point set: "
                << (stats.config.has_operating_point_set ? "yes" : "no") << std::endl;
      std::cout << std::endl;
    }

    return 0;
  }

  // No subcommand was parsed
  std::cout << app.help() << std::endl;
  return 0;
}
