#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

#if defined(ESP32) || defined(ARDUINO)
    extern void* Doom_MallocPSRAM(size_t size);
	DG_ScreenBuffer = (pixel_t*)Doom_MallocPSRAM(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#else
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif

	DG_Init();

	D_DoomMain ();
}

