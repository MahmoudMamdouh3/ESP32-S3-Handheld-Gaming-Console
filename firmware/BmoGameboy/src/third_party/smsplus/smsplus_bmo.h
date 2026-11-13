#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BMO_SMS_CONSOLE_SMS = 0,
  BMO_SMS_CONSOLE_GG = 1,
  BMO_SMS_CONSOLE_SG1000 = 2,
  BMO_SMS_CONSOLE_COLECO = 3
} BmoSmsConsoleMode;

/* Match smsplus system.h pad/system bits cleanly without exposing macro collisions. */
#define BMO_SMS_INPUT_UP      (0x00000001)
#define BMO_SMS_INPUT_DOWN    (0x00000002)
#define BMO_SMS_INPUT_LEFT    (0x00000004)
#define BMO_SMS_INPUT_RIGHT   (0x00000008)
#define BMO_SMS_INPUT_BUTTON2 (0x00000010)
#define BMO_SMS_INPUT_BUTTON1 (0x00000020)

#define BMO_SMS_SYS_START     (0x00000001)
#define BMO_SMS_SYS_PAUSE     (0x00000002)
#define BMO_SMS_COLECO_KEYPAD_SHIFT (8)
#define BMO_SMS_COLECO_KEYPAD_NONE  (0x00000F00)

bool bmo_sms_init(const uint8_t* romData, size_t romSize,
                  BmoSmsConsoleMode mode, const uint8_t* colecoBios8k);
void bmo_sms_set_input(int pad0, int systemFlags);
int bmo_sms_input_pad0(void);
int bmo_sms_input_system(void);
void bmo_sms_run_frame(int skipRender);
void bmo_sms_destroy(void);
bool bmo_sms_is_running(void);

const uint8_t* bmo_sms_framebuffer_rgb332(void);
uint16_t bmo_sms_rgb332_to_rgb565(uint8_t idx);
void bmo_sms_render_panel565(uint16_t* dst256x192);

uint8_t* bmo_sms_wram_data(size_t* outSize);
uint8_t* bmo_sms_sram_data(size_t* outSize);
bool bmo_sms_sram_used(void);

size_t bmo_sms_state_size(void);
bool bmo_sms_save_state(uint8_t* dst, size_t cap, size_t* outSize);
bool bmo_sms_load_state(const uint8_t* src, size_t len);

#ifdef __cplusplus
}
#endif
