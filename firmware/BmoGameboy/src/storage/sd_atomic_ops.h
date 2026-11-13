#pragma once

#include "atomic_fs.h"
#include "../hal/config.h"
#if FEATURE_SD_CARD
#include <SD.h>
#endif

namespace SdAtomicOps {

#if FEATURE_SD_CARD
namespace detail {
inline bool sdExists(void*, const char* path) {
  return SD.exists(path);
}
inline bool sdRemove(void*, const char* path) {
  return SD.remove(path);
}
inline bool sdRename(void*, const char* src, const char* dst) {
  return SD.rename(src, dst);
}

inline bool sdOpenRead(void*, const char* path, AtomicFs::ReadHandle* outHandle) {
  File* fp = new File(SD.open(path, FILE_READ));
  if (!fp || !(*fp)) {
    delete fp;
    return false;
  }
  outHandle->impl = fp;
  outHandle->totalSize = static_cast<size_t>(fp->size());
  return true;
}

inline bool sdSeekRead(void*, AtomicFs::ReadHandle* handle, size_t offset) {
  if (!handle || !handle->impl) return false;
  return static_cast<File*>(handle->impl)->seek(offset);
}

inline int sdReadChunk(void*, AtomicFs::ReadHandle* handle, uint8_t* dst, size_t len) {
  if (!handle || !handle->impl) return -1;
  return static_cast<File*>(handle->impl)->read(dst, len);
}

inline void sdCloseRead(void*, AtomicFs::ReadHandle* handle) {
  if (!handle || !handle->impl) return;
  File* fp = static_cast<File*>(handle->impl);
  fp->close();
  delete fp;
  handle->impl = nullptr;
}
} // namespace detail

inline AtomicFs::FsOps makeOps() {
  AtomicFs::FsOps ops{};
  ops.exists = detail::sdExists;
  ops.remove = detail::sdRemove;
  ops.rename = detail::sdRename;
  ops.openRead = detail::sdOpenRead;
  ops.seekRead = detail::sdSeekRead;
  ops.readChunk = detail::sdReadChunk;
  ops.closeRead = detail::sdCloseRead;
  return ops;
}
#endif

} // namespace SdAtomicOps
