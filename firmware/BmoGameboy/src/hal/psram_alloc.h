#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>

static inline void* bmo_psram_malloc(size_t size) {
  if (size == 0) size = 1;
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) {
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
  return ptr;
}

static inline void* bmo_psram_calloc(size_t nmemb, size_t size) {
  const size_t total = nmemb * size;
  void* ptr = bmo_psram_malloc(total);
  if (ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static inline void* bmo_psram_realloc(void* ptr, size_t size) {
  if (!ptr) return bmo_psram_malloc(size);
  if (size == 0) {
    heap_caps_free(ptr);
    return NULL;
  }
  void* resized = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!resized) {
    resized = heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
  }
  return resized;
}

static inline void* bmo_internal_aligned_calloc(size_t align, size_t size) {
  if (size == 0) size = 1;
  void* ptr = heap_caps_aligned_alloc(align, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ptr) {
    ptr = heap_caps_aligned_alloc(align, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void bmo_psram_free(void* ptr) {
  if (ptr) {
    heap_caps_free(ptr);
  }
}

#else

static inline void* bmo_psram_malloc(size_t size) {
  return malloc(size ? size : 1);
}

static inline void* bmo_psram_calloc(size_t nmemb, size_t size) {
  return calloc(nmemb ? nmemb : 1, size ? size : 1);
}

static inline void* bmo_psram_realloc(void* ptr, size_t size) {
  return realloc(ptr, size ? size : 1);
}

static inline void* bmo_internal_aligned_calloc(size_t align, size_t size) {
  (void)align;
  return calloc(1, size ? size : 1);
}

static inline void bmo_psram_free(void* ptr) {
  free(ptr);
}

#endif

#ifdef __cplusplus
}
#endif
