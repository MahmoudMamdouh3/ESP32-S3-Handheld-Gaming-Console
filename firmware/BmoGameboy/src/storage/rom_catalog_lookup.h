// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include "rom_path_match.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(ESP_PLATFORM) || (defined(__has_include) && __has_include(<esp_heap_caps.h>))
#include <esp_heap_caps.h>
#define BMO_HAS_HEAP_CAPS_HEADER 1
#else
#define BMO_HAS_HEAP_CAPS_HEADER 0
#endif

namespace RomPathMatch {

using IndexedPathFn = const char* (*)(int);

struct CatalogHashEntry {
  uint32_t hash;
  int32_t index;
};

inline uint32_t fnv1aLowerAscii(const char* s) {
  if (!s) return 0U;
  uint32_t hash = 2166136261U;
  while (*s) {
    unsigned char ch = static_cast<unsigned char>(*s++);
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<unsigned char>(ch + ('a' - 'A'));
    }
    hash ^= ch;
    hash *= 16777619U;
  }
  return hash;
}

inline uint32_t fnv1aNormalized(const char* s) {
  const char* p = normalizeRelative(s);
  uint32_t hash = 2166136261U;
  while (*p) {
    const unsigned char raw = static_cast<unsigned char>(*p++);
    const unsigned char ch = (raw == '\\') ? static_cast<unsigned char>('/') : raw;
    hash ^= ch;
    hash *= 16777619U;
  }
  return hash;
}

namespace detail {

inline bool hashEntryLess(const CatalogHashEntry& a, const CatalogHashEntry& b) {
  return (a.hash < b.hash) || (a.hash == b.hash && a.index < b.index);
}

inline void siftDown(CatalogHashEntry* arr, int root, int endIdx) {
  while (root * 2 + 1 < endIdx) {
    int child = root * 2 + 1;
    if (child + 1 < endIdx && hashEntryLess(arr[child], arr[child + 1])) {
      child++;
    }
    if (!hashEntryLess(arr[root], arr[child])) {
      return;
    }
    const CatalogHashEntry tmp = arr[root];
    arr[root] = arr[child];
    arr[child] = tmp;
    root = child;
  }
}

} // namespace detail

// Iterative in-place heapsort: O(N log N) worst-case, zero recursion, ~48 B stack.
inline void sortHashEntries(CatalogHashEntry* arr, int count) {
  if (!arr || count <= 1) return;
  for (int start = (count - 2) / 2; start >= 0; --start) {
    detail::siftDown(arr, start, count);
  }
  for (int endIdx = count - 1; endIdx > 0; --endIdx) {
    const CatalogHashEntry tmp = arr[0];
    arr[0] = arr[endIdx];
    arr[endIdx] = tmp;
    detail::siftDown(arr, 0, endIdx);
  }
}

// 4-pass LSD radix sort into `dst` (even number of passes ends cleanly back in `arr`).
// Uses 512 B stack for 256 16-bit bucket counts (safe under 1,024 B limit for N <= 65,535).
inline bool radixSortHashEntries(CatalogHashEntry* arr, CatalogHashEntry* scratch, int count) {
  if (!arr || !scratch || count <= 1 || count > 65535) return false;
  constexpr size_t kRadixBuckets = 256;
  CatalogHashEntry* src = arr;
  CatalogHashEntry* dst = scratch;
  for (unsigned shift = 0; shift < 32U; shift += 8U) {
    uint16_t buckets[kRadixBuckets] = {};
    for (int i = 0; i < count; ++i) {
      buckets[(src[i].hash >> shift) & 0xFFU]++;
    }
    uint16_t running = 0U;
    for (int b = 0; b < 256; ++b) {
      const uint16_t c = buckets[b];
      buckets[b] = running;
      running = static_cast<uint16_t>(running + c);
    }
    for (int i = 0; i < count; ++i) {
      const unsigned bucket = (src[i].hash >> shift) & 0xFFU;
      dst[buckets[bucket]++] = src[i];
    }
    CatalogHashEntry* tmpPtr = src;
    src = dst;
    dst = tmpPtr;
  }
  return true;
}

inline int lowerBoundHash(const CatalogHashEntry* arr, int count, uint32_t targetHash) {
  int lo = 0;
  int hi = count;
  while (lo < hi) {
    const int mid = lo + (hi - lo) / 2;
    if (arr[mid].hash < targetHash) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

// Strips directory prefix and last extension into outBase (e.g. "gb/Zelda.gb" -> "Zelda").
inline void extractStemBaseName(const char* romFilename, char* outBase, size_t maxLen) {
  if (!outBase || maxLen == 0U) return;
  outBase[0] = '\0';
  if (!romFilename) return;
  const char* base = baseNameOf(romFilename);
  const char* dot = strrchr(base, '.');
  size_t len = (dot && dot > base) ? static_cast<size_t>(dot - base) : strlen(base);
  if (len >= maxLen) len = maxLen - 1U;
  memcpy(outBase, base, len);
  outBase[len] = '\0';
}

class CatalogPathLookup {
public:
  template <typename PathFn>
  bool init(int count, PathFn&& pathAt, CatalogHashEntry* normBuf, CatalogHashEntry* baseBuf) {
    count_ = 0;
    normEntries_ = normBuf;
    baseEntries_ = baseBuf;
    baseBuilt_ = false;
    if (count < 0) return false;
    if (count == 0) return true;
    if (!normBuf || !baseBuf) return false;

    for (int i = 0; i < count; ++i) {
      normEntries_[i] = {fnv1aNormalized(pathAt(i)), static_cast<int32_t>(i)};
    }
    if (!radixSortHashEntries(normEntries_, baseBuf, count)) {
      sortHashEntries(normEntries_, count);
    }
    count_ = count;
    return true;
  }

  void reset() {
    count_ = 0;
    normEntries_ = nullptr;
    baseEntries_ = nullptr;
    baseBuilt_ = false;
  }

  int count() const { return count_; }

  template <typename PathFn> int findExact(const char* query, PathFn&& pathAt) const {
    if (!query || !query[0] || !normEntries_ || count_ <= 0) return -1;
    const uint32_t qHash = fnv1aNormalized(query);
    int pos = lowerBoundHash(normEntries_, count_, qHash);
    while (pos < count_ && normEntries_[pos].hash == qHash) {
      const int idx = normEntries_[pos].index;
      if (equalsNormalized(pathAt(idx), query)) {
        return idx;
      }
      pos++;
    }
    return -1;
  }

  template <typename PathFn> int resolve(const char* query, PathFn&& pathAt, bool* outMigrated = nullptr) const {
    if (outMigrated) *outMigrated = false;
    if (!query || !query[0] || count_ <= 0) return -1;

    const int exactIdx = findExact(query, pathAt);
    if (exactIdx >= 0) return exactIdx;

    if (hasPathSeparator(normalizeRelative(query)) || !baseEntries_) return -1;
    const char* needle = baseNameOf(query);
    if (!needle || needle[0] == '\0') return -1;

    if (!baseBuilt_) {
      for (int i = 0; i < count_; ++i) {
        baseEntries_[i] = {fnv1aLowerAscii(baseNameOf(pathAt(i))), static_cast<int32_t>(i)};
      }
      sortHashEntries(baseEntries_, count_);
      baseBuilt_ = true;
    }

    const uint32_t qBaseHash = fnv1aLowerAscii(needle);
    int pos = lowerBoundHash(baseEntries_, count_, qBaseHash);
    int matches = 0;
    int uniqueMatch = -1;
    while (pos < count_ && baseEntries_[pos].hash == qBaseHash) {
      const int idx = baseEntries_[pos].index;
      if (equalsIgnoreCaseAscii(baseNameOf(pathAt(idx)), needle)) {
        matches++;
        if (matches > 1) return -1;
        uniqueMatch = idx;
      }
      pos++;
    }
    if (matches == 1) {
      if (outMigrated) *outMigrated = true;
      return uniqueMatch;
    }
    return -1;
  }

private:
  int count_ = 0;
  CatalogHashEntry* normEntries_ = nullptr;
  CatalogHashEntry* baseEntries_ = nullptr;
  mutable bool baseBuilt_ = false;
};

class StemBaseIndex {
public:
  template <typename PathFn> bool init(int count, PathFn&& pathAt, CatalogHashEntry* entryBuf) {
    count_ = 0;
    entries_ = entryBuf;
    if (count < 0) return false;
    if (count == 0) return true;
    if (!entryBuf) return false;

    char stem[64];
    for (int i = 0; i < count; ++i) {
      extractStemBaseName(pathAt(i), stem, sizeof(stem));
      entries_[i] = {fnv1aLowerAscii(stem), static_cast<int32_t>(i)};
    }
    sortHashEntries(entries_, count);
    count_ = count;
    return true;
  }

  void reset() {
    count_ = 0;
    entries_ = nullptr;
  }

  template <typename PathFn>
  bool isUniqueForRom(const char* romFilename, const char* legacyBase, PathFn&& pathAt) const {
    if (!romFilename || !legacyBase || legacyBase[0] == '\0' || !entries_ || count_ <= 0) {
      return false;
    }
    const uint32_t qHash = fnv1aLowerAscii(legacyBase);
    int pos = lowerBoundHash(entries_, count_, qHash);
    char otherBase[64];
    while (pos < count_ && entries_[pos].hash == qHash) {
      const char* candidatePath = pathAt(entries_[pos].index);
      if (candidatePath && strcmp(candidatePath, romFilename) != 0) {
        extractStemBaseName(candidatePath, otherBase, sizeof(otherBase));
        if (equalsIgnoreCaseAscii(legacyBase, otherBase)) {
          return false;
        }
      }
      pos++;
    }
    return true;
  }

private:
  int count_ = 0;
  CatalogHashEntry* entries_ = nullptr;
};

#if BMO_HAS_HEAP_CAPS_HEADER

class OwnedCatalogPathLookup {
public:
  ~OwnedCatalogPathLookup() { freeBuffers(); }

  void invalidate() {
    builtGeneration_ = 0U;
    builtCount_ = -1;
    builtList_ = nullptr;
    pathAt_ = nullptr;
    lookup_.reset();
  }

  bool ensureBuilt(int count, const void* listToken, uint32_t generation, IndexedPathFn pathAt) {
    if (builtCount_ == count && builtList_ == listToken && builtGeneration_ == generation && generation != 0U &&
        pathAt_ == pathAt) {
      return (count == 0) || (normBuf_ != nullptr && baseBuf_ != nullptr);
    }
    if (count < 0 || !pathAt) {
      invalidate();
      return false;
    }
    pathAt_ = pathAt;
    if (count == 0) {
      builtCount_ = 0;
      builtList_ = listToken;
      builtGeneration_ = generation;
      lookup_.init(0, pathAt, nullptr, nullptr);
      return true;
    }
    if (!ensureCapacity(count)) {
      invalidate();
      return false;
    }
    if (!lookup_.init(count, pathAt, normBuf_, baseBuf_)) {
      invalidate();
      return false;
    }
    builtCount_ = count;
    builtList_ = listToken;
    builtGeneration_ = generation;
    return true;
  }

  int findExact(const char* query) const { return pathAt_ ? lookup_.findExact(query, pathAt_) : -1; }

  int resolve(const char* query, bool* outMigrated = nullptr) const {
    return pathAt_ ? lookup_.resolve(query, pathAt_, outMigrated) : -1;
  }

  void freeBuffers() {
    if (normBuf_) {
      heap_caps_free(normBuf_);
      normBuf_ = nullptr;
    }
    if (baseBuf_) {
      heap_caps_free(baseBuf_);
      baseBuf_ = nullptr;
    }
    capacity_ = 0;
    invalidate();
  }

private:
  bool ensureCapacity(int needed) {
    if (needed <= capacity_ && normBuf_ && baseBuf_) return true;
    freeBuffers();
    const size_t bytes = static_cast<size_t>(needed) * sizeof(CatalogHashEntry);
    normBuf_ = static_cast<CatalogHashEntry*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    baseBuf_ = static_cast<CatalogHashEntry*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    if (!normBuf_ || !baseBuf_) {
      freeBuffers();
      return false;
    }
    capacity_ = needed;
    return true;
  }

  CatalogPathLookup lookup_{};
  IndexedPathFn pathAt_ = nullptr;
  CatalogHashEntry* normBuf_ = nullptr;
  CatalogHashEntry* baseBuf_ = nullptr;
  int capacity_ = 0;
  int builtCount_ = -1;
  const void* builtList_ = nullptr;
  uint32_t builtGeneration_ = 0U;
};

// PSRAM-cached O(log N) uniqueness index for legacy stem names across a ROM catalog.
class OwnedStemBaseIndex {
public:
  ~OwnedStemBaseIndex() { freeBuffers(); }

  bool ensureBuilt(int count, const void* listToken, uint32_t generation, IndexedPathFn pathAt) {
    if (builtCount_ == count && builtList_ == listToken && builtGeneration_ == generation && generation != 0U &&
        pathAt_ == pathAt) {
      return (count == 0) || (entries_ != nullptr);
    }
    if (count < 0 || !pathAt) {
      invalidate();
      return false;
    }
    pathAt_ = pathAt;
    if (count == 0) {
      index_.init(0, pathAt, nullptr);
      builtCount_ = 0;
      builtList_ = listToken;
      builtGeneration_ = generation;
      return true;
    }
    if (!ensureCapacity(count)) {
      invalidate();
      return false;
    }
    if (!index_.init(count, pathAt, entries_)) {
      invalidate();
      return false;
    }
    builtCount_ = count;
    builtList_ = listToken;
    builtGeneration_ = generation;
    return true;
  }

  bool isUniqueForRom(const char* romFilename, const char* legacyBase) const {
    return pathAt_ ? index_.isUniqueForRom(romFilename, legacyBase, pathAt_) : false;
  }

  void invalidate() {
    builtCount_ = -1;
    builtList_ = nullptr;
    builtGeneration_ = 0U;
    pathAt_ = nullptr;
    index_.reset();
  }

  void freeBuffers() {
    if (entries_) {
      heap_caps_free(entries_);
      entries_ = nullptr;
    }
    capacity_ = 0;
    invalidate();
  }

private:
  bool ensureCapacity(int needed) {
    if (needed <= capacity_ && entries_) return true;
    freeBuffers();
    entries_ = static_cast<CatalogHashEntry*>(
        heap_caps_malloc(static_cast<size_t>(needed) * sizeof(CatalogHashEntry), MALLOC_CAP_SPIRAM));
    if (!entries_) return false;
    capacity_ = needed;
    return true;
  }

  StemBaseIndex index_{};
  IndexedPathFn pathAt_ = nullptr;
  CatalogHashEntry* entries_ = nullptr;
  int capacity_ = 0;
  int builtCount_ = -1;
  const void* builtList_ = nullptr;
  uint32_t builtGeneration_ = 0U;
};

#endif // BMO_HAS_HEAP_CAPS_HEADER

} // namespace RomPathMatch
