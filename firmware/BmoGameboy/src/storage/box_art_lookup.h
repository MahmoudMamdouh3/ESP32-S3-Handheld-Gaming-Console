#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "path_key.h"
#include "sd_card.h"

namespace BoxArtLookup {

inline char asciiLower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c + ('a' - 'A'));
  }
  return c;
}

inline void getBaseName(const char* romFilename, char* outBaseName, size_t maxLen) {
  if (!romFilename || !outBaseName || maxLen == 0) return;
  outBaseName[0] = '\0';

  const char* base = strrchr(romFilename, '/');
  const char* backslash = strrchr(romFilename, '\\');
  if (backslash && (!base || backslash > base)) {
    base = backslash;
  }
  base = base ? (base + 1) : romFilename;

  strncpy(outBaseName, base, maxLen - 1);
  outBaseName[maxLen - 1] = '\0';

  char* dot = strrchr(outBaseName, '.');
  if (dot) {
    *dot = '\0';
  }
}

inline void baseNameSpan(const char* romFilename, const char** outStart, size_t* outLen) {
  if (!outStart || !outLen) return;
  *outStart = "";
  *outLen = 0;
  if (!romFilename) return;

  const char* base = strrchr(romFilename, '/');
  const char* backslash = strrchr(romFilename, '\\');
  if (backslash && (!base || backslash > base)) {
    base = backslash;
  }
  base = base ? (base + 1) : romFilename;

  size_t baseLen = strnlen(base, 63);
  const char* lastDot = nullptr;
  for (size_t i = 0; i < baseLen; ++i) {
    if (base[i] == '.') {
      lastDot = base + i;
    }
  }
  *outStart = base;
  *outLen = lastDot ? static_cast<size_t>(lastDot - base) : baseLen;
}

inline int compareSpanIgnoreAsciiCase(const char* a, size_t aLen, const char* b, size_t bLen) {
  const size_t minLen = (aLen < bLen) ? aLen : bLen;
  for (size_t i = 0; i < minLen; ++i) {
    const unsigned char ca = static_cast<unsigned char>(asciiLower(a[i]));
    const unsigned char cb = static_cast<unsigned char>(asciiLower(b[i]));
    if (ca != cb) {
      return (ca < cb) ? -1 : 1;
    }
  }
  if (aLen == bLen) return 0;
  return (aLen < bLen) ? -1 : 1;
}

inline bool sameNameIgnoreAsciiCase(const char* left, const char* right) {
  if (!left || !right) return false;
  return compareSpanIgnoreAsciiCase(left, strlen(left), right, strlen(right)) == 0;
}

inline bool parseHex8(const char* text, uint32_t* outValue) {
  if (!text || !outValue) return false;
  uint32_t value = 0;
  for (int i = 0; i < 8; ++i) {
    const char c = text[i];
    uint32_t nibble = 0;
    if (c >= '0' && c <= '9') {
      nibble = static_cast<uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      nibble = static_cast<uint32_t>(10 + (c - 'a'));
    } else if (c >= 'A' && c <= 'F') {
      nibble = static_cast<uint32_t>(10 + (c - 'A'));
    } else {
      return false;
    }
    value = (value << 4) | nibble;
  }
  *outValue = value;
  return true;
}

inline bool parseArtFilename(const char* entryName, char* outStem, size_t outStemSize, bool* outHasKeyCrc,
                             uint32_t* outKeyCrc) {
  if (!entryName || !outStem || outStemSize == 0 || !outHasKeyCrc || !outKeyCrc) {
    return false;
  }
  outStem[0] = '\0';
  *outHasKeyCrc = false;
  *outKeyCrc = 0;

  const char* base = strrchr(entryName, '/');
  const char* backslash = strrchr(entryName, '\\');
  if (backslash && (!base || backslash > base)) {
    base = backslash;
  }
  base = base ? (base + 1) : entryName;
  if (base[0] == '\0' || base[0] == '.') {
    return false;
  }

  const char* dot = strrchr(base, '.');
  if (!dot || dot == base) {
    return false;
  }
  const char* ext = dot + 1;
  if (compareSpanIgnoreAsciiCase(ext, strlen(ext), "raw", 3) != 0 &&
      compareSpanIgnoreAsciiCase(ext, strlen(ext), "bmp", 3) != 0) {
    return false;
  }

  const size_t stemLen = static_cast<size_t>(dot - base);
  if (stemLen == 0 || stemLen >= outStemSize) {
    return false;
  }
  memcpy(outStem, base, stemLen);
  outStem[stemLen] = '\0';

  if (stemLen >= 10 && outStem[stemLen - 9] == '_') {
    uint32_t parsedCrc = 0;
    if (parseHex8(outStem + stemLen - 8, &parsedCrc)) {
      *outHasKeyCrc = true;
      *outKeyCrc = parsedCrc;
    }
  }
  return true;
}

struct KeyEntry {
  uint32_t crc;
  int32_t romIdx;
};

namespace detail {
inline int compareKeyEntries(const KeyEntry& a, const KeyEntry& b) {
  if (a.crc != b.crc) return (a.crc < b.crc) ? -1 : 1;
  if (a.romIdx != b.romIdx) return (a.romIdx < b.romIdx) ? -1 : 1;
  return 0;
}

inline int compareBaseOrder(const RomFile* roms, int32_t idxA, int32_t idxB) {
  const char* startA = nullptr;
  const char* startB = nullptr;
  size_t lenA = 0;
  size_t lenB = 0;
  baseNameSpan(roms[idxA].filename, &startA, &lenA);
  baseNameSpan(roms[idxB].filename, &startB, &lenB);
  const int cmp = compareSpanIgnoreAsciiCase(startA, lenA, startB, lenB);
  if (cmp != 0) return cmp;
  return (idxA < idxB) ? -1 : ((idxA > idxB) ? 1 : 0);
}

inline void siftDownKeys(KeyEntry* arr, int count, int root) {
  while (true) {
    const int left = root * 2 + 1;
    if (left >= count) break;
    int largest = root;
    if (compareKeyEntries(arr[largest], arr[left]) < 0) largest = left;
    const int right = left + 1;
    if (right < count && compareKeyEntries(arr[largest], arr[right]) < 0) largest = right;
    if (largest == root) break;
    const KeyEntry tmp = arr[root];
    arr[root] = arr[largest];
    arr[largest] = tmp;
    root = largest;
  }
}

inline void siftDownBase(int32_t* arr, int count, int root, const RomFile* roms) {
  while (true) {
    const int left = root * 2 + 1;
    if (left >= count) break;
    int largest = root;
    if (compareBaseOrder(roms, arr[largest], arr[left]) < 0) largest = left;
    const int right = left + 1;
    if (right < count && compareBaseOrder(roms, arr[largest], arr[right]) < 0) largest = right;
    if (largest == root) break;
    const int32_t tmp = arr[root];
    arr[root] = arr[largest];
    arr[largest] = tmp;
    root = largest;
  }
}
} // namespace detail

class CatalogMatcher {
public:
  CatalogMatcher() : roms_(nullptr), count_(0), keys_(nullptr), baseOrder_(nullptr) {}

  bool init(RomFile* roms, int count, KeyEntry* keysBuf, int32_t* baseOrderBuf) {
    if (!roms || count <= 0 || !keysBuf || !baseOrderBuf) return false;
    roms_ = roms;
    count_ = count;
    keys_ = keysBuf;
    baseOrder_ = baseOrderBuf;

    for (int i = 0; i < count_; ++i) {
      keys_[i].crc = RomKey::crc32RelativePath(roms_[i].filename);
      keys_[i].romIdx = i;
      baseOrder_[i] = i;
    }

    for (int i = count_ / 2 - 1; i >= 0; --i) {
      detail::siftDownKeys(keys_, count_, i);
      detail::siftDownBase(baseOrder_, count_, i, roms_);
    }
    for (int end = count_ - 1; end > 0; --end) {
      const KeyEntry tmpKey = keys_[0];
      keys_[0] = keys_[end];
      keys_[end] = tmpKey;
      detail::siftDownKeys(keys_, end, 0);

      const int32_t tmpBase = baseOrder_[0];
      baseOrder_[0] = baseOrder_[end];
      baseOrder_[end] = tmpBase;
      detail::siftDownBase(baseOrder_, end, 0, roms_);
    }
    return true;
  }

  void markArtFile(const char* entryName) {
    if (!roms_ || count_ <= 0 || !entryName) return;

    char stem[64];
    bool hasKeyCrc = false;
    uint32_t keyCrc = 0;
    if (!parseArtFilename(entryName, stem, sizeof(stem), &hasKeyCrc, &keyCrc)) {
      return;
    }
    const size_t stemLen = strlen(stem);

    if (hasKeyCrc) {
      int lo = 0;
      int hi = count_;
      while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if (keys_[mid].crc < keyCrc) {
          lo = mid + 1;
        } else {
          hi = mid;
        }
      }
      for (int i = lo; i < count_ && keys_[i].crc == keyCrc; ++i) {
        const int32_t romIdx = keys_[i].romIdx;
        char expectedKey[33];
        if (RomKey::make(roms_[romIdx].filename, expectedKey, sizeof(expectedKey)) &&
            sameNameIgnoreAsciiCase(stem, expectedKey)) {
          roms_[romIdx].hasBoxArt = 1;
        }
      }
    }

    // Legacy basename fallback applies ONLY when the basename is globally unique across roms_.
    int lo = 0;
    int hi = count_;
    while (lo < hi) {
      const int mid = lo + (hi - lo) / 2;
      const char* start = nullptr;
      size_t len = 0;
      baseNameSpan(roms_[baseOrder_[mid]].filename, &start, &len);
      if (compareSpanIgnoreAsciiCase(start, len, stem, stemLen) < 0) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    if (lo < count_) {
      const char* firstStart = nullptr;
      size_t firstLen = 0;
      baseNameSpan(roms_[baseOrder_[lo]].filename, &firstStart, &firstLen);
      if (compareSpanIgnoreAsciiCase(firstStart, firstLen, stem, stemLen) == 0) {
        bool unique = true;
        if (lo + 1 < count_) {
          const char* nextStart = nullptr;
          size_t nextLen = 0;
          baseNameSpan(roms_[baseOrder_[lo + 1]].filename, &nextStart, &nextLen);
          if (compareSpanIgnoreAsciiCase(nextStart, nextLen, stem, stemLen) == 0) {
            unique = false;
          }
        }
        if (unique) {
          roms_[baseOrder_[lo]].hasBoxArt = 1;
        }
      }
    }
  }

private:
  RomFile* roms_;
  int count_;
  KeyEntry* keys_;
  int32_t* baseOrder_;
};

} // namespace BoxArtLookup
