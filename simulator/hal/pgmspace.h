#pragma once
// pgmspace.h - flash-resident data is ordinary memory on this build.
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef PROGMEM
#define PROGMEM
#endif
#define PGM_P const char *
#define PGM_VOID_P const void *
#define PSTR(s) (s)

#ifdef __cplusplus
class __FlashStringHelper;
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#define F(string_literal) (FPSTR(PSTR(string_literal)))
#endif

#define pgm_read_byte(addr)    (*reinterpret_cast<const uint8_t *>(addr))
#define pgm_read_word(addr)    (*reinterpret_cast<const uint16_t *>(addr))
#define pgm_read_dword(addr)   (*reinterpret_cast<const uint32_t *>(addr))
#define pgm_read_float(addr)   (*reinterpret_cast<const float *>(addr))
#define pgm_read_ptr(addr)     (*reinterpret_cast<void *const *>(addr))
#define pgm_read_pointer(addr) pgm_read_ptr(addr)

#define memcmp_P   memcmp
#define memccpy_P  memccpy
#define memmem_P   memmem
#define memcpy_P   memcpy
#define strcpy_P   strcpy
#define strncpy_P  strncpy
#define strcat_P   strcat
#define strncat_P  strncat
#define strcmp_P   strcmp
#define strncmp_P  strncmp
#define strcasecmp_P  strcasecmp
#define strncasecmp_P strncasecmp
#define strlen_P   strlen
#define strnlen_P  strnlen
#define strstr_P   strstr
#define printf_P   printf
#define sprintf_P  sprintf
#define snprintf_P snprintf
#define vsnprintf_P vsnprintf
