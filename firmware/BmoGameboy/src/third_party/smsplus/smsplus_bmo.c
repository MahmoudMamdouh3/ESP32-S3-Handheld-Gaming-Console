#include "smsplus_bmo.h"
#include "shared.h"
#include "../../hal/panel_color.h"

#include <string.h>

static uint8_t* s_videoBuf = NULL;
static uint8_t* s_dummyBuf = NULL;
static uint8_t* s_sramBuf = NULL;
static uint8_t* s_romCopy = NULL;
static uint8_t* s_colecoBios = NULL;
static uint16_t s_panelLut[256];
static bool s_lutReady = false;
static bool s_running = false;
static BmoSmsConsoleMode s_mode = BMO_SMS_CONSOLE_SMS;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t version;
  int afterEi;
  t_vdp vdpState;
  t_sms smsState;
  Z80_Regs z80State;
  uint8_t wram[0x2000];
  uint8_t vram[SMS_VDP_VRAM_SIZE];
  uint8_t sram[0x8000];
  uint8_t hasSram;
} BmoSmsSaveBlob;

#define BMO_SMS_STATE_MAGIC   0x534D5331u /* "SMS1" */
#define BMO_SMS_STATE_VERSION 1u

static void build_lut(void) {
  static const uint8_t c3[8] = {0x00, 0x24, 0x48, 0x6C, 0x90, 0xB4, 0xD8, 0xFF};
  static const uint8_t c2[4] = {0x00, 0x55, 0xAA, 0xFF};
  if (s_lutReady) return;
  for (int i = 0; i < 256; ++i) {
    uint8_t r = c3[(i >> 5) & 7];
    uint8_t g = c3[(i >> 2) & 7];
    uint8_t b = c2[i & 3];
    s_panelLut[i] = bmo_rgb888_to_panel565(r, g, b);
  }
  s_lutReady = true;
}

static void build_coleco_bios_trampoline(uint8_t* bios, const uint8_t* rom, size_t romSize) {
  memset(bios, 0xC9, 0x2000); /* RET */
  /* Z80 Reset vector at 0x0000: jump to cartridge entry if valid Coleco header (AA 55 / 55 AA). */
  uint16_t entry = 0x8000;
  uint16_t nmiVec = 0x0069; /* plain RETN fallback below */
  if (rom && romSize >= 0x24 &&
      ((rom[0] == 0xAA && rom[1] == 0x55) || (rom[0] == 0x55 && rom[1] == 0xAA))) {
    entry = (uint16_t)rom[0x0A] | ((uint16_t)rom[0x0B] << 8);
    if (entry < 0x8000) entry = 0x8000;
    /* Forward Coleco RST 08..30 & IRQ/NMI vectors if non-zero. */
    for (int r = 0; r < 6; ++r) {
      size_t off = (size_t)(0x08 + r * 8);
      size_t hdrOff = (size_t)(0x0C + r * 3);
      bios[off + 0] = 0xC3;
      bios[off + 1] = rom[hdrOff + 1];
      bios[off + 2] = rom[hdrOff + 2];
    }
    bios[0x0038] = 0xC3;
    bios[0x0039] = rom[0x1F];
    bios[0x003A] = rom[0x20];
    nmiVec = (uint16_t)rom[0x22] | ((uint16_t)rom[0x23] << 8);
  }
  /* 0x0000: DI; LD SP, 0x73B9; JP entry */
  bios[0x0000] = 0xF3;
  bios[0x0001] = 0x31;
  bios[0x0002] = 0xB9;
  bios[0x0003] = 0x73;
  bios[0x0004] = 0xC3;
  bios[0x0005] = (uint8_t)(entry & 0xFF);
  bios[0x0006] = (uint8_t)(entry >> 8);
  /* 0x0066: NMI vector -> JP nmiVec or RETN at 0x0069 */
  if (nmiVec >= 0x8000) {
    bios[0x0066] = 0xC3;
    bios[0x0067] = (uint8_t)(nmiVec & 0xFF);
    bios[0x0068] = (uint8_t)(nmiVec >> 8);
  } else {
    bios[0x0066] = 0xED;
    bios[0x0067] = 0x45; /* RETN */
  }
  bios[0x0069] = 0xED;
  bios[0x006A] = 0x45;
}

bool bmo_sms_init(const uint8_t* romData, size_t romSize,
                  BmoSmsConsoleMode mode, const uint8_t* colecoBios8k) {
  bmo_sms_destroy();
  if (!romData || romSize == 0) return false;

  build_lut();
  s_mode = mode;

  const size_t minPad = (mode == BMO_SMS_CONSOLE_COLECO) ? 0x8000u : 0x8000u;
  const uint8_t* activeRom = romData;
  if (romSize < minPad) {
    s_romCopy = (uint8_t*)bmo_psram_calloc(1, minPad);
    if (!s_romCopy) {
      bmo_sms_destroy();
      return false;
    }
    memcpy(s_romCopy, romData, romSize);
    activeRom = s_romCopy;
    romSize = minPad;
  }

  s_videoBuf = (uint8_t*)bmo_psram_calloc(1, 256u * 240u);
  s_dummyBuf = (uint8_t*)bmo_psram_calloc(1, 0x2000u);
  const bool needsSram = (mode == BMO_SMS_CONSOLE_SMS || mode == BMO_SMS_CONSOLE_GG);
  if (needsSram) {
    s_sramBuf = (uint8_t*)bmo_psram_malloc(0x8000u);
    if (s_sramBuf) memset(s_sramBuf, 0xFF, 0x8000u);
  }
  if (!s_videoBuf || !s_dummyBuf || (needsSram && !s_sramBuf)) {
    bmo_sms_destroy();
    return false;
  }

  if (mode == BMO_SMS_CONSOLE_COLECO) {
    s_colecoBios = (uint8_t*)bmo_psram_malloc(0x2000u);
    if (!s_colecoBios) {
      bmo_sms_destroy();
      return false;
    }
    if (colecoBios8k) {
      memcpy(s_colecoBios, colecoBios8k, 0x2000u);
    } else {
      build_coleco_bios_trampoline(s_colecoBios, activeRom, romSize);
    }
    sms.coleco_bios = s_colecoBios;
  } else {
    sms.coleco_bios = NULL;
  }

  sms.dummy = s_dummyBuf;
  sms.sram = s_sramBuf;

  memset(&bitmap, 0, sizeof(bitmap));
  bitmap.width = 256;
  bitmap.height = 192;
  bitmap.pitch = 256;
  bitmap.depth = 8;
  bitmap.data = s_videoBuf + 24u * 256u;

  cart.rom = (uint8*)activeRom;
  if (mode == BMO_SMS_CONSOLE_COLECO) {
    cart.pages = (uint8)((romSize + 0x1FFFu) / 0x2000u);
    cart.type = TYPE_COLECO;
  } else if (mode == BMO_SMS_CONSOLE_SG1000) {
    cart.pages = (uint8)((romSize + 0x3FFFu) / 0x4000u);
    cart.type = TYPE_SG1000;
  } else if (mode == BMO_SMS_CONSOLE_GG) {
    cart.pages = (uint8)((romSize + 0x3FFFu) / 0x4000u);
    cart.type = TYPE_GG;
  } else {
    cart.pages = (uint8)((romSize + 0x3FFFu) / 0x4000u);
    cart.type = TYPE_SMS;
  }
  if (cart.pages == 0) cart.pages = 1;

  render_set_console_type(cart.type);

  if (!z80_allocate_flag_tables() || !sms_init_ram() || !vdp_init_vram()) {
    bmo_sms_destroy();
    return false;
  }

  emu_system_init(0); /* sound disabled on hardware (FEATURE_AUDIO=0) */
  system_reset();
  s_running = true;
  return true;
}

void bmo_sms_set_input(int pad0, int systemFlags) {
  input.pad[0] = pad0;
  input.pad[1] = 0;
  input.system = systemFlags;
}

int bmo_sms_input_pad0(void) {
  return s_running ? input.pad[0] : 0;
}

int bmo_sms_input_system(void) {
  return s_running ? input.system : 0;
}

void bmo_sms_run_frame(int skipRender) {
  if (!s_running) return;
  sms_frame(skipRender);
}

void bmo_sms_destroy(void) {
  if (s_running) {
    system_shutdown();
    s_running = false;
  } else {
    render_shutdown();
    tms_shutdown();
    vdp_shutdown_vram();
    sms_shutdown_ram();
    z80_exit();
  }
  bmo_psram_free(s_colecoBios); s_colecoBios = NULL;
  bmo_psram_free(s_sramBuf);    s_sramBuf = NULL;
  bmo_psram_free(s_dummyBuf);   s_dummyBuf = NULL;
  bmo_psram_free(s_videoBuf);   s_videoBuf = NULL;
  bmo_psram_free(s_romCopy);    s_romCopy = NULL;
  sms.sram = NULL;
  sms.dummy = NULL;
  sms.coleco_bios = NULL;
  bitmap.data = NULL;
  cart.rom = NULL;
}

bool bmo_sms_is_running(void) {
  return s_running;
}

const uint8_t* bmo_sms_framebuffer_rgb332(void) {
  return bitmap.data;
}

uint16_t bmo_sms_rgb332_to_rgb565(uint8_t idx) {
  static const uint8_t c3[8] = {0x00, 0x24, 0x48, 0x6C, 0x90, 0xB4, 0xD8, 0xFF};
  static const uint8_t c2[4] = {0x00, 0x55, 0xAA, 0xFF};
  uint16_t r = c3[(idx >> 5) & 7];
  uint16_t g = c3[(idx >> 2) & 7];
  uint16_t b = c2[idx & 3];
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void bmo_sms_render_panel565(uint16_t* dst256x192) {
  if (!dst256x192 || !bitmap.data) return;
  build_lut();
  const uint8_t* src = bitmap.data;
  const int count = 256 * 192;
  for (int i = 0; i < count; ++i) {
    dst256x192[i] = s_panelLut[src[i]];
  }
}

uint8_t* bmo_sms_wram_data(size_t* outSize) {
  if (!s_running || !sms.ram) {
    if (outSize) *outSize = 0u;
    return NULL;
  }
  if (outSize) *outSize = 0x2000u;
  return sms.ram;
}

uint8_t* bmo_sms_sram_data(size_t* outSize) {
  if (outSize) *outSize = s_sramBuf ? 0x8000u : 0u;
  return s_sramBuf;
}

bool bmo_sms_sram_used(void) {
  if (!s_sramBuf || !sms.save) return false;
  for (size_t i = 0; i < 0x8000u; ++i) {
    if (s_sramBuf[i] != 0xFFu && s_sramBuf[i] != 0x00u) return true;
  }
  return false;
}

size_t bmo_sms_state_size(void) {
  return sizeof(BmoSmsSaveBlob);
}

bool bmo_sms_save_state(uint8_t* dst, size_t cap, size_t* outSize) {
  if (!s_running || !dst || cap < sizeof(BmoSmsSaveBlob) || !sms.ram || !vdp.vram) {
    return false;
  }
  BmoSmsSaveBlob* blob = (BmoSmsSaveBlob*)dst;
  memset(blob, 0, sizeof(*blob));
  blob->magic = BMO_SMS_STATE_MAGIC;
  blob->version = BMO_SMS_STATE_VERSION;
  blob->afterEi = after_EI;
  blob->vdpState = vdp;
  blob->smsState = sms;
  z80_get_context(&blob->z80State);
  memcpy(blob->wram, sms.ram, 0x2000u);
  memcpy(blob->vram, vdp.vram, SMS_VDP_VRAM_SIZE);
  if (s_sramBuf) {
    blob->hasSram = 1;
    memcpy(blob->sram, s_sramBuf, 0x8000u);
  }
  if (outSize) *outSize = sizeof(BmoSmsSaveBlob);
  return true;
}

bool bmo_sms_load_state(const uint8_t* src, size_t len) {
  if (!s_running || !src || len < sizeof(BmoSmsSaveBlob) || !sms.ram || !vdp.vram) {
    return false;
  }
  const BmoSmsSaveBlob* blob = (const BmoSmsSaveBlob*)src;
  if (blob->magic != BMO_SMS_STATE_MAGIC || blob->version != BMO_SMS_STATE_VERSION) {
    return false;
  }

  uint8_t* savedVram = vdp.vram;
  uint8_t* savedWram = sms.ram;
  uint8_t* savedDummy = sms.dummy;
  uint8_t* savedSram = sms.sram;
  uint8_t* savedBios = sms.coleco_bios;

  cpu_reset();
  system_reset();

  vdp = blob->vdpState;
  vdp.vram = savedVram;
  memcpy(vdp.vram, blob->vram, SMS_VDP_VRAM_SIZE);

  sms = blob->smsState;
  sms.ram = savedWram;
  sms.dummy = savedDummy;
  sms.sram = savedSram;
  sms.coleco_bios = savedBios;
  memcpy(sms.ram, blob->wram, 0x2000u);
  if (blob->hasSram && s_sramBuf) {
    memcpy(s_sramBuf, blob->sram, 0x8000u);
  }

  Z80_Regs z80Copy = blob->z80State;
  z80_set_context(&z80Copy);
  after_EI = blob->afterEi;
  z80_set_irq_callback(sms_irq_callback);

  /* Restore memory map for the active console type. */
  if (cart.type == TYPE_SMS || cart.type == TYPE_GG) {
    cpu_readmap[0] = cart.rom + 0x0000;
    cpu_readmap[1] = cart.rom + 0x2000;
    cpu_readmap[2] = cart.rom + 0x4000;
    cpu_readmap[3] = cart.rom + 0x6000;
    cpu_readmap[4] = cart.rom + 0x0000;
    cpu_readmap[5] = cart.rom + 0x2000;
    cpu_readmap[6] = sms.ram;
    cpu_readmap[7] = sms.ram;

    cpu_writemap[0] = sms.dummy;
    cpu_writemap[1] = sms.dummy;
    cpu_writemap[2] = sms.dummy;
    cpu_writemap[3] = sms.dummy;
    cpu_writemap[4] = sms.dummy;
    cpu_writemap[5] = sms.dummy;
    cpu_writemap[6] = sms.ram;
    cpu_writemap[7] = sms.ram;

    sms_mapper_w(3, sms.fcr[3]);
    sms_mapper_w(2, sms.fcr[2]);
    sms_mapper_w(1, sms.fcr[1]);
    sms_mapper_w(0, sms.fcr[0]);
  } else if (cart.type == TYPE_COLECO) {
    int p = cart.pages ? cart.pages : 1;
    cpu_readmap[0] = sms.coleco_bios ? sms.coleco_bios : sms.dummy;
    cpu_readmap[1] = sms.dummy;
    cpu_readmap[2] = sms.dummy;
    cpu_readmap[3] = sms.ram;
    cpu_readmap[4] = cart.rom + ((0 % p) << 13);
    cpu_readmap[5] = cart.rom + ((1 % p) << 13);
    cpu_readmap[6] = cart.rom + ((2 % p) << 13);
    cpu_readmap[7] = cart.rom + ((3 % p) << 13);

    cpu_writemap[0] = sms.dummy;
    cpu_writemap[1] = sms.dummy;
    cpu_writemap[2] = sms.dummy;
    cpu_writemap[3] = sms.ram;
    cpu_writemap[4] = sms.dummy;
    cpu_writemap[5] = sms.dummy;
    cpu_writemap[6] = sms.dummy;
    cpu_writemap[7] = sms.dummy;
  } else {
    cpu_readmap[0] = cart.rom + 0x0000;
    cpu_readmap[1] = cart.rom + 0x2000;
    cpu_readmap[2] = cart.rom + 0x4000;
    cpu_readmap[3] = cart.rom + 0x6000;
    cpu_readmap[4] = sms.dummy;
    cpu_readmap[5] = sms.dummy;
    cpu_readmap[6] = sms.ram;
    cpu_readmap[7] = sms.ram;
    cpu_writemap[0] = sms.dummy;
    cpu_writemap[1] = sms.dummy;
    cpu_writemap[2] = sms.dummy;
    cpu_writemap[3] = sms.dummy;
    cpu_writemap[4] = sms.dummy;
    cpu_writemap[5] = sms.dummy;
    cpu_writemap[6] = sms.ram;
    cpu_writemap[7] = sms.ram;
  }

  for (int i = 0; i < PALETTE_SIZE; ++i) {
    palette_sync(i);
  }
  for (int i = 0; i < 512; ++i) {
    vramMarkTileDirty(i);
  }
  (void)s_mode;
  return true;
}
