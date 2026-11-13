#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SaveState SaveState;

SaveState* saveGwenesisStateOpenForRead(const char* name);
SaveState* saveGwenesisStateOpenForWrite(const char* name);
int saveGwenesisStateGet(SaveState* state, const char* name);
void saveGwenesisStateSet(SaveState* state, const char* name, int value);
void saveGwenesisStateGetBuffer(SaveState* state, const char* name, void* dest, size_t size);
void saveGwenesisStateSetBuffer(SaveState* state, const char* name, const void* src, size_t size);

bool saveGwenesisStateSaveToBuffer(void* dst, size_t maxBytes, size_t* outBytes);
bool saveGwenesisStateLoadFromBuffer(const void* src, size_t bytes);
void saveGwenesisStateResetArchive(void);

#ifdef __cplusplus
}
#endif
