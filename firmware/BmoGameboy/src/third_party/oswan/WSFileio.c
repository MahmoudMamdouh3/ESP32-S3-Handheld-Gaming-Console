static int g_ws_rom_xip = 0;
/*
$Date: 2009-10-30 05:26:46 +0100 (ven., 30 oct. 2009) $
$Rev: 71 $
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WSHard.h"
#include "WS.h"
#include "WSFileio.h"
#include "WSRender.h"
#include "cpu/necintrf.h"
#include "../../hal/psram_alloc.h"

#define ERR_MALLOC					0
#define ERR_OVER_RAMSIZE			0
#define ERR_WRITE_ROM				0
#define ERR_WAVEOUT_OPEN			0
#define ERR_WAVEOUT_PREPAREHEADER	0
#define ERR_FOPEN					0
#define ERR_FREAD_ROMINFO			0
#define ERR_ILLEGAL_ROMSIZE			0
#define ERR_FREAD_ROM				0
#define ERR_CHECKSUM				0
#define ERR_FREAD_SAVE				0

static char *SaveDir = "RAM";
static char *StateDir = "STATE";
static char SaveName[1];   // unused
static char StateName[1];
static char* IEepPath = "./oswan-od.dat";
static BYTE g_dummy_ff = 0xFF;

#if 0
void WsSetDir(char* path)
{
	char temp[512];
	char file[512];
	char* p;

	SaveName[0] = 0;
	StateName[0] = 0;
	if (path == NULL)
	{
		return;
	}
	strcpy(temp, path);
	p = strrchr(temp, '\\');
	if (p == NULL)
	{
		return;
	}
	strcpy(file, p++);
	*p = 0;
	p = strrchr(file, '.');
	if (p == NULL)
	{
		return;
	}
	*p = 0;
	strcpy(SaveName, temp);
	strcat(SaveName, SaveDir);
	strcat(SaveName, file);
	strcat(SaveName, ".sav");
	strcpy(StateName, temp);
	strcat(StateName, StateDir);
	strcat(StateName, file);
}
#endif

int WsSetPdata(void)
{
    ROMBanks = 1;
    RAMBanks = 0;
    RAMSize = 0;
    CartKind = 0;
    if ((ROMMap[0xFF] = (BYTE*)bmo_psram_malloc(0x10000)) == NULL)
    {
		fprintf(stderr,"WsSetPdata\n");

        //ErrorMsg(ERR_MALLOC);
        return 0;
    }
    WsCpyPdata(ROMMap[0xFF] + 0xF080);
    WsReset();
    SetHVMode(0);
    return 1;
}


int WsCreate(char *CartName)
{
    (void)CartName;
    return -1; /* File loading disabled; use WsCreateFromMemory */
}

void WsRelease(void)
{
    int i;
    if (ROMMap)
    {
        for (i = 0; i < 0x100; i++)
        {
            if (ROMMap[i] && ROMMap[i] != MemDummy)
            {
                if (!g_ws_rom_xip)
                    bmo_psram_free(ROMMap[i]);
            }
            ROMMap[i] = NULL;
        }
    }
    if (RAMMap)
    {
        for (i = 0; i < 0x100; i++)
        {
            BYTE* ptr = RAMMap[i];
            if (ptr && ptr != MemDummy)
            {
                bmo_psram_free(ptr);
                for (int j = i; j < 0x100; j++)
                {
                    if (RAMMap[j] == ptr)
                        RAMMap[j] = NULL;
                }
            }
            else
            {
                RAMMap[i] = NULL;
            }
        }
    }
    g_ws_rom_xip = 0;
    if (IRAM)        { bmo_psram_free(IRAM);        IRAM = NULL; }
    if (IO)          { bmo_psram_free(IO);          IO = NULL; }
    if (ROMMap)      { bmo_psram_free(ROMMap);      ROMMap = NULL; }
    if (RAMMap)      { bmo_psram_free(RAMMap);      RAMMap = NULL; }
    if (Palette)     { bmo_psram_free(Palette);     Palette = NULL; }
    if (SprTMap)     { bmo_psram_free(SprTMap);     SprTMap = NULL; }
    if (FrameBuffer) { bmo_psram_free(FrameBuffer); FrameBuffer = NULL; }
}

void WsSaveEep(void) {}
void WsLoadEep(void) {}

void WsLoadIEep(void)
{
    /* File-based IEep skipped on embedded targets; use default internal EEPROM table */
	{
		WORD* p = IEep + 0x30;
		memset(IEep, 0xFF, 0x60);
		memset(p, 0, 0x20);
		*p++ = 0x211D;
		*p++ = 0x180B;
		*p++ = 0x1C0D;
		*p++ = 0x1D23;
		*p++ = 0x0B1E;
		*p   = 0x0016;
	}
}



void WsSaveIEep(void) {}

size_t WsStateGetSize(void)
{
    size_t ramBytes = 0;
    for (int i = 0; i < RAMBanks; i++)
        ramBytes += (RAMSize < 0x10000) ? (size_t)RAMSize : 0x10000u;
    return 8u + 18u * sizeof(unsigned int) + 0x10000u + 0x100u + ramBytes + (16u * 16u * sizeof(WORD)) + sizeof(sCEep);
}

static const unsigned char kWsStateMagic[8] = { 'B', 'M', 'O', 'W', 'S', '0', '0', '1' };

int WsSaveStateToMem(void *dst, size_t maxBytes, size_t *outBytes)
{
    size_t need = WsStateGetSize();
    if (outBytes) *outBytes = need;
    if (!dst || maxBytes < need || !IRAM || !IO || !Palette) return 0;
    unsigned char *w = (unsigned char *)dst;
    memcpy(w, kWsStateMagic, sizeof(kWsStateMagic)); w += sizeof(kWsStateMagic);
    const int regs[18] = {
        NEC_IP, NEC_AW, NEC_BW, NEC_CW, NEC_DW, NEC_CS, NEC_DS, NEC_ES, NEC_SS,
        NEC_IX, NEC_IY, NEC_BP, NEC_SP, NEC_FLAGS, NEC_VECTOR, NEC_PENDING,
        NEC_NMI_STATE, NEC_IRQ_STATE
    };
    for (int r = 0; r < 18; r++) {
        unsigned int v = nec_get_reg(regs[r]);
        memcpy(w, &v, sizeof(unsigned int));
        w += sizeof(unsigned int);
    }
    memcpy(w, IRAM, 0x10000u); w += 0x10000u;
    memcpy(w, IO, 0x100u);     w += 0x100u;
    for (int i = 0; i < RAMBanks; i++) {
        size_t chunk = (RAMSize < 0x10000) ? (size_t)RAMSize : 0x10000u;
        if (RAMMap && RAMMap[i]) memcpy(w, RAMMap[i], chunk);
        else memset(w, 0, chunk);
        w += chunk;
    }
    memcpy(w, Palette, 16u * 16u * sizeof(WORD)); w += 16u * 16u * sizeof(WORD);
    memcpy(w, &sCEep, sizeof(sCEep));             w += sizeof(sCEep);
    return 1;
}

int WsLoadStateFromMem(const void *src, size_t bytes)
{
    size_t need = WsStateGetSize();
    if (!src || bytes < need || !IRAM || !IO || !Palette) return 0;
    const unsigned char *rptr = (const unsigned char *)src;
    if (memcmp(rptr, kWsStateMagic, sizeof(kWsStateMagic)) != 0) return 0;
    rptr += sizeof(kWsStateMagic);
    const int regs[18] = {
        NEC_IP, NEC_AW, NEC_BW, NEC_CW, NEC_DW, NEC_CS, NEC_DS, NEC_ES, NEC_SS,
        NEC_IX, NEC_IY, NEC_BP, NEC_SP, NEC_FLAGS, NEC_VECTOR, NEC_PENDING,
        NEC_NMI_STATE, NEC_IRQ_STATE
    };
    for (int r = 0; r < 18; r++) {
        unsigned int v = 0;
        memcpy(&v, rptr, sizeof(unsigned int));
        rptr += sizeof(unsigned int);
        nec_set_reg(regs[r], v);
    }
    memcpy(IRAM, rptr, 0x10000u); rptr += 0x10000u;
    memcpy(IO, rptr, 0x100u);     rptr += 0x100u;
    for (int i = 0; i < RAMBanks; i++) {
        size_t chunk = (RAMSize < 0x10000) ? (size_t)RAMSize : 0x10000u;
        if (RAMMap && RAMMap[i]) memcpy(RAMMap[i], rptr, chunk);
        rptr += chunk;
    }
    memcpy(Palette, rptr, 16u * 16u * sizeof(WORD)); rptr += 16u * 16u * sizeof(WORD);
    memcpy(&sCEep, rptr, sizeof(sCEep));             rptr += sizeof(sCEep);
    sCEep.data = ((CartKind & CK_EEP) && RAMMap) ? (WORD*)RAMMap[0x00] : NULL;
    WriteIO(0xC1, IO[0xC1]);
    WriteIO(0xC2, IO[0xC2]);
    WriteIO(0xC3, IO[0xC3]);
    WriteIO(0xC0, IO[0xC0]);
    for (int i = 0x80; i <= 0x90; i++) WriteIO(i, IO[i]);
    return 1;
}

void WsLoadState(int num) { (void)num; }
void WsSaveState(int num) { (void)num; }

int WsCreateFromMemory(const BYTE *romData, size_t romSize)
{
    int i, j;
    BYTE buf[16];
    int Checksum;

    if (!romData || romSize < 16) return ERR_FOPEN;
    for (i = 0; i < 0x100; i++) {
        ROMMap[i] = MemDummy;
        RAMMap[i] = MemDummy;
    }
    g_ws_rom_xip = 1;
    memcpy(buf, romData + romSize - 10, 10);

    switch (buf[4]) {
    case 1: ROMBanks = 4;   break;
    case 2: ROMBanks = 8;   break;
    case 3: ROMBanks = 16;  break;
    case 4: ROMBanks = 32;  break;
    case 5: ROMBanks = 48;  break;
    case 6: ROMBanks = 64;  break;
    case 7: ROMBanks = 96;  break;
    case 8: ROMBanks = 128; break;
    case 9: ROMBanks = 256; break;
    default: ROMBanks = (int)(romSize / 0x10000u); break;
    }
    if (ROMBanks <= 0) return ERR_ILLEGAL_ROMSIZE;
    if ((size_t)ROMBanks * 0x10000u > romSize) {
        ROMBanks = (int)(romSize / 0x10000u);
        if (ROMBanks <= 0) return ERR_ILLEGAL_ROMSIZE;
    }

    switch (buf[5]) {
    case 0x01: RAMBanks = 1; RAMSize = 0x2000;  CartKind = 0;      break;
    case 0x02: RAMBanks = 1; RAMSize = 0x8000;  CartKind = 0;      break;
    case 0x03: RAMBanks = 2; RAMSize = 0x20000; CartKind = 0;      break;
    case 0x04: RAMBanks = 4; RAMSize = 0x40000; CartKind = 0;      break;
    case 0x10: RAMBanks = 1; RAMSize = 0x80;    CartKind = CK_EEP; break;
    case 0x20: RAMBanks = 1; RAMSize = 0x800;   CartKind = CK_EEP; break;
    case 0x50: RAMBanks = 1; RAMSize = 0x400;   CartKind = CK_EEP; break;
    default:   RAMBanks = 0; RAMSize = 0;       CartKind = 0;      break;
    }

    WsRomPatch(buf);
    Checksum = (int)((buf[9] << 8) + buf[8]);
    Checksum += (int)(buf[9] + buf[8]);
    for (i = ROMBanks - 1; i >= 0; i--) {
        size_t off = romSize - (size_t)(ROMBanks - i) * 0x10000u;
        ROMMap[0x100 - ROMBanks + i] = (BYTE *)(romData + off);
        for (j = 0; j < 0x10000; j++) Checksum -= ROMMap[0x100 - ROMBanks + i][j];
    }
    (void)Checksum;

    for (i = 0; i < RAMBanks; i++) {
        RAMMap[i] = (BYTE *)bmo_psram_malloc(0x10000u);
        if (!RAMMap[i]) return ERR_MALLOC;
        memset(RAMMap[i], 0x00, 0x10000u);
    }

    WsLoadIEep();
    return 0;
}
