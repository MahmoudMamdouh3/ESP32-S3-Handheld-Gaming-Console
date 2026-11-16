#pragma once
#include "Arduino.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef FILE_READ
#define FILE_READ "r"
#endif
#ifndef FILE_WRITE
#define FILE_WRITE "w"
#endif

namespace fs {

class File {
public:
  struct Backing {
    std::vector<uint8_t> data;
  };

  File() = default;
  File(std::shared_ptr<Backing> backing, bool writeMode)
      : backing_(backing), pos_(writeMode ? backing->data.size() : 0U), writeMode_(writeMode),
        valid_(backing != nullptr) {}

  explicit operator bool() const { return valid_; }

  size_t size() const { return backing_ ? backing_->data.size() : 0U; }

  size_t write(const uint8_t* src, size_t len) {
    if (!valid_ || !backing_ || !writeMode_ || !src) return 0U;
    if (pos_ + len > backing_->data.size()) backing_->data.resize(pos_ + len, 0U);
    memcpy(backing_->data.data() + pos_, src, len);
    pos_ += len;
    return len;
  }

  size_t write(uint8_t byteVal) { return write(&byteVal, 1U); }

  size_t read(uint8_t* dst, size_t len) {
    if (!valid_ || !backing_ || !dst || pos_ >= backing_->data.size()) return 0U;
    const size_t avail = backing_->data.size() - pos_;
    const size_t take = (len < avail) ? len : avail;
    memcpy(dst, backing_->data.data() + pos_, take);
    pos_ += take;
    return take;
  }

  int read() {
    uint8_t b = 0U;
    return (read(&b, 1U) == 1U) ? static_cast<int>(b) : -1;
  }

  bool seek(size_t offset) {
    if (!valid_ || !backing_ || offset > backing_->data.size()) return false;
    pos_ = offset;
    return true;
  }

  void flush() {}
  void close() { valid_ = false; }

private:
  std::shared_ptr<Backing> backing_;
  size_t pos_ = 0U;
  bool writeMode_ = false;
  bool valid_ = false;
};

} // namespace fs

using fs::File;

class SDClass {
public:
  void reset() { files_.clear(); }

  bool exists(const char* path) const { return path && files_.find(path) != files_.end(); }

  bool mkdir(const char*) { return true; }

  bool remove(const char* path) { return path && files_.erase(path) > 0U; }

  bool rename(const char* src, const char* dst) {
    if (!src || !dst) return false;
    auto it = files_.find(src);
    if (it == files_.end()) return false;
    files_[dst] = it->second;
    files_.erase(it);
    return true;
  }

  File open(const char* path, const char* mode = FILE_READ) {
    if (!path) return File();
    const bool isWrite = (mode && (mode[0] == 'w' || mode[0] == 'a'));
    if (!isWrite) {
      auto it = files_.find(path);
      return (it != files_.end()) ? File(it->second, false) : File();
    }
    auto backing = std::make_shared<File::Backing>();
    files_[path] = backing;
    return File(backing, true);
  }

  const std::vector<uint8_t>* rawBytes(const char* path) const {
    if (!path) return nullptr;
    auto it = files_.find(path);
    return (it != files_.end()) ? &it->second->data : nullptr;
  }

private:
  std::map<std::string, std::shared_ptr<File::Backing>> files_;
};

inline SDClass SD;
