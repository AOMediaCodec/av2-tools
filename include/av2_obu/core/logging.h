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

// Library-private logger. Routes all libav2_obu output through a dedicated named spdlog logger "av2_obu"
// Consumer applications keep full control of the spdlog default logger and their own log streams.
// Consumers can:
//   * silence library output:   av2_obu::set_log_level(spdlog::level::off)
//   * route to their own sink:  spdlog::logger& l = av2_obu::log();
//                               l.sinks() = { my_sink };
//   * leave consumer logging untouched while doing either of the above.
spdlog::logger& log();

// Convenience wrappers around the named logger's level. set_log_level() is
// kept here so callers don't have to pull the larger types header just to silence the library.
void set_log_level(spdlog::level::level_enum level);
spdlog::level::level_enum get_log_level();

}  // namespace av2_obu

// Macros that route through the library logger. Library code must use these
// instead of spdlog::xxx(...) — they expand to no-ops at compile time when
// SPDLOG_ACTIVE_LEVEL excludes the corresponding level.
#define LIB_TRACE(...) SPDLOG_LOGGER_TRACE(&av2_obu::log(), __VA_ARGS__)
#define LIB_DEBUG(...) SPDLOG_LOGGER_DEBUG(&av2_obu::log(), __VA_ARGS__)
#define LIB_INFO(...) SPDLOG_LOGGER_INFO(&av2_obu::log(), __VA_ARGS__)
#define LIB_WARN(...) SPDLOG_LOGGER_WARN(&av2_obu::log(), __VA_ARGS__)
#define LIB_ERROR(...) SPDLOG_LOGGER_ERROR(&av2_obu::log(), __VA_ARGS__)
