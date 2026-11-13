/*---------------------------------------------------------------------------
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version. See also the license.txt file for
 *	additional informations.
 *---------------------------------------------------------------------------
 */

#ifndef STDAFX_H
#define STDAFX_H

#include <stdint.h>

#ifndef USE_CZ80
#define USE_CZ80 1
#endif
#ifndef CZ80
#define CZ80 1
#endif
#ifndef NGP_ONLY_RENDER_VISIBLE_LINES
#define NGP_ONLY_RENDER_VISIBLE_LINES 1
#endif
#ifndef NGP_OPTIMIZATION_16BIT_READ
#define NGP_OPTIMIZATION_16BIT_READ 1
#endif
#ifndef DISABLE_WATCHDOGS
#define DISABLE_WATCHDOGS 1
#endif


#define DWORD unsigned int

#define _u8   unsigned char
#define Uint8 unsigned char
#define _u16  unsigned short
#define _u32  unsigned int

#define BOOL  int
#define FALSE 0
#define TRUE  (!0)

#ifdef _WIN32
#define path_default_slash() "\\"
#define path_default_slash_c() '\\'
#else
#define path_default_slash() "/"
#define path_default_slash_c() '/'
#endif

#endif /* STDAFX_H */
