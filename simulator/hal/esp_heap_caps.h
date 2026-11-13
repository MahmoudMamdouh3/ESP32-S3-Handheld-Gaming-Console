#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Capability flags mirror ESP-IDF; every region is plain linear memory in the
// WebAssembly build. Reported free sizes model the ESP32-S3-N16R8 so firmware
// diagnostics stay meaningful.
#define MALLOC_CAP_EXEC          (1 << 0)
#define MALLOC_CAP_32BIT         (1 << 1)
#define MALLOC_CAP_8BIT          (1 << 2)
#define MALLOC_CAP_DMA           (1 << 3)
#define MALLOC_CAP_SPIRAM        (1 << 10)
#define MALLOC_CAP_INTERNAL      (1 << 11)
#define MALLOC_CAP_DEFAULT       (1 << 12)
#define MALLOC_CAP_IRAM_8BIT     (1 << 13)

#ifdef __cplusplus
extern "C" {
#endif

void* heap_caps_malloc(size_t size, uint32_t caps);
void* heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void* heap_caps_realloc(void* ptr, size_t size, uint32_t caps);
void heap_caps_free(void* ptr);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);

#ifdef __cplusplus
}
#endif
