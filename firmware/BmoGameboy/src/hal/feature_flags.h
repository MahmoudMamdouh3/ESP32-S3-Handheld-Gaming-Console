// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

// -----------------------------------------------------------------------------
// feature_flags.h - Hardware hard stops, compile profiles, and serial logging.
// -----------------------------------------------------------------------------

#ifndef FEATURE_SD_CARD
#define FEATURE_SD_CARD 1
#endif
#ifndef FEATURE_AUDIO
#define FEATURE_AUDIO 0
#endif
#ifndef FEATURE_BATTERY_MONITOR
#define FEATURE_BATTERY_MONITOR 0
#endif

#ifndef BMO_ENABLE_BAKED_ROMS
#define BMO_ENABLE_BAKED_ROMS 0
#endif

#ifndef BMO_ENABLE_DOOM
#define BMO_ENABLE_DOOM 1
#endif

#ifndef BMO_EXPERIMENTAL_CORES
#define BMO_EXPERIMENTAL_CORES 1
#endif

// -----------------------------------------------------------------------------
// LOGGING & DIAGNOSTICS
// -----------------------------------------------------------------------------
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR_STR(str) Serial.println("[ERROR] " str)
#else
#define LOG_ERROR(fmt, ...)                                                                                            \
  do {                                                                                                                 \
  } while (0)
#define LOG_ERROR_STR(str)                                                                                             \
  do {                                                                                                                 \
  } while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN_STR(str) Serial.println("[WARN] " str)
#else
#define LOG_WARN(fmt, ...)                                                                                             \
  do {                                                                                                                 \
  } while (0)
#define LOG_WARN_STR(str)                                                                                              \
  do {                                                                                                                 \
  } while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO_STR(str) Serial.println("[INFO] " str)
#else
#define LOG_INFO(fmt, ...)                                                                                             \
  do {                                                                                                                 \
  } while (0)
#define LOG_INFO_STR(str)                                                                                              \
  do {                                                                                                                 \
  } while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG_STR(str) Serial.println("[DEBUG] " str)
#else
#define LOG_DEBUG(fmt, ...)                                                                                            \
  do {                                                                                                                 \
  } while (0)
#define LOG_DEBUG_STR(str)                                                                                             \
  do {                                                                                                                 \
  } while (0)
#endif
