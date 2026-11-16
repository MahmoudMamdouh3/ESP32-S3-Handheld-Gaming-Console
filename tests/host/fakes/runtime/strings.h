#pragma once
#include <string.h>

#if defined(_MSC_VER)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#else
#include_next <strings.h>
#endif
