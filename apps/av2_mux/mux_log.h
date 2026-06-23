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

#include <memory>

#include <spdlog/spdlog.h>

namespace av2_obu {

// Named logger so the muxer's verbosity is independent of the libav2_obu
// parser logger (which emits per-bit-field debug spam at the same level).
spdlog::logger& mux_log();

}  // namespace av2_obu

#define MUX_TRACE(...) av2_obu::mux_log().trace(__VA_ARGS__)
#define MUX_DEBUG(...) av2_obu::mux_log().debug(__VA_ARGS__)
#define MUX_INFO(...) av2_obu::mux_log().info(__VA_ARGS__)
#define MUX_WARN(...) av2_obu::mux_log().warn(__VA_ARGS__)
#define MUX_ERROR(...) av2_obu::mux_log().error(__VA_ARGS__)
