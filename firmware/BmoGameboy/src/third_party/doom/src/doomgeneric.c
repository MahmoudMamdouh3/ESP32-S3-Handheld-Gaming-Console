#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);
extern void W_Shutdown(void);
extern void I_ShutdownZone(void);
void doomgeneric_Destroy(void);


void doomgeneric_Destroy(void)
{
    W_Shutdown();
    I_ShutdownZone();
    if (DG_ScreenBuffer != NULL)
    {
#if defined(ESP32) || defined(ARDUINO)
        extern void Doom_FreePSRAM(void* ptr);
        Doom_FreePSRAM(DG_ScreenBuffer);
#else
        free(DG_ScreenBuffer);
#endif
        DG_ScreenBuffer = NULL;
    }
}

void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();
    doomgeneric_Destroy();

#if defined(ESP32) || defined(ARDUINO)
    extern void* Doom_MallocPSRAM(size_t size);
	DG_ScreenBuffer = (pixel_t*)Doom_MallocPSRAM(
        DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(*DG_ScreenBuffer));
#else
	DG_ScreenBuffer = malloc(
        DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(*DG_ScreenBuffer));
#endif

	DG_Init();

	D_DoomMain ();
}
