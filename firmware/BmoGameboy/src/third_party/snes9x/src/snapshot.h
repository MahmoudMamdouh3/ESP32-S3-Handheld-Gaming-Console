#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool S9xSaveState(const char *filename);
bool S9xLoadState(const char *filename);
size_t S9xBmoSerializeSize(void);
bool S9xBmoSerialize(void *dst, size_t maxBytes, size_t *outBytes);
bool S9xBmoUnserialize(const void *src, size_t bytes);

#ifdef __cplusplus
}
#endif
