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

#include "packet_loss_simulator.h"

#include <spdlog/spdlog.h>

#include <fstream>

namespace av2_obu {

PacketLossSimulator::PacketLossSimulator(size_t packet_size, double plr, const Options& opts)
    : ChannelSimulator(opts), packet_size_(packet_size), plr_(plr), rng_(options_.seed) {
  if (plr_ < 0.0 || plr_ > 1.0) {
    spdlog::error("PLR must be between 0.0 and 1.0");
    plr_ = 0.0;
  }
  if (packet_size_ == 0) {
    spdlog::error("Packet size must be > 0");
    packet_size_ = 1200;
  }
  spdlog::info("PacketLossSimulator initialized: packet_size={}, PLR={:.6f}, seed={}", packet_size_,
               plr_, options_.seed);
}

bool PacketLossSimulator::simulate(const std::string& input, const std::string& output) {
  // Parse bitstream to identify protected regions
  if (!analyze_bitstream(input)) {
    return false;
  }

  // Read input file
  std::ifstream ifs(input, std::ios::binary);
  if (!ifs) {
    spdlog::error("Failed to open input file: {}", input);
    return false;
  }

  std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  total_bytes_ = data.size();
  packets_total_ = (total_bytes_ + packet_size_ - 1) / packet_size_;  // Round up
  packets_dropped_ = 0;
  packets_protected_ = 0;
  bytes_lost_ = 0;

  spdlog::info("Simulating packet loss ({} bytes, {} packets of {} bytes)...", total_bytes_,
               packets_total_, packet_size_);

  std::vector<uint8_t> output_data;
  output_data.reserve(total_bytes_);

  // Process each packet
  for (size_t i = 0; i < packets_total_; ++i) {
    size_t packet_start = i * packet_size_;
    size_t packet_end = std::min(packet_start + packet_size_, total_bytes_);
    size_t packet_bytes = packet_end - packet_start;

    if (should_drop_packet(packet_start, packet_end)) {
      packets_dropped_++;
      bytes_lost_ += packet_bytes;
      if (options_.verbose) {
        spdlog::debug("  Dropped packet {} [{}, {}) - {} bytes", i, packet_start, packet_end,
                      packet_bytes);
      }
    } else {
      // Keep packet
      output_data.insert(output_data.end(), data.begin() + packet_start, data.begin() + packet_end);
    }
  }

  spdlog::info("Packet loss complete: {}/{} packets dropped ({} bytes lost), {} packets protected",
               packets_dropped_, packets_total_, bytes_lost_, packets_protected_);

  // Write output file
  std::ofstream ofs(output, std::ios::binary);
  if (!ofs) {
    spdlog::error("Failed to open output file: {}", output);
    return false;
  }

  ofs.write(reinterpret_cast<const char*>(output_data.data()),
            static_cast<std::streamsize>(output_data.size()));
  ofs.close();

  spdlog::info("Output written to: {} ({} bytes)", output, output_data.size());
  return true;
}

bool PacketLossSimulator::should_drop_packet(size_t packet_start, size_t packet_end) {
  // Don't drop if packet overlaps with any protected OBU
  if (overlaps_protected(packet_start, packet_end)) {
    packets_protected_++;
    if (options_.verbose) {
      spdlog::debug("  Protecting packet [{}, {}) - overlaps config OBU", packet_start, packet_end);
    }
    return false;
  }

  // Normal random drop decision
  return uniform_dist_(rng_) < plr_;
}

}  // namespace av2_obu
