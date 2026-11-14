#pragma once

#include <stddef.h>

namespace RomPathMatch {

inline char asciiLower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

inline const char* normalizeRelative(const char* path) {
  if (!path) return "";
  while (*path == '/' || *path == '\\')
    ++path;
  return path;
}

inline bool equalsNormalized(const char* a, const char* b) {
  const char* pa = normalizeRelative(a);
  const char* pb = normalizeRelative(b);
  while (*pa && *pb) {
    const char ca = (*pa == '\\') ? '/' : *pa;
    const char cb = (*pb == '\\') ? '/' : *pb;
    if (ca != cb) return false;
    ++pa;
    ++pb;
  }
  return *pa == '\0' && *pb == '\0';
}

inline const char* baseNameOf(const char* path) {
  if (!path) return "";
  const char* base = path;
  for (const char* p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  return base;
}

inline bool hasPathSeparator(const char* path) {
  if (!path) return false;
  for (const char* p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') return true;
  }
  return false;
}

inline bool equalsIgnoreCaseAscii(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (asciiLower(*a++) != asciiLower(*b++)) return false;
  }
  return *a == '\0' && *b == '\0';
}

// Resolve a metadata entry (from /favorites.txt or /history.txt) against catalog paths.
// Returns matching index or -1; sets outMigrated=true when a legacy basename matched uniquely.
template <typename GetPathFn>
inline int resolveCatalogIndex(const char* entry, int count, GetPathFn getPath, bool* outMigrated = nullptr) {
  if (outMigrated) *outMigrated = false;
  if (!entry || entry[0] == '\0' || count <= 0) return -1;

  for (int i = 0; i < count; ++i) {
    const char* candidate = getPath(i);
    if (candidate && equalsNormalized(candidate, entry)) {
      return i;
    }
  }

  // Only perform legacy basename migration when the stored metadata line has no subfolder.
  if (hasPathSeparator(normalizeRelative(entry))) {
    return -1;
  }
  const char* needle = baseNameOf(entry);
  if (!needle || needle[0] == '\0') return -1;

  int matchedIndex = -1;
  int matchCount = 0;
  for (int i = 0; i < count; ++i) {
    const char* candidate = getPath(i);
    if (!candidate) continue;
    if (equalsIgnoreCaseAscii(baseNameOf(candidate), needle)) {
      matchedIndex = i;
      matchCount++;
      if (matchCount > 1) return -1; // Ambiguous across multiple folders: refuse unsafe guess
    }
  }
  if (matchCount == 1) {
    if (outMigrated) *outMigrated = true;
    return matchedIndex;
  }
  return -1;
}

} // namespace RomPathMatch
