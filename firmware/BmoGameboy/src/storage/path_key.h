#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace RomKey {
constexpr char normalizePathByte(char value) {
  if (value == '\\') return '/';
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value + ('a' - 'A'));
  return value;
}

constexpr uint32_t crc32RelativePath(const char* relativePath) {
  if (!relativePath) return 0;
  while (*relativePath == '/' || *relativePath == '\\') ++relativePath;
  uint32_t crc = 0xFFFFFFFFU;
  while (*relativePath) {
    crc ^= static_cast<uint8_t>(normalizePathByte(*relativePath++));
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

inline bool make(const char* relativePath, char* output, size_t outputSize) {
  if (!relativePath || !output || outputSize < 33) return false;
  output[0] = '\0';

  const char* pathEnd = relativePath + strlen(relativePath);
  while (pathEnd > relativePath && (pathEnd[-1] == '/' || pathEnd[-1] == '\\')) --pathEnd;
  if (pathEnd == relativePath) return false;

  const char* filename = relativePath;
  for (const char* p = relativePath; *p; ++p) {
    if (*p == '/' || *p == '\\') filename = p + 1;
  }
  if (!*filename) return false;
  const char* dot = nullptr;
  for (const char* p = filename; *p; ++p) {
    if (*p == '.') dot = p;
  }
  const size_t stemLength = dot ? static_cast<size_t>(dot - filename) : strlen(filename);

  char prefix[24];
  size_t prefixLength = 0;
  bool previousUnderscore = false;
  for (size_t i = 0; i < stemLength && prefixLength < 23; ++i) {
    char c = filename[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (allowed) {
      prefix[prefixLength++] = c;
      previousUnderscore = false;
    } else if (!previousUnderscore && prefixLength > 0) {
      prefix[prefixLength++] = '_';
      previousUnderscore = true;
    }
  }
  while (prefixLength > 0 && prefix[prefixLength - 1] == '_') --prefixLength;
  if (prefixLength == 0) {
    prefix[0] = 'r';
    prefix[1] = 'o';
    prefix[2] = 'm';
    prefixLength = 3;
  }
  prefix[prefixLength] = '\0';

  const int written = snprintf(output, outputSize, "%s_%08lx", prefix,
                               static_cast<unsigned long>(crc32RelativePath(relativePath)));
  if (written < 0 || written > 32 || static_cast<size_t>(written) >= outputSize) {
    output[0] = '\0';
    return false;
  }
  return true;
}
}
