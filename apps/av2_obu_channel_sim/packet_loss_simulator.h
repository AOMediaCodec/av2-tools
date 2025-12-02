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

#include "channel_simulator.h"

namespace av2_obu {

// Packet loss simulation (drops fixed-size packets)
class PacketLossSimulator : public ChannelSimulator {
public:
  PacketLossSimulator(size_t packet_size, double plr, const Options& opts = {});

  // Simulate packet loss and write surviving data to output
  bool simulate(const std::string& input, const std::string& output);

  // Statistics
  size_t packets_total() const { return packets_total_; }
  size_t packets_dropped() const { return packets_dropped_; }
  size_t packets_protected() const { return packets_protected_; }
  size_t bytes_lost() const { return bytes_lost_; }
  size_t total_bytes() const { return total_bytes_; }

private:
  size_t packet_size_;  // Packet size in bytes
  double plr_;          // Packet loss rate (0.0 to 1.0)
  std::mt19937 rng_;
  std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};

  size_t packets_total_ = 0;
  size_t packets_dropped_ = 0;
  size_t packets_protected_ = 0;
  size_t bytes_lost_ = 0;
  size_t total_bytes_ = 0;

  // Check if packet should be dropped (considering protection)
  bool should_drop_packet(size_t packet_start, size_t packet_end);
};

}  // namespace av2_obu
