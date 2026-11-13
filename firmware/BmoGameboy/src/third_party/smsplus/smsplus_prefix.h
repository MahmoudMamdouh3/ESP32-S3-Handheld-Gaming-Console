#ifndef SMSPLUS_PREFIX_H
#define SMSPLUS_PREFIX_H

/* Isolate smsplus global C symbols from other compiled cores (e.g., gwenesis Z80, race CZ80, stella). */
#define bitmap               smsp_bitmap
#define cart                 smsp_cart
#define snd                  smsp_snd
#define input                smsp_input
#define sms                  smsp_sms
#define vdp                  smsp_vdp
#define ym2413               smsp_ym2413
#define sn                   smsp_sn
#define cramd                smsp_cramd
#define linebuf              smsp_linebuf
#define vp_vstart            smsp_vp_vstart
#define vp_vend              smsp_vp_vend
#define vp_hstart            smsp_vp_hstart
#define vp_hend              smsp_vp_hend
#define is_vram_dirty        smsp_is_vram_dirty
#define cacheKillPtr         smsp_cacheKillPtr
#define freePtr              smsp_freePtr
#define atex                 smsp_atex
#define text_counter         smsp_text_counter
#define after_EI             smsp_after_EI
#define Z80_Context          smsp_Z80_Context
#define z80_ICount           smsp_z80_ICount
#define cpu_readmap          smsp_cpu_readmap
#define cpu_writemap         smsp_cpu_writemap

#define render_bg            smsp_render_bg
#define render_init          smsp_render_init
#define render_reset         smsp_render_reset
#define render_bg_gg         smsp_render_bg_gg
#define render_bg_sms        smsp_render_bg_sms
#define render_obj           smsp_render_obj
#define render_line          smsp_render_line
#define update_cache         smsp_update_cache
#define palette_sync         smsp_palette_sync
#define remap_8_to_16        smsp_remap_8_to_16
#define render_shutdown      smsp_render_shutdown
#define render_set_console_type smsp_render_set_console_type
#define vramMarkTileDirty    smsp_vramMarkTileDirty
#define getCache             smsp_getCache

#define sms_frame            smsp_sms_frame
#define sms_init             smsp_sms_init
#define sms_init_ram         smsp_sms_init_ram
#define sms_shutdown_ram     smsp_sms_shutdown_ram
#define sms_reset            smsp_sms_reset
#define sms_irq_callback     smsp_sms_irq_callback
#define sms_mapper_w         smsp_sms_mapper_w
#define cpu_reset            smsp_cpu_reset
#define cpu_writemem16       smsp_cpu_writemem16
#define cpu_writeport        smsp_cpu_writeport
#define cpu_readport         smsp_cpu_readport
#define unalChar             smsp_unalChar

#define vdp_init             smsp_vdp_init
#define vdp_reset            smsp_vdp_reset
#define vdp_ctrl_w           smsp_vdp_ctrl_w
#define vdp_ctrl_r           smsp_vdp_ctrl_r
#define vdp_vcounter_r       smsp_vdp_vcounter_r
#define vdp_hcounter_r       smsp_vdp_hcounter_r
#define vdp_data_w           smsp_vdp_data_w
#define vdp_data_r           smsp_vdp_data_r
#define vdp_run              smsp_vdp_run
#define vdp_init_vram        smsp_vdp_init_vram
#define vdp_shutdown_vram    smsp_vdp_shutdown_vram

#define make_tms_tables      smsp_make_tms_tables
#define tms_shutdown         smsp_tms_shutdown
#define render_bg_tms        smsp_render_bg_tms
#define render_obj_tms       smsp_render_obj_tms
#define parse_line           smsp_parse_line

#define SN76496_alloc        smsp_SN76496_alloc
#define SN76496_free         smsp_SN76496_free
#define SN76496Write         smsp_SN76496Write
#define SN76496Update        smsp_SN76496Update
#define SN76496_set_clock    smsp_SN76496_set_clock
#define SN76496_set_gain     smsp_SN76496_set_gain
#define SN76496_init         smsp_SN76496_init

#define emu_system_init      smsp_emu_system_init
#define system_shutdown      smsp_system_shutdown
#define system_reset         smsp_system_reset
#define system_load_sram     smsp_system_load_sram
#define system_save_state    smsp_system_save_state
#define system_load_state    smsp_system_load_state
#define audio_init           smsp_audio_init
#define audio_shutdown       smsp_audio_shutdown
#define ym2413_write         smsp_ym2413_write

#define z80_reset            smsp_z80_reset
#define z80_exit             smsp_z80_exit
#define z80_execute          smsp_z80_execute
#define z80_burn             smsp_z80_burn
#define z80_get_context      smsp_z80_get_context
#define z80_set_context      smsp_z80_set_context
#define z80_get_pc           smsp_z80_get_pc
#define z80_set_pc           smsp_z80_set_pc
#define z80_get_sp           smsp_z80_get_sp
#define z80_set_sp           smsp_z80_set_sp
#define z80_get_reg          smsp_z80_get_reg
#define z80_set_reg          smsp_z80_set_reg
#define z80_set_nmi_line     smsp_z80_set_nmi_line
#define z80_set_irq_line     smsp_z80_set_irq_line
#define z80_set_irq_callback smsp_z80_set_irq_callback
#define z80_state_save       smsp_z80_state_save
#define z80_state_load       smsp_z80_state_load
#define z80_info             smsp_z80_info
#define z80_dasm             smsp_z80_dasm
#define z80_allocate_flag_tables smsp_z80_allocate_flag_tables

#endif /* SMSPLUS_PREFIX_H */
