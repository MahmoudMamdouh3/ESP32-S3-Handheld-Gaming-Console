#include <map>
#include <string>
#include <vector>

#include "src/storage/atomic_fs.h"
#include "test_check.h"

struct MemVfs {
  std::map<std::string, std::vector<uint8_t>> files;
  bool failRenameToFinal = false;
  bool failBackupCleanup = false;
  int corruptFinalAfterRename = 0;
};

struct MemReader {
  std::vector<uint8_t> data;
  size_t pos = 0;
};

static bool memExists(void* ctx, const char* path) {
  auto* fs = static_cast<MemVfs*>(ctx);
  return fs->files.find(path) != fs->files.end();
}

static bool memRemove(void* ctx, const char* path) {
  auto* fs = static_cast<MemVfs*>(ctx);
  if (fs->failBackupCleanup && std::string(path).find(".bak") != std::string::npos) {
    return false;
  }
  return fs->files.erase(path) > 0;
}

static bool memRename(void* ctx, const char* src, const char* dst) {
  auto* fs = static_cast<MemVfs*>(ctx);
  if (fs->failRenameToFinal && std::string(src).find(".tmp") != std::string::npos) {
    return false;
  }
  auto it = fs->files.find(src);
  if (it == fs->files.end()) return false;
  std::vector<uint8_t> payload = it->second;
  if (fs->corruptFinalAfterRename > 0 && std::string(src).find(".tmp") != std::string::npos) {
    fs->corruptFinalAfterRename--;
    if (!payload.empty()) payload[0] ^= 0xFFU;
  }
  fs->files[dst] = payload;
  fs->files.erase(it);
  return true;
}

static bool memOpenRead(void* ctx, const char* path, AtomicFs::ReadHandle* out) {
  auto* fs = static_cast<MemVfs*>(ctx);
  auto it = fs->files.find(path);
  if (it == fs->files.end()) return false;
  auto* r = new MemReader{it->second, 0};
  out->impl = r;
  out->totalSize = r->data.size();
  return true;
}

static bool memSeekRead(void*, AtomicFs::ReadHandle* handle, size_t offset) {
  auto* r = static_cast<MemReader*>(handle->impl);
  if (offset > r->data.size()) return false;
  r->pos = offset;
  return true;
}

static int memReadChunk(void*, AtomicFs::ReadHandle* handle, uint8_t* dst, size_t len) {
  auto* r = static_cast<MemReader*>(handle->impl);
  const size_t avail = r->data.size() - r->pos;
  const size_t take = (len < avail) ? len : avail;
  if (take == 0) return 0;
  for (size_t i = 0; i < take; ++i)
    dst[i] = r->data[r->pos + i];
  r->pos += take;
  return static_cast<int>(take);
}

static void memCloseRead(void*, AtomicFs::ReadHandle* handle) {
  delete static_cast<MemReader*>(handle->impl);
  handle->impl = nullptr;
}

static uint32_t computeFullCrc(const std::vector<uint8_t>& v) {
  return AtomicFs::crc32Update(v.data(), v.size(), 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
}

int main() {
  MemVfs vfs;
  AtomicFs::FsOps ops{&vfs, memExists, memRemove, memRename, memOpenRead, memSeekRead, memReadChunk, memCloseRead};

  const std::vector<uint8_t> oldData = {'O', 'L', 'D', '1'};
  const std::vector<uint8_t> newData = {'N', 'E', 'W', '2', '3'};
  vfs.files["/saves/game.sav"] = oldData;
  vfs.files["/saves/game.sav.tmp"] = newData;

  // Normal commit promotes temp -> final, removes backup and temp.
  bool deferred = false;
  BMO_CHECK(AtomicFs::commitTransaction(ops, "/saves/game.sav.tmp", "/saves/game.sav", newData.size(), 0,
                                        newData.size(), computeFullCrc(newData), &deferred));
  BMO_CHECK(!deferred);
  BMO_CHECK(vfs.files["/saves/game.sav"] == newData);
  BMO_CHECK(!memExists(&vfs, "/saves/game.sav.bak"));
  BMO_CHECK(!memExists(&vfs, "/saves/game.sav.tmp"));

  // Rename failure rolls backup back into final path automatically.
  vfs.files["/saves/game.sav.tmp"] = oldData;
  vfs.failRenameToFinal = true;
  BMO_CHECK(!AtomicFs::commitTransaction(ops, "/saves/game.sav.tmp", "/saves/game.sav", oldData.size(), 0,
                                         oldData.size(), computeFullCrc(oldData)));
  BMO_CHECK(vfs.files["/saves/game.sav"] == newData);
  BMO_CHECK(!memExists(&vfs, "/saves/game.sav.bak"));

  // Post-rename corruption detects CRC failure and restores previous valid save.
  vfs.failRenameToFinal = false;
  vfs.corruptFinalAfterRename = 1;
  BMO_CHECK(!AtomicFs::commitTransaction(ops, "/saves/game.sav.tmp", "/saves/game.sav", oldData.size(), 0,
                                         oldData.size(), computeFullCrc(oldData)));
  BMO_CHECK(vfs.files["/saves/game.sav"] == newData);

  // Interrupted power-loss state (final absent, backup + stale temp present) recovers backup.
  vfs.files["/saves/game.sav.bak"] = oldData;
  vfs.files.erase("/saves/game.sav");
  vfs.files["/saves/game.sav.tmp"] = newData;
  BMO_CHECK(AtomicFs::recoverTransaction(ops, "/saves/game.sav"));
  BMO_CHECK(vfs.files["/saves/game.sav"] == oldData);
  BMO_CHECK(!memExists(&vfs, "/saves/game.sav.bak"));
  BMO_CHECK(!memExists(&vfs, "/saves/game.sav.tmp"));
  return 0;
}
