#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace RomFileFilter {

inline bool equalIgnoreCase(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return *a == '\0' && *b == '\0';
}

inline const char* extensionOf(const char* name) {
  if (!name) return nullptr;
  const char* slash = strrchr(name, '/');
  const char* base = slash ? slash + 1 : name;
  return strrchr(base, '.');
}

inline bool isIgnoredName(const char* name) {
  if (!name || name[0] == '\0' || name[0] == '.') return true;
  return equalIgnoreCase(name, "$RECYCLE.BIN") || equalIgnoreCase(name, "SYSTEM VOLUME INFORMATION");
}

inline bool needsSample(const char* name) {
  const char* ext = extensionOf(name);
  return equalIgnoreCase(ext, ".md") || equalIgnoreCase(ext, ".sg") || equalIgnoreCase(ext, ".col");
}

inline bool looksLikeBinary(const uint8_t* bytes, size_t length) {
  if (!bytes || length == 0) return false;
  size_t textBytes = 0;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t value = bytes[i];
    if ((value >= 0x20 && value <= 0x7E) || value == '\n' || value == '\r' || value == '\t') {
      ++textBytes;
    }
  }
  return textBytes * 4 < length * 3;
}

inline bool isPlausibleFile(const char* name, size_t fileSize, const uint8_t* prefix, size_t prefixSize,
                            const uint8_t* genesisHeader, size_t genesisHeaderSize) {
  if (fileSize < 64) return false;
  const char* ext = extensionOf(name);
  if (equalIgnoreCase(ext, ".md")) {
    return fileSize >= 0x104 && genesisHeader && genesisHeaderSize == 4 && memcmp(genesisHeader, "SEGA", 4) == 0;
  }
  if (equalIgnoreCase(ext, ".sg") || equalIgnoreCase(ext, ".col")) {
    if (fileSize > 4U * 1024U * 1024U || !prefix || prefixSize < 16) return false;
    if (equalIgnoreCase(ext, ".col") && prefixSize >= 2 &&
        ((prefix[0] == 0xAA && prefix[1] == 0x55) || (prefix[0] == 0x55 && prefix[1] == 0xAA)))
      return true;
    return looksLikeBinary(prefix, prefixSize);
  }
  return true;
}

} // namespace RomFileFilter
