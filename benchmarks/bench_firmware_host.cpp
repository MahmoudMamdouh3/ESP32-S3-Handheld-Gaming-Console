// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cores/pico_blit.h"
#include "hal/panel_color.h"
#include "hal/profiler.h"
#include "storage/atomic_fs.h"
#include "storage/bmp_header.h"
#include "storage/box_art_lookup.h"
#include "storage/rom_index.h"
#include "storage/rom_catalog_lookup.h"
#include "storage/rom_path_match.h"
#include "storage/sd_card.h"
#include "ui/bmo_face.h"
#include "ui/bmo_face_sdf.h"

namespace {

uint64_t g_benchClockUs = 1000000ULL;
uint32_t g_rngState = 0x12345678U;

uint32_t nextBenchRandom() {
  g_rngState ^= g_rngState << 13;
  g_rngState ^= g_rngState >> 17;
  g_rngState ^= g_rngState << 5;
  return g_rngState;
}

volatile uint64_t g_sink = 0;

} // namespace

extern "C" unsigned long millis() {
  return static_cast<unsigned long>(g_benchClockUs / 1000ULL);
}

extern "C" unsigned long micros() {
  return static_cast<unsigned long>(g_benchClockUs);
}

void randomSeed(unsigned long seed) {
  g_rngState = static_cast<uint32_t>(seed ? seed : 1U);
}

long random(long upperBound) {
  return upperBound > 0 ? static_cast<long>(nextBenchRandom() % static_cast<uint32_t>(upperBound)) : 0L;
}

long random(long lowerBound, long upperBound) {
  if (upperBound <= lowerBound) return lowerBound;
  return lowerBound + random(upperBound - lowerBound);
}

extern "C" void* heap_caps_malloc(size_t size, uint32_t) {
  return malloc(size ? size : 1U);
}

extern "C" void* heap_caps_calloc(size_t n, size_t size, uint32_t) {
  return calloc(n ? n : 1U, size ? size : 1U);
}

extern "C" void* heap_caps_realloc(void* ptr, size_t size, uint32_t) {
  return realloc(ptr, size ? size : 1U);
}

extern "C" void heap_caps_free(void* memory) {
  free(memory);
}

extern "C" size_t heap_caps_get_free_size(uint32_t) {
  return 7U * 1024U * 1024U;
}

extern "C" size_t heap_caps_get_largest_free_block(uint32_t) {
  return 6U * 1024U * 1024U;
}

namespace DisplayEmu {
void pushPixelsAt(int, int, int, int, const uint16_t*) {}
} // namespace DisplayEmu

namespace {

using Clock = std::chrono::steady_clock;

template <typename Fn>
double measureUsPerIter(int warmupIters, int measureIters, Fn&& fn) {
  for (int i = 0; i < warmupIters; ++i) {
    fn(i);
  }
  const auto t0 = Clock::now();
  for (int i = 0; i < measureIters; ++i) {
    fn(i);
  }
  const auto t1 = Clock::now();
  const double totalUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
  return totalUs / static_cast<double>(measureIters);
}

// Synthetic 10k catalog fixture
struct SyntheticCatalog {
  std::vector<std::string> paths;
  std::vector<RomFile> roms;

  explicit SyntheticCatalog(int count) {
    static const char* const kDirs[] = {"gb", "gbc", "nes", "sms", "pce", "atari", "pico8", "genesis", "snes", "gba"};
    static const char* const kExts[] = {".gb", ".gbc", ".nes", ".sms", ".pce", ".a26", ".p8", ".gen", ".sfc", ".gb"};
    paths.reserve(static_cast<size_t>(count));
    roms.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      char buf[96];
      const int d = i % 10;
      snprintf(buf, sizeof(buf), "%s/Retro_Title_%05d%s", kDirs[d], i, kExts[d]);
      paths.emplace_back(buf);
    }
    for (int i = 0; i < count; ++i) {
      roms[static_cast<size_t>(i)].filename = paths[static_cast<size_t>(i)].c_str();
      roms[static_cast<size_t>(i)].fileSize = 32768U + static_cast<uint32_t>(i * 16);
      roms[static_cast<size_t>(i)].crc32Prefix = 0xA0000000U ^ static_cast<uint32_t>(i);
      roms[static_cast<size_t>(i)].type = ROM_GB;
      roms[static_cast<size_t>(i)].isFavorite = 0;
      roms[static_cast<size_t>(i)].hasBoxArt = 0;
      roms[static_cast<size_t>(i)].isPopular = 0;
    }
  }
};

double benchBmoFaceSdfSnapshot() {
  std::vector<uint16_t> fb(320U * 240U, 0);
  static const int kExprs[] = {0, 1, 2, 6, 15};
  return measureUsPerIter(3, 25, [&](int iter) {
    const int idx = kExprs[iter % 5];
    BmoFaceSdf::renderTargetSnapshot(BmoFaceSdf::EXPR_TABLE[idx], idx == 10, fb.data(), 320, 240);
    g_sink ^= fb[static_cast<size_t>(iter * 97) % fb.size()];
  });
}

double benchBmoFaceFullPipeline() {
  BmoFace::begin();
  std::vector<uint16_t> fb(320U * 240U, 0);
  static const BmoFace::BmoExpression kExprs[] = {
      BmoFace::IDLE, BmoFace::HAPPY, BmoFace::SURPRISED, BmoFace::ERROR, BmoFace::LOVE,
  };
  return measureUsPerIter(3, 25, [&](int iter) {
    g_benchClockUs += 16667ULL;
    BmoFace::setExpression(kExprs[iter % 5]);
    BmoFace::update();
    BmoFace::renderFullScreen(fb.data(), 320, 240);
    g_sink ^= fb[static_cast<size_t>(iter * 131) % fb.size()];
  });
}

double benchMetadataResolve10k(const SyntheticCatalog& cat) {
  auto getPath = [&](int idx) -> const char* { return cat.roms[static_cast<size_t>(idx)].filename; };
  std::vector<std::string> queries;
  for (int q = 0; q < 100; ++q) {
    const int idx = (q * 97 + 13) % static_cast<int>(cat.roms.size());
    queries.emplace_back(cat.roms[static_cast<size_t>(idx)].filename);
  }
  const int n = static_cast<int>(cat.roms.size());
  std::vector<RomPathMatch::CatalogHashEntry> normBuf(static_cast<size_t>(n));
  std::vector<RomPathMatch::CatalogHashEntry> baseBuf(static_cast<size_t>(n));
  return measureUsPerIter(1, 5, [&](int) {
    RomPathMatch::CatalogPathLookup lookup;
    lookup.init(n, getPath, normBuf.data(), baseBuf.data());
    int sum = 0;
    for (const auto& q : queries) {
      bool migrated = false;
      sum += lookup.resolve(q.c_str(), getPath, &migrated);
    }
    g_sink ^= static_cast<uint64_t>(sum);
  });
}

double benchCatalogFindByPath10k(const SyntheticCatalog& cat) {
  auto getPath = [&](int idx) -> const char* { return cat.roms[static_cast<size_t>(idx)].filename; };
  std::vector<std::string> queries;
  for (int q = 0; q < 500; ++q) {
    const int idx = (q * 19 + 7) % static_cast<int>(cat.roms.size());
    queries.emplace_back(cat.roms[static_cast<size_t>(idx)].filename);
  }
  const int n = static_cast<int>(cat.roms.size());
  std::vector<RomPathMatch::CatalogHashEntry> normBuf(static_cast<size_t>(n));
  std::vector<RomPathMatch::CatalogHashEntry> baseBuf(static_cast<size_t>(n));
  RomPathMatch::CatalogPathLookup lookup;
  lookup.init(n, getPath, normBuf.data(), baseBuf.data());
  return measureUsPerIter(1, 5, [&](int) {
    int hits = 0;
    for (const auto& q : queries) {
      const int idx = lookup.findExact(q.c_str(), getPath);
      if (idx >= 0) hits += idx;
    }
    g_sink ^= static_cast<uint64_t>(hits);
  });
}

double benchBoxArtLegacyUnique1k(const SyntheticCatalog& cat) {
  const int n = 1000;
  auto getPath = [&](int idx) -> const char* { return cat.roms[static_cast<size_t>(idx)].filename; };
  std::vector<RomPathMatch::CatalogHashEntry> stemBuf(static_cast<size_t>(n));
  return measureUsPerIter(1, 3, [&](int) {
    RomPathMatch::StemBaseIndex stemIndex;
    stemIndex.init(n, getPath, stemBuf.data());
    int uniqueCount = 0;
    char baseA[64];
    for (int i = 0; i < n; ++i) {
      RomPathMatch::extractStemBaseName(cat.roms[static_cast<size_t>(i)].filename, baseA, sizeof(baseA));
      if (stemIndex.isUniqueForRom(cat.roms[static_cast<size_t>(i)].filename, baseA, getPath)) {
        uniqueCount++;
      }
    }
    g_sink ^= static_cast<uint64_t>(uniqueCount);
  });
}

double benchRomIndexV3Verify10k(const SyntheticCatalog& cat) {
  const uint32_t n = static_cast<uint32_t>(cat.roms.size());
  std::vector<BmoIndexEntry> entries(n);
  std::vector<uint8_t> strTable;
  strTable.reserve(n * 32U);
  for (uint32_t i = 0; i < n; ++i) {
    const char* s = cat.roms[i].filename;
    const size_t len = strlen(s);
    entries[i].pathOffset = static_cast<uint32_t>(strTable.size());
    entries[i].pathLength = static_cast<uint16_t>(len);
    entries[i].type = static_cast<uint8_t>(cat.roms[i].type);
    entries[i].flags = 0;
    entries[i].fileSize = cat.roms[i].fileSize;
    entries[i].crc32Prefix = cat.roms[i].crc32Prefix;
    entries[i].reserved = 0;
    strTable.insert(strTable.end(), reinterpret_cast<const uint8_t*>(s), reinterpret_cast<const uint8_t*>(s) + len + 1U);
  }
  const size_t entriesBytes = entries.size() * sizeof(BmoIndexEntry);
  return measureUsPerIter(2, 10, [&](int) {
    uint32_t totalCrc = 0;
    // Save computes payload CRC once before write and verifies the temp index payload once before atomic promotion.
    for (int pass = 0; pass < 2; ++pass) {
      uint32_t crc = AtomicFs::crc32Update(reinterpret_cast<const uint8_t*>(entries.data()), entriesBytes, 0xFFFFFFFFU);
      crc = AtomicFs::crc32Update(strTable.data(), strTable.size(), crc) ^ 0xFFFFFFFFU;
      totalCrc ^= crc;
    }
    g_sink ^= totalCrc;
  });
}

double benchNes8BitBlit60Frames() {
  static constexpr uint16_t kDummyPalette[64] = {
      0x2C63, 0x5101, 0x9410, 0x1438, 0x0F58, 0x0868, 0x2068, 0xE050, 0xA031, 0x400A, 0x8002, 0x6102, 0x0902,
      0x0000, 0x0000, 0x0000, 0x75AD, 0xFB12, 0x1F42, 0x3F71, 0xD9A0, 0xEFB0, 0x84B1, 0x609A, 0x606B, 0x203C,
      0x800C, 0x6604, 0xF103, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x9F65, 0x9F94, 0xBFC3, 0x5FF3, 0x79FB, 0x0EFC,
      0xE4EC, 0xE0BD, 0xC08E, 0x265F, 0x1047, 0x7B4E, 0x694A, 0x0000, 0x0000, 0xFFFF, 0x3FA7, 0xDFBD, 0xDFDD,
      0xDFFD, 0x38FD, 0x96F6, 0x15FF, 0xCFFE, 0xCFDF, 0xD7BF, 0xDBBF, 0xFF07, 0xDFFE, 0x0000, 0x0000,
  };
  std::vector<uint8_t> nesFb(256U * 240U, 0);
  for (size_t i = 0; i < nesFb.size(); ++i) {
    nesFb[i] = static_cast<uint8_t>(i & 0x3FU);
  }
  std::vector<uint16_t> outBuf(256U * 240U, 0);
  return measureUsPerIter(2, 10, [&](int) {
    for (int f = 0; f < 60; ++f) {
      for (int y = 0; y < 240; ++y) {
        const uint8_t* inRow = &nesFb[static_cast<size_t>(y) * 256U];
        uint32_t* out32 = reinterpret_cast<uint32_t*>(&outBuf[static_cast<size_t>(y) * 256U]);
        for (int x = 0; x < 256; x += 4) {
          const uint16_t p0 = kDummyPalette[inRow[x] & 0x3FU];
          const uint16_t p1 = kDummyPalette[inRow[x + 1] & 0x3FU];
          const uint16_t p2 = kDummyPalette[inRow[x + 2] & 0x3FU];
          const uint16_t p3 = kDummyPalette[inRow[x + 3] & 0x3FU];
          out32[(x >> 1)] = static_cast<uint32_t>(p0) | (static_cast<uint32_t>(p1) << 16);
          out32[(x >> 1) + 1] = static_cast<uint32_t>(p2) | (static_cast<uint32_t>(p3) << 16);
        }
      }
      g_sink ^= outBuf[static_cast<size_t>(f) * 17U];
    }
  });
}

double benchPicoNibble60Frames() {
  std::vector<uint8_t> picoFb(128U * 64U, 0x35U);
  uint8_t palMap[16];
  uint16_t pal[144];
  for (int i = 0; i < 16; ++i) palMap[i] = static_cast<uint8_t>(i);
  for (int i = 0; i < 144; ++i) pal[i] = static_cast<uint16_t>(i * 331U);
  std::vector<uint16_t> panelFb(128U * 128U, 0);
  return measureUsPerIter(2, 10, [&](int iter) {
    const uint8_t drawMode = (iter & 1) ? 129U : 0U;
    for (int f = 0; f < 60; ++f) {
      PicoBlit::renderFrame(picoFb.data(), palMap, pal, drawMode, panelFb.data());
      g_sink ^= panelFb[static_cast<size_t>(f) * 19U];
    }
  });
}

double benchProfilerSummarize100() {
  BmoProfilerCore::ProfilerState state;
  BmoProfilerCore::resetAll(state);
  for (uint32_t i = 0; i < 600U; ++i) {
    BmoProfilerCore::recordZoneElapsed(state, ZONE_EMU_FRAME, 14000U + (i % 500U));
  }
  uint32_t scratch[BmoProfilerCore::FRAME_RING_CAPACITY];
  BmoProfilerTypes::FrameSummary summary;
  return measureUsPerIter(5, 100, [&](int iter) {
    BmoProfilerCore::recordZoneElapsed(state, ZONE_EMU_FRAME, 14000U + static_cast<uint32_t>(iter % 300));
    BmoProfilerCore::summarizeFrames(state, summary, scratch, BmoProfilerCore::FRAME_RING_CAPACITY);
    g_sink ^= summary.p99Us;
  });
}

double benchScreenshotEncode320x240() {
  std::vector<uint16_t> fb(320U * 240U, 0xB9E7U);
  std::vector<uint8_t> rowRgb(320U * 3U, 0);
  return measureUsPerIter(2, 20, [&](int) {
    uint32_t acc = 0;
    for (int y = 239; y >= 0; --y) {
      const uint16_t* row = &fb[static_cast<size_t>(y) * 320U];
      for (int x = 0; x < 320; ++x) {
        const uint16_t swapped = row[x];
        const uint16_t bgr = static_cast<uint16_t>((swapped >> 8) | (swapped << 8));
        rowRgb[static_cast<size_t>(x) * 3U + 0U] = static_cast<uint8_t>((bgr >> 11) << 3);
        rowRgb[static_cast<size_t>(x) * 3U + 1U] = static_cast<uint8_t>(((bgr >> 5) & 0x3FU) << 2);
        rowRgb[static_cast<size_t>(x) * 3U + 2U] = static_cast<uint8_t>((bgr & 0x1FU) << 3);
      }
      acc += rowRgb[0] + rowRgb[959];
    }
    g_sink ^= acc;
  });
}

} // namespace

int main() {
  SyntheticCatalog catalog(10000);

  const double mFaceSnapshot = benchBmoFaceSdfSnapshot();
  const double mFaceFull = benchBmoFaceFullPipeline();
  const double mMetaResolve = benchMetadataResolve10k(catalog);
  const double mCatalogFind = benchCatalogFindByPath10k(catalog);
  const double mBoxArtUnique = benchBoxArtLegacyUnique1k(catalog);
  const double mIndexVerify = benchRomIndexV3Verify10k(catalog);
  const double mNesBlit = benchNes8BitBlit60Frames();
  const double mPicoNibble = benchPicoNibble60Frames();
  const double mProfiler = benchProfilerSummarize100();
  const double mScreenshot = benchScreenshotEncode320x240();

  printf("{\n");
  printf("  \"bmo_face_sdf_snapshot_320x240_us\": %.2f,\n", mFaceSnapshot);
  printf("  \"bmo_face_full_pipeline_320x240_us\": %.2f,\n", mFaceFull);
  printf("  \"metadata_resolve_100_on_10k_us\": %.2f,\n", mMetaResolve);
  printf("  \"catalog_find_500_on_10k_us\": %.2f,\n", mCatalogFind);
  printf("  \"boxart_legacy_unique_1k_us\": %.2f,\n", mBoxArtUnique);
  printf("  \"rom_index_v3_verify_10k_us\": %.2f,\n", mIndexVerify);
  printf("  \"nes_8bit_to_565_60frames_us\": %.2f,\n", mNesBlit);
  printf("  \"pico_128x128_nibble_60frames_us\": %.2f,\n", mPicoNibble);
  printf("  \"profiler_summarize_600_us\": %.2f,\n", mProfiler);
  printf("  \"screenshot_encode_320x240_us\": %.2f,\n", mScreenshot);
  printf("  \"sink\": %llu\n", static_cast<unsigned long long>(g_sink));
  printf("}\n");
  return 0;
}
