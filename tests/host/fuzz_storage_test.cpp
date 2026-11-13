#include "bmp_header.h"
#include "rom_file_filter.h"
#include "rom_index.h"
#include "rom_path_match.h"
#include "save_state_format.h"
#include "test_check.h"

#include <stdint.h>
#include <string.h>

namespace {

uint64_t nextRng(uint64_t& state) {
  state += 0x9E3779B97F4A7C15ULL;
  uint64_t z = state;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void fillValid24BmpHeader(uint8_t header[54], int32_t width, int32_t height, uint32_t dataOffset) {
  memset(header, 0, 54);
  header[0] = 'B';
  header[1] = 'M';
  const uint32_t fileSize = dataOffset + 64U * 64U * 3U;
  header[2] = static_cast<uint8_t>(fileSize & 0xFFU);
  header[3] = static_cast<uint8_t>((fileSize >> 8) & 0xFFU);
  header[4] = static_cast<uint8_t>((fileSize >> 16) & 0xFFU);
  header[5] = static_cast<uint8_t>((fileSize >> 24) & 0xFFU);

  header[10] = static_cast<uint8_t>(dataOffset & 0xFFU);
  header[11] = static_cast<uint8_t>((dataOffset >> 8) & 0xFFU);
  header[12] = static_cast<uint8_t>((dataOffset >> 16) & 0xFFU);
  header[13] = static_cast<uint8_t>((dataOffset >> 24) & 0xFFU);

  header[14] = 40U;
  const uint32_t rawW = static_cast<uint32_t>(width);
  const uint32_t rawH = static_cast<uint32_t>(height);
  header[18] = static_cast<uint8_t>(rawW & 0xFFU);
  header[19] = static_cast<uint8_t>((rawW >> 8) & 0xFFU);
  header[20] = static_cast<uint8_t>((rawW >> 16) & 0xFFU);
  header[21] = static_cast<uint8_t>((rawW >> 24) & 0xFFU);
  header[22] = static_cast<uint8_t>(rawH & 0xFFU);
  header[23] = static_cast<uint8_t>((rawH >> 8) & 0xFFU);
  header[24] = static_cast<uint8_t>((rawH >> 16) & 0xFFU);
  header[25] = static_cast<uint8_t>((rawH >> 24) & 0xFFU);
  header[26] = 1U;
  header[28] = 24U;
}

} // namespace

int main() {
  uint64_t rng = 0x424D4F5F46555A5AULL;

  // Deterministic boundary tests on BmpHeader::parse64x64 (including UB regression cases)
  {
    uint8_t hdr[54];
    BmpHeader::Info info;
    fillValid24BmpHeader(hdr, 64, 64, 54U);
    BMO_CHECK(BmpHeader::parse64x64(hdr, 54U + 64U * 64U * 3U, &info));
    BMO_CHECK(info.bottomUp);
    BMO_CHECK(info.bpp == 24U);

    fillValid24BmpHeader(hdr, 64, -64, 108U);
    BMO_CHECK(BmpHeader::parse64x64(hdr, 108U + 64U * 64U * 3U, &info));
    BMO_CHECK(!info.bottomUp);

    // INT32_MIN height previously triggered signed negation UB (-height)
    fillValid24BmpHeader(hdr, 64, static_cast<int32_t>(0x80000000U), 54U);
    BMO_CHECK(!BmpHeader::parse64x64(hdr, 54U + 64U * 64U * 3U, &info));

    // Data offset truncated / out of range
    fillValid24BmpHeader(hdr, 64, 64, 53U);
    BMO_CHECK(!BmpHeader::parse64x64(hdr, 65536U, &info));
    fillValid24BmpHeader(hdr, 64, 64, 0xFFFFFF00U);
    BMO_CHECK(!BmpHeader::parse64x64(hdr, 20000U, &info));
  }

  // Fuzz 4,000 random BMP headers + truncated file sizes without UB/crashes
  for (int i = 0; i < 4000; ++i) {
    uint8_t raw[54];
    for (int j = 0; j < 54; ++j) {
      raw[j] = static_cast<uint8_t>(nextRng(rng) & 0xFFU);
    }
    if ((i & 3) == 0) {
      raw[0] = 'B';
      raw[1] = 'M';
    }
    BmpHeader::Info info;
    const uint32_t fsz = static_cast<uint32_t>(nextRng(rng) & 0x3FFFFU);
    (void)BmpHeader::parse64x64(raw, fsz, &info);
  }

  // Fuzz 4,000 SaveStateFormat headers across BSS1/BSS2 sizes
  for (int i = 0; i < 4000; ++i) {
    uint8_t buf[96];
    for (size_t j = 0; j < sizeof(buf); ++j) {
      buf[j] = static_cast<uint8_t>(nextRng(rng) & 0xFFU);
    }
    if ((i & 7) == 0) {
      memcpy(buf, "BSS2", 4);
    } else if ((i & 7) == 1) {
      memcpy(buf, "BSS1", 4);
    }
    const size_t avail = static_cast<size_t>(nextRng(rng) % (sizeof(buf) + 1U));
    const size_t total = static_cast<size_t>(nextRng(rng) & 0xFFFFFU);
    SaveStateFormat::DecodedHeader decoded;
    (void)SaveStateFormat::decodeHeader(buf, avail, total, static_cast<uint8_t>(nextRng(rng) & 0x0FU),
                                        static_cast<uint32_t>(nextRng(rng)), 65536U, 0U, 131072U, &decoded);
    SaveStateFormat::HeaderV2 summary;
    (void)SaveStateFormat::inspectSummaryHeader(buf, avail, total, &summary);
  }

  // Fuzz 4,000 BmoIndexEntry records against random string tables & RomFileFilter inputs
  static constexpr const char* kExtList[] = {".gb", ".gbc", ".nes", ".md", ".sg", ".col", ".txt", ".bin", ""};
  for (int i = 0; i < 4000; ++i) {
    char nameBuf[64];
    const size_t baseLen = 1U + static_cast<size_t>(nextRng(rng) % 24U);
    for (size_t k = 0; k < baseLen; ++k) {
      const uint8_t ch = static_cast<uint8_t>(33U + (nextRng(rng) % 90U));
      nameBuf[k] = (ch == '\0') ? 'a' : static_cast<char>(ch);
    }
    nameBuf[baseLen] = '\0';
    const char* ext = kExtList[nextRng(rng) % (sizeof(kExtList) / sizeof(kExtList[0]))];
    char fullName[80];
    snprintf(fullName, sizeof(fullName), "%s%s", nameBuf, ext);
    fullName[sizeof(fullName) - 1] = '\0';

    uint8_t prefix[32];
    for (size_t k = 0; k < sizeof(prefix); ++k) {
      prefix[k] = static_cast<uint8_t>(nextRng(rng) & 0xFFU);
    }
    (void)RomFileFilter::isIgnoredName(fullName);
    (void)RomFileFilter::isPlausibleFile(fullName, static_cast<size_t>(nextRng(rng) & 0x3FFFFU), prefix, sizeof(prefix),
                                         prefix, 4U);
    (void)RomPathMatch::normalizeRelative(fullName);

    char strTab[64];
    for (size_t k = 0; k < sizeof(strTab); ++k) {
      strTab[k] = static_cast<char>(nextRng(rng) & 0xFFU);
    }
    BmoIndexEntry entry;
    entry.pathOffset = static_cast<uint32_t>(nextRng(rng) & 0x7FU);
    entry.pathLength = static_cast<uint16_t>(nextRng(rng) & 0x3FFU);
    entry.type = static_cast<uint8_t>(nextRng(rng) & 0xFFU);
    entry.flags = static_cast<uint8_t>(nextRng(rng) & 0xFFU);
    entry.fileSize = static_cast<uint32_t>(nextRng(rng));
    entry.crc32Prefix = static_cast<uint32_t>(nextRng(rng));
    entry.reserved = static_cast<uint32_t>(nextRng(rng) & 0x3U);
    const uint32_t strLen = static_cast<uint32_t>(nextRng(rng) % (sizeof(strTab) + 1U));
    (void)BmoIndex::validateEntryRecord(entry, strLen, (i & 31) == 0 ? nullptr : strTab);
  }

  return 0;
}
