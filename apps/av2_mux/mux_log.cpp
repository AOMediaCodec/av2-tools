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

#include "mux_log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace av2_obu {

spdlog::logger& mux_log() {
  static auto logger = []() {
    if (auto existing = spdlog::get("av2_mux")) return existing;
    return spdlog::stdout_color_mt("av2_mux");
  }();
  return *logger;
}

}  // namespace av2_obu
