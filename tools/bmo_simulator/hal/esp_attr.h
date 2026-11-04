#pragma once
#define IRAM_ATTR
#define DRAM_ATTR
#define EXT_RAM_ATTR
#define RTC_DATA_ATTR
#define WORD_ALIGNED_ATTR
#define DMA_ATTR
#define NOINLINE_ATTR __attribute__((noinline))
#define FORCE_INLINE_ATTR static inline __attribute__((always_inline))
