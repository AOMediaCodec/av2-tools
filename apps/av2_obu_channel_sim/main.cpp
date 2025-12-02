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

#include "bit_corruptor.h"
#include "packet_loss_simulator.h"

using namespace av2_obu;

void setup_logging(bool verbose) {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  av2_obu::set_log_level(verbose ? spdlog::level::debug : spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

void print_report_header() {
  spdlog::info("========================================");
  spdlog::info("  Channel Simulation Report");
  spdlog::info("========================================");
}

int main(int argc, char** argv) {
  CLI::App app{"AV2 OBU Channel Simulator - Simulate bit errors and packet loss"};
  app.require_subcommand(1);  // Require exactly one subcommand

  // Global options
  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");

  // ========== CORRUPT SUBCOMMAND ==========
  auto* corrupt_cmd = app.add_subcommand("corrupt", "Inject random bit errors (bit flips)");

  std::string corrupt_input;
  std::string corrupt_output;
  double ber = 0.001;
  bool no_protect = false;
  uint32_t corrupt_seed = 0;

  corrupt_cmd->add_option("input", corrupt_input, "Input AV2 bitstream")->required();
  corrupt_cmd->add_option("-o,--output", corrupt_output, "Output corrupted bitstream")->required();
  corrupt_cmd->add_option("--ber", ber, "Bit error rate (0.0-1.0, e.g., 0.001 = 0.1%)")
      ->check(CLI::Range(0.0, 1.0));
  corrupt_cmd->add_flag("--no-protect-config", no_protect,
                        "Disable protection of config OBUs (default: protected)");
  corrupt_cmd->add_option("--seed", corrupt_seed, "Random seed for reproducibility (0 = random)");

  // ========== PACKETLOSS SUBCOMMAND ==========
  auto* packetloss_cmd = app.add_subcommand("packetloss", "Simulate packet loss (drop packets)");

  std::string packet_input;
  std::string packet_output;
  size_t packet_size = 1200;
  double plr = 0.05;
  bool no_protect_packets = false;
  uint32_t packet_seed = 0;

  packetloss_cmd->add_option("input", packet_input, "Input AV2 bitstream")->required();
  packetloss_cmd->add_option("-o,--output", packet_output, "Output bitstream with packet loss")
      ->required();
  packetloss_cmd->add_option("--packet-size", packet_size, "Packet size in bytes (default: 1200)")
      ->check(CLI::Range(1, 65536));
  packetloss_cmd->add_option("--plr", plr, "Packet loss rate (0.0-1.0, e.g., 0.05 = 5%)")
      ->check(CLI::Range(0.0, 1.0));
  packetloss_cmd->add_flag("--no-protect-config", no_protect_packets,
                           "Disable protection of config OBUs (default: protected)");
  packetloss_cmd->add_option("--seed", packet_seed, "Random seed for reproducibility (0 = random)");

  CLI11_PARSE(app, argc, argv);

  setup_logging(verbose);

  // ========== EXECUTE SUBCOMMANDS ==========
  if (*corrupt_cmd) {
    print_report_header();
    spdlog::info("Mode: Bit Corruption");
    spdlog::info("Input: {}", corrupt_input);
    spdlog::info("BER: {:.6f} ({:.4f}%)", ber, ber * 100);

    ChannelSimulator::Options opts;
    opts.protect_config_obus = !no_protect;
    opts.seed = corrupt_seed;
    opts.verbose = verbose;

    BitCorruptor corruptor(ber, opts);

    if (!corruptor.corrupt(corrupt_input, corrupt_output)) {
      spdlog::error("Corruption failed");
      return 1;
    }

    // Print summary
    spdlog::info("========================================");
    spdlog::info("Summary:");
    spdlog::info("  Total bytes: {}", corruptor.total_bytes());
    spdlog::info("  Protected OBUs: {}", corruptor.protected_obu_count());
    spdlog::info("  Protected bytes: {} ({:.2f}%)", corruptor.protected_bytes(),
                 100.0 * corruptor.protected_bytes() / corruptor.total_bytes());
    spdlog::info("  Unprotected bytes: {}", corruptor.unprotected_bytes());
    spdlog::info("  Bits corrupted: {}", corruptor.bits_corrupted());
    spdlog::info("  Bytes affected: {} ({:.2f}% of unprotected)", corruptor.bytes_affected(),
                 100.0 * corruptor.bytes_affected() / corruptor.unprotected_bytes());
    spdlog::info("========================================");
    spdlog::info("Output: {}", corrupt_output);

    return 0;
  }

  if (*packetloss_cmd) {
    print_report_header();
    spdlog::info("Mode: Packet Loss");
    spdlog::info("Input: {}", packet_input);
    spdlog::info("Packet size: {} bytes", packet_size);
    spdlog::info("PLR: {:.6f} ({:.2f}%)", plr, plr * 100);

    ChannelSimulator::Options opts;
    opts.protect_config_obus = !no_protect_packets;
    opts.seed = packet_seed;
    opts.verbose = verbose;

    PacketLossSimulator simulator(packet_size, plr, opts);

    if (!simulator.simulate(packet_input, packet_output)) {
      spdlog::error("Packet loss simulation failed");
      return 1;
    }

    // Print summary
    spdlog::info("========================================");
    spdlog::info("Summary:");
    spdlog::info("  Total bytes: {}", simulator.total_bytes());
    spdlog::info("  Protected OBUs: {}", simulator.protected_obu_count());
    spdlog::info("  Protected bytes: {} ({:.2f}%)", simulator.protected_bytes(),
                 100.0 * simulator.protected_bytes() / simulator.total_bytes());
    spdlog::info("  Total packets: {}", simulator.packets_total());
    spdlog::info("  Packets dropped: {} ({:.2f}%)", simulator.packets_dropped(),
                 100.0 * simulator.packets_dropped() / simulator.packets_total());
    spdlog::info("  Packets protected: {}", simulator.packets_protected());
    spdlog::info("  Bytes lost: {} ({:.2f}%)", simulator.bytes_lost(),
                 100.0 * simulator.bytes_lost() / simulator.total_bytes());
    spdlog::info("  Surviving bytes: {}", simulator.total_bytes() - simulator.bytes_lost());
    spdlog::info("========================================");
    spdlog::info("Output: {}", packet_output);

    return 0;
  }

  spdlog::error("No subcommand selected");
  return 1;
}
