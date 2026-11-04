#ifndef LCD_WAVE_H
#define LCD_WAVE_H

#include "xil_types.h"
#include "ecg_ring.h"

typedef struct {
    u32 rr_ms;
    u32 hr_bpm;

    u8 lod;
    u8 otr;
    u8 overflow;
    u8 running;

    u64 r_index;
    u64 total_samples;

    u8 cls_valid;
    u8 cls_id;
    u8 cls_conf;
    u16 gain, hp_x100, lp_hz;
    u8 sd_ready, recording, files_closed, error, frontend_ready, keys_ready, menu;
    u8 net_ready;
    u32 record_id, elapsed_s;
    const char *cfg_source;
    const char *note;
    const char *touch_hint;
} LcdEcgInfo;

int  lcd_wave_init(void);
void lcd_wave_mark_rpeak(u64 index);
void lcd_wave_draw(const EcgRing *ring, const LcdEcgInfo *info);

#endif /* LCD_WAVE_H */
