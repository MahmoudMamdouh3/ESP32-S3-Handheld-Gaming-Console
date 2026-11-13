#include "gwenesis_savestate.h"
#include <string.h>
#include "../../../hal/psram_alloc.h"

#include "../bus/gwenesis_bus.h"
#include "../cpus/M68K/m68k.h"
#include "../cpus/Z80/Z80.h"
#include "../sound/z80inst.h"
#include "../sound/ym2612.h"
#include "../sound/gwenesis_sn76489.h"
#include "../vdp/gwenesis_vdp.h"
#include "../io/gwenesis_io.h"

/* In-memory section-tagged savestate archive stored in PSRAM during Save/Load */
struct SaveState {
  char name[24];
};

#define GW_ARCHIVE_MAGIC 0x56535747u /* "GWSV" */
#define GW_MAX_ARCHIVE_BYTES (256u * 1024u)

static uint8_t* g_arch_buf = NULL;
static size_t   g_arch_cap = 0;
static size_t   g_arch_len = 0;
static const uint8_t* g_read_buf = NULL;
static size_t   g_read_len = 0;
static SaveState g_active_state;

static bool ensure_arch_cap(size_t need) {
  if (need > GW_MAX_ARCHIVE_BYTES) return false;
  if (g_arch_cap >= need && g_arch_buf) return true;
  size_t new_cap = g_arch_cap ? g_arch_cap : 65536u;
  while (new_cap < need) new_cap *= 2u;
  if (new_cap > GW_MAX_ARCHIVE_BYTES) new_cap = GW_MAX_ARCHIVE_BYTES;
  uint8_t* nb = (uint8_t*)bmo_psram_realloc(g_arch_buf, new_cap);
  if (!nb) return false;
  g_arch_buf = nb;
  g_arch_cap = new_cap;
  return true;
}

void saveGwenesisStateResetArchive(void) {
  if (g_arch_buf) { bmo_psram_free(g_arch_buf); g_arch_buf = NULL; }
  g_arch_cap = 0;
  g_arch_len = 0;
  g_read_buf = NULL;
  g_read_len = 0;
}

static void copy_tag(char dst[24], const char* src) {
  memset(dst, 0, 24);
  if (src) strncpy(dst, src, 23);
}

SaveState* saveGwenesisStateOpenForWrite(const char* name) {
  copy_tag(g_active_state.name, name);
  return &g_active_state;
}

SaveState* saveGwenesisStateOpenForRead(const char* name) {
  copy_tag(g_active_state.name, name);
  return &g_active_state;
}

void saveGwenesisStateSetBuffer(SaveState* state, const char* name, const void* src, size_t size) {
  if (!state || !name || !src) return;
  size_t rec = 24u + 24u + 4u + size;
  if (!ensure_arch_cap(g_arch_len + rec)) return;
  uint8_t* w = g_arch_buf + g_arch_len;
  memcpy(w, state->name, 24); w += 24;
  char key[24]; copy_tag(key, name);
  memcpy(w, key, 24);         w += 24;
  uint32_t sz = (uint32_t)size;
  memcpy(w, &sz, 4);          w += 4;
  if (size > 0) memcpy(w, src, size);
  g_arch_len += rec;
}

void saveGwenesisStateSet(SaveState* state, const char* name, int value) {
  int32_t v = (int32_t)value;
  saveGwenesisStateSetBuffer(state, name, &v, sizeof(v));
}

static const uint8_t* find_entry(const SaveState* state, const char* name, uint32_t* outSize) {
  if (!state || !name || !g_read_buf || g_read_len < 4u) return NULL;
  char key[24]; copy_tag(key, name);
  size_t pos = 4u;
  while (pos + 52u <= g_read_len) {
    const uint8_t* e = g_read_buf + pos;
    uint32_t sz = 0;
    memcpy(&sz, e + 48u, 4u);
    if (pos + 52u + sz > g_read_len) break;
    if (memcmp(e, state->name, 24) == 0 && memcmp(e + 24, key, 24) == 0) {
      if (outSize) *outSize = sz;
      return e + 52u;
    }
    pos += 52u + sz;
  }
  return NULL;
}

void saveGwenesisStateGetBuffer(SaveState* state, const char* name, void* dest, size_t size) {
  if (!dest || size == 0) return;
  uint32_t foundSz = 0;
  const uint8_t* p = find_entry(state, name, &foundSz);
  if (!p) return;
  size_t n = foundSz < size ? (size_t)foundSz : size;
  memcpy(dest, p, n);
}

int saveGwenesisStateGet(SaveState* state, const char* name) {
  int32_t v = 0;
  saveGwenesisStateGetBuffer(state, name, &v, sizeof(v));
  return (int)v;
}

/* Forward declarations for core modules lacking state prototypes */
extern void gwenesis_m68k_save_state(void);
extern void gwenesis_m68k_load_state(void);
extern void gwenesis_z80inst_save_state(void);
extern void gwenesis_z80inst_load_state(void);
extern void gwenesis_ym2612_save_state(void);
extern void gwenesis_ym2612_load_state(void);
extern void gwenesis_sn76489_save_state(void);
extern void gwenesis_sn76489_load_state(void);

bool saveGwenesisStateSaveToBuffer(void* dst, size_t maxBytes, size_t* outBytes) {
  g_arch_len = 0;
  if (!ensure_arch_cap(4096u)) return false;
  uint32_t magic = GW_ARCHIVE_MAGIC;
  memcpy(g_arch_buf, &magic, 4u);
  g_arch_len = 4u;
  gwenesis_bus_save_state();
  gwenesis_io_save_state();
  gwenesis_vdp_mem_save_state();
  gwenesis_vdp_gfx_save_state();
  gwenesis_m68k_save_state();
  gwenesis_z80inst_save_state();
  gwenesis_ym2612_save_state();
  gwenesis_sn76489_save_state();
  if (outBytes) *outBytes = g_arch_len;
  if (!dst || maxBytes < g_arch_len) return false;
  memcpy(dst, g_arch_buf, g_arch_len);
  return true;
}

bool saveGwenesisStateLoadFromBuffer(const void* src, size_t bytes) {
  if (!src || bytes < 4u) return false;
  uint32_t magic = 0;
  memcpy(&magic, src, 4u);
  if (magic != GW_ARCHIVE_MAGIC) return false;
  g_read_buf = (const uint8_t*)src;
  g_read_len = bytes;
  gwenesis_bus_load_state();
  gwenesis_io_load_state();
  gwenesis_vdp_mem_load_state();
  gwenesis_vdp_gfx_load_state();
  gwenesis_m68k_load_state();
  gwenesis_z80inst_load_state();
  gwenesis_ym2612_load_state();
  gwenesis_sn76489_load_state();
  g_read_buf = NULL;
  g_read_len = 0;
  return true;
}
