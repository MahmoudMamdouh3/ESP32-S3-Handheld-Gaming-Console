#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MALLOC_CAP_EXEC (1U << 0)
#define MALLOC_CAP_32BIT (1U << 1)
#define MALLOC_CAP_8BIT (1U << 2)
#define MALLOC_CAP_DMA (1U << 3)
#define MALLOC_CAP_SPIRAM (1U << 10)
#define MALLOC_CAP_INTERNAL (1U << 11)
#define MALLOC_CAP_DEFAULT (1U << 12)
#define MALLOC_CAP_IRAM_8BIT (1U << 13)

inline void* heap_caps_malloc(size_t size, uint32_t) {
  return malloc(size);
}
inline void* heap_caps_calloc(size_t n, size_t size, uint32_t) {
  return calloc(n, size);
}
inline void* heap_caps_aligned_alloc(size_t, size_t size, uint32_t) {
  return calloc(1U, size ? size : 1U);
}
inline void* heap_caps_realloc(void* ptr, size_t size, uint32_t) {
  return realloc(ptr, size);
}
inline void heap_caps_free(void* ptr) {
  free(ptr);
}
inline size_t heap_caps_get_free_size(uint32_t) {
  return 4U * 1024U * 1024U;
}
inline size_t heap_caps_get_largest_free_block(uint32_t) {
  return 4U * 1024U * 1024U;
}
