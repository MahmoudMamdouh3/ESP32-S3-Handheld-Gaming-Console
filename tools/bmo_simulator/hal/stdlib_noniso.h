#pragma once
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char* itoa(int value, char* result, int base);
char* ltoa(long value, char* result, int base);
char* utoa(unsigned int value, char* result, int base);
char* ultoa(unsigned long value, char* result, int base);
char* lltoa(long long value, char* result, int base);
char* ulltoa(unsigned long long value, char* result, int base);
char* dtostrf(double value, signed char width, unsigned char prec, char* sout);
char* strlwr(char* s);
char* strupr(char* s);

#ifdef __cplusplus
}
#endif
