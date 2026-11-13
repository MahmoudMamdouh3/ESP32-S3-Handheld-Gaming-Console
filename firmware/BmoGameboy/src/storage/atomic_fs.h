#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace AtomicFs {

inline constexpr uint32_t crc32Update(const uint8_t* data, size_t length, uint32_t crc) {
  if (!data || length == 0) return crc;
  constexpr uint32_t kNibble[16] = {
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
  };
  for (size_t i = 0; i < length; ++i) {
    const uint8_t b = data[i];
    crc = (crc >> 4) ^ kNibble[(crc ^ b) & 0x0FU];
    crc = (crc >> 4) ^ kNibble[(crc ^ (b >> 4)) & 0x0FU];
  }
  return crc;
}

inline bool makeSuffixedPath(const char* finalPath,
                             const char* suffix,
                             char* outPath,
                             size_t maxLen) {
  if (!finalPath || !suffix || !outPath || maxLen == 0) return false;
  const int n = snprintf(outPath, maxLen, "%s%s", finalPath, suffix);
  return n > 0 && static_cast<size_t>(n) < maxLen;
}

inline bool makeTempPath(const char* finalPath, char* outPath, size_t maxLen) {
  return makeSuffixedPath(finalPath, ".tmp", outPath, maxLen);
}

inline bool makeBackupPath(const char* finalPath, char* outPath, size_t maxLen) {
  return makeSuffixedPath(finalPath, ".bak", outPath, maxLen);
}

struct ReadHandle {
  void* impl = nullptr;
  size_t totalSize = 0;
};

struct FsOps {
  void* ctx = nullptr;
  bool (*exists)(void* ctx, const char* path) = nullptr;
  bool (*remove)(void* ctx, const char* path) = nullptr;
  bool (*rename)(void* ctx, const char* srcPath, const char* dstPath) = nullptr;
  bool (*openRead)(void* ctx, const char* path, ReadHandle* outHandle) = nullptr;
  bool (*seekRead)(void* ctx, ReadHandle* handle, size_t offset) = nullptr;
  int (*readChunk)(void* ctx, ReadHandle* handle, uint8_t* dst, size_t len) = nullptr;
  void (*closeRead)(void* ctx, ReadHandle* handle) = nullptr;
};

/**
 * Recover an interrupted `.tmp` / `.bak` write before reading or replacing
 * `finalPath`. If `finalPath` is missing and `.bak` exists, restores `.bak`.
 */
inline bool recoverTransaction(const FsOps& ops, const char* finalPath) {
  if (!ops.exists || !ops.remove || !ops.rename || !finalPath) return false;
  char tmpPath[144];
  char bakPath[144];
  if (!makeTempPath(finalPath, tmpPath, sizeof(tmpPath)) ||
      !makeBackupPath(finalPath, bakPath, sizeof(bakPath))) {
    return false;
  }

  const bool hasFinal = ops.exists(ops.ctx, finalPath);
  const bool hasBak = ops.exists(ops.ctx, bakPath);
  if (hasFinal) {
    if (hasBak && !ops.remove(ops.ctx, bakPath)) return false;
    if (ops.exists(ops.ctx, tmpPath) && !ops.remove(ops.ctx, tmpPath)) return false;
    return true;
  }

  if (hasBak) {
    if (!ops.rename(ops.ctx, bakPath, finalPath)) return false;
  }
  if (ops.exists(ops.ctx, tmpPath) && !ops.remove(ops.ctx, tmpPath)) return false;
  return !hasBak || ops.exists(ops.ctx, finalPath);
}

inline bool verifyFilePayloadCrc(const FsOps& ops,
                                 const char* path,
                                 size_t expectedFileSize,
                                 size_t payloadOffset,
                                 size_t payloadSize,
                                 uint32_t expectedCrc) {
  if (!ops.openRead || !ops.seekRead || !ops.readChunk || !ops.closeRead || !path) {
    return false;
  }
  ReadHandle handle{};
  if (!ops.openRead(ops.ctx, path, &handle)) return false;
  if (handle.totalSize != expectedFileSize ||
      payloadOffset > expectedFileSize ||
      payloadSize > expectedFileSize - payloadOffset ||
      !ops.seekRead(ops.ctx, &handle, payloadOffset)) {
    ops.closeRead(ops.ctx, &handle);
    return false;
  }

  uint8_t buf[512];
  size_t remaining = payloadSize;
  uint32_t crc = 0xFFFFFFFFU;
  while (remaining > 0) {
    const size_t req = remaining < sizeof(buf) ? remaining : sizeof(buf);
    const int got = ops.readChunk(ops.ctx, &handle, buf, req);
    if (got <= 0 || static_cast<size_t>(got) > req) {
      ops.closeRead(ops.ctx, &handle);
      return false;
    }
    crc = crc32Update(buf, static_cast<size_t>(got), crc);
    remaining -= static_cast<size_t>(got);
  }
  ops.closeRead(ops.ctx, &handle);
  return (crc ^ 0xFFFFFFFFU) == expectedCrc;
}

/**
 * Verify `tempPath`, rotate any existing `finalPath` to `.bak`, promote `tempPath`
 * -> `finalPath`, re-verify `finalPath`, and clean up `.bak`. Rolls back cleanly
 * on rename or post-install verification failure.
 */
inline bool commitTransaction(const FsOps& ops,
                              const char* tempPath,
                              const char* finalPath,
                              size_t expectedFileSize,
                              size_t payloadOffset,
                              size_t payloadSize,
                              uint32_t expectedCrc,
                              bool* outBackupCleanupDeferred = nullptr) {
  if (outBackupCleanupDeferred) *outBackupCleanupDeferred = false;
  if (!ops.exists || !ops.remove || !ops.rename || !tempPath || !finalPath) {
    return false;
  }
  if (!verifyFilePayloadCrc(ops, tempPath, expectedFileSize, payloadOffset, payloadSize, expectedCrc)) {
    return false;
  }

  char bakPath[144];
  if (!makeBackupPath(finalPath, bakPath, sizeof(bakPath))) return false;
  const bool hadFinal = ops.exists(ops.ctx, finalPath);
  if (hadFinal && !ops.rename(ops.ctx, finalPath, bakPath)) {
    return false;
  }
  if (!ops.rename(ops.ctx, tempPath, finalPath)) {
    if (hadFinal) ops.rename(ops.ctx, bakPath, finalPath);
    return false;
  }
  if (!verifyFilePayloadCrc(ops, finalPath, expectedFileSize, payloadOffset, payloadSize, expectedCrc)) {
    ops.remove(ops.ctx, finalPath);
    if (hadFinal) ops.rename(ops.ctx, bakPath, finalPath);
    return false;
  }
  if (hadFinal && ops.exists(ops.ctx, bakPath) && !ops.remove(ops.ctx, bakPath)) {
    if (outBackupCleanupDeferred) *outBackupCleanupDeferred = true;
  }
  return true;
}

}  // namespace AtomicFs
