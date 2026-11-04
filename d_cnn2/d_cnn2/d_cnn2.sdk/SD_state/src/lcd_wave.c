#include "lcd_wave.h"
#include "lcd_port.h"
#include "ecg_config.h"
#include "ecg_result.h"
#include "ecg_touch.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define WAVE_X0          10
#define WAVE_Y0          80
#define WAVE_W           (LCD_WIDTH - 20)
#define WAVE_H           (LCD_HEIGHT - WAVE_Y0 - TOUCH_FOOTER_HEIGHT - 2)

#define INFO_X0          10
#define INFO_Y0          6

#ifndef LCD_SHOW_RAW_WAVE
#define LCD_SHOW_RAW_WAVE 0
#endif
#define SWEEP_GAP 3
#define CHANNELS (1 + LCD_SHOW_RAW_WAVE)

typedef struct {
    u64 cycle;
    int valid;
    int lo[CHANNELS], hi[CHANNELS], last[CHANNELS];
    u64 peak_cycle;
    int peak;
    int dirty;
} SweepColumn;

static SweepColumn columns[WAVE_W];
static u64 next_sample;
static char previous_line1[96], previous_line2[96], previous_line3[96], previous_line4[96];
static const char *previous_hint;
static int previous_footer_state = -1;

static void draw_controls(const LcdEcgInfo *info)
{
    static const char *labels[] = {"RECORD", "GAIN -", "GAIN +", "HP", "LP", "SAVE"};
    int i, y = LCD_HEIGHT - TOUCH_FOOTER_HEIGHT;
    int text_y = y + (TOUCH_FOOTER_HEIGHT - 7 * LCD_FONT_SCALE) / 2;
    int state = info->recording | (info->error ? 4 : 0) |
                (info->sd_ready && info->frontend_ready ? 8 : 0);
    if (state == previous_footer_state && info->touch_hint == previous_hint) return;
    previous_footer_state = state;
    previous_hint = info->touch_hint;
    lcd_port_fill_rect(0, y, LCD_WIDTH, TOUCH_FOOTER_HEIGHT, LCD_COLOR_BLACK);
    if (info->touch_hint) {
        lcd_port_draw_string(10, text_y, info->touch_hint, LCD_COLOR_YELLOW, LCD_COLOR_BLACK);
    } else {
        int cell = LCD_WIDTH / TOUCH_BUTTON_COUNT;
        for (i = 0; i < TOUCH_BUTTON_COUNT; ++i) {
            const char *label = i == TOUCH_RECORD && info->recording ? "STOP" : labels[i];
            int disabled = i == TOUCH_RECORD ? !info->recording && (!(state & 8) || (state & 4))
                         : info->recording || (state & 4);
            u16 color = disabled ? LCD_COLOR_DARKGRAY :
                        i == TOUCH_RECORD && info->recording ? LCD_COLOR_RED : 0x0330U;
            lcd_port_fill_rect(i * cell + 4, y + 4, cell - 8, TOUCH_FOOTER_HEIGHT - 8, color);
            lcd_port_draw_string(i * cell + (cell - (int)strlen(label) * 6 * LCD_FONT_SCALE) / 2,
                                 text_y, label, LCD_COLOR_WHITE, color);
        }
    }
    lcd_port_flush_rect(0, y, LCD_WIDTH, TOUCH_FOOTER_HEIGHT);
}

/*
 * If the signal looks too flat or too clipped, tune this value.
 * Smaller value -> waveform appears taller.
 * Larger value  -> waveform appears shorter.
 */
#ifndef LCD_RAW_WAVE_SCALE
#define LCD_RAW_WAVE_SCALE   1.25f
#endif

#ifndef LCD_FILT_WAVE_SCALE
#define LCD_FILT_WAVE_SCALE  1.25f
#endif

static const char *lcd_cls_name(u8 cls_id)
{
    return ecg_result_class_name(cls_id);
}

static int f_to_y_scaled(float x, float scale)
{
    float v;
    int mid;
    int amp;
    int y;

    if (scale <= 0.001f) {
        scale = 1.00f;
    }

    v = x / scale;

    mid = WAVE_Y0 + WAVE_H / 2;
    amp = WAVE_H / 2 - 6;

    if (v > 1.0f) {
        v = 1.0f;
    }

    if (v < -1.0f) {
        v = -1.0f;
    }

    y = mid - (int)(v * (float)amp);

    if (y < WAVE_Y0) {
        y = WAVE_Y0;
    }

    if (y >= WAVE_Y0 + WAVE_H) {
        y = WAVE_Y0 + WAVE_H - 1;
    }

    return y;
}

static void lcd_wave_draw_grid(void)
{
    int x;
    int y;

    lcd_port_fill_rect(WAVE_X0, WAVE_Y0, WAVE_W, WAVE_H, LCD_COLOR_BLACK);

    for (y = WAVE_Y0; y < WAVE_Y0 + WAVE_H; y += 24) {
        lcd_port_draw_line(WAVE_X0,
                           y,
                           WAVE_X0 + WAVE_W - 1,
                           y,
                           LCD_COLOR_DARKGRAY);
    }

    for (x = WAVE_X0; x < WAVE_X0 + WAVE_W; x += 40) {
        lcd_port_draw_line(x,
                           WAVE_Y0,
                           x,
                           WAVE_Y0 + WAVE_H - 1,
                           LCD_COLOR_DARKGRAY);
    }

    lcd_port_draw_line(WAVE_X0,
                       WAVE_Y0 + WAVE_H / 2,
                       WAVE_X0 + WAVE_W - 1,
                       WAVE_Y0 + WAVE_H / 2,
                       LCD_COLOR_GRAY);
}

static void lcd_wave_draw_info(const LcdEcgInfo *info)
{
    char line1[96], line2[96], line3[96], line4[96];
    const char *cls_name = "UNK";
    const char *sd;
    static const char *menus[] = {"GAIN-60", "GAIN+60", "HP TOGGLE", "LP40/55", "SAVE CFG"};


    if (info == 0) {
        return;
    }

    if (info->cls_valid) {
        cls_name = lcd_cls_name(info->cls_id);
    }

    /*
     * The built-in font converts lowercase to uppercase automatically.
     * So "ms" and "bpm" will appear as "MS" and "BPM".
     */
    snprintf(line1, sizeof(line1),
            "RR:%lums HR:%lubpm LOD:%d OTR:%d",
            (unsigned long)info->rr_ms,
            (unsigned long)info->hr_bpm,
            info->lod,
            info->otr);

    if (info->cls_conf == ECG_CONFIDENCE_UNAVAILABLE) {
        snprintf(line2, sizeof(line2),
                "OVR:%d RUN:%d CLS:%s CF:N/A UDP:%s",
                info->overflow,
                info->running,
                cls_name, info->net_ready ? "ON" : "OFF");
    } else {
        snprintf(line2, sizeof(line2),
                "OVR:%d RUN:%d CLS:%s CF:%u%% UDP:%s",
                info->overflow,
                info->running,
                cls_name,
                info->cls_conf, info->net_ready ? "ON" : "OFF");
    }
    sd = info->error ? "ERROR" : !info->sd_ready ? "NO CARD" :
         info->recording == 2U ? "PAUSED" : info->recording ? "REC" :
         info->files_closed ? "CLOSED" : "IDLE";
    snprintf(line3, sizeof(line3), "SD:%s R%04lu %02lu:%02lu:%02lu E:%u",
        sd, (unsigned long)info->record_id, (unsigned long)(info->elapsed_s / 3600U),
        (unsigned long)(info->elapsed_s / 60U % 60U),
        (unsigned long)(info->elapsed_s % 60U), info->error);
    if (info->note) {
        snprintf(line4, sizeof(line4), "%s", info->note);
    } else {
        snprintf(line4, sizeof(line4), "G:%u HP:%s LP:%u %s %s",
            info->gain, !info->frontend_ready ? "ERR" : info->hp_x100 == 5U ? ".05" : "4",
            info->lp_hz, info->cfg_source ? info->cfg_source : "?",
            info->keys_ready ? menus[info->menu % 5U] : "KEYS:OFF");
    }

    if (strcmp(line1, previous_line1) == 0 &&
        strcmp(line2, previous_line2) == 0 &&
        strcmp(line3, previous_line3) == 0 &&
        strcmp(line4, previous_line4) == 0) {
        return;
    }
    strcpy(previous_line1, line1);
    strcpy(previous_line2, line2);
    strcpy(previous_line3, line3);
    strcpy(previous_line4, line4);
    lcd_port_fill_rect(0, 0, LCD_WIDTH, WAVE_Y0 - 2, LCD_COLOR_BLACK);
    lcd_port_draw_string(INFO_X0,
                         INFO_Y0,
                         line1,
                         LCD_COLOR_GREEN,
                         LCD_COLOR_BLACK);
    lcd_port_draw_string(INFO_X0, INFO_Y0 + 32, line3,
                         info->error ? LCD_COLOR_RED : LCD_COLOR_GREEN, LCD_COLOR_BLACK);
    lcd_port_draw_string(INFO_X0, INFO_Y0 + 48, line4,
                         LCD_COLOR_CYAN, LCD_COLOR_BLACK);

    lcd_port_draw_string(INFO_X0,
                         INFO_Y0 + 16,
                         line2,
                         LCD_COLOR_YELLOW,
                         LCD_COLOR_BLACK);
    lcd_port_flush_rect(0, 0, LCD_WIDTH, WAVE_Y0 - 2);
}

static int sample_column(u64 index)
{
    return (int)((index % LCD_WIN_LEN) * WAVE_W / LCD_WIN_LEN);
}

static void restore_column(int col)
{
    int x = WAVE_X0 + col;
    int y;
    lcd_port_fill_rect(x, WAVE_Y0, 1, WAVE_H,
                      col % 40 == 0 ? LCD_COLOR_DARKGRAY : LCD_COLOR_BLACK);
    for (y = WAVE_Y0; y < WAVE_Y0 + WAVE_H; y += 24) {
        lcd_port_draw_pixel(x, y, LCD_COLOR_DARKGRAY);
    }
    lcd_port_draw_pixel(x, WAVE_Y0 + WAVE_H / 2, LCD_COLOR_GRAY);
}

static void reset_sweep(void)
{
    memset(columns, 0, sizeof(columns));
    next_sample = 0;
    lcd_wave_draw_grid();
    lcd_port_flush_rect(WAVE_X0, WAVE_Y0, WAVE_W, WAVE_H);
}

/* Called for every detection, so a batch containing several beats loses none. */
void lcd_wave_mark_rpeak(u64 index)
{
    SweepColumn *c = &columns[sample_column(index)];
    c->peak = 1;
    c->peak_cycle = index / LCD_WIN_LEN;
    c->dirty = 1;
}

int lcd_wave_init(void)
{
    if (lcd_port_init() != 0) {
        return -1;
    }
    memset(columns, 0, sizeof(columns));
    next_sample = 0;
    previous_line1[0] = previous_line2[0] = '\0';
    previous_line3[0] = previous_line4[0] = '\0';
    previous_footer_state = -1;
    lcd_port_clear(LCD_COLOR_BLACK);
    lcd_wave_draw_grid();
    lcd_wave_draw_info(0);
    lcd_port_flush();
    return 0;
}

void lcd_wave_draw(const EcgRing *ring, const LcdEcgInfo *info)
{
    u64 total, index;
    int col, ch;
    if (ring == 0) {
        return;
    }
    lcd_wave_draw_info(info);
    if (info) draw_controls(info);
    total = ring->total_samples;
    if (total < next_sample || next_sample < ecg_ring_oldest_index(ring) ||
        total - next_sample > LCD_WIN_LEN) {
        reset_sweep();
        next_sample = total > LCD_WIN_LEN ? total - LCD_WIN_LEN : 0;
        if (next_sample < ecg_ring_oldest_index(ring)) {
            next_sample = ecg_ring_oldest_index(ring);
        }
    }
    for (index = next_sample; index < total; ++index) {
        float values[CHANNELS];
        int ys[CHANNELS];
        int usable = ecg_ring_get_filt(ring, index, &values[0]);
        u64 cycle = index / LCD_WIN_LEN;
        SweepColumn *c;
#if LCD_SHOW_RAW_WAVE
        usable = usable && ecg_ring_get_raw(ring, index, &values[1]);
#endif
        col = sample_column(index);
        c = &columns[col];
        for (ch = 0; ch < CHANNELS; ++ch) {
            if (!usable || !isfinite(values[ch])) {
                usable = 0;
                break;
            }
            ys[ch] = f_to_y_scaled(values[ch],
                                 ch == 0 ? LCD_FILT_WAVE_SCALE : LCD_RAW_WAVE_SCALE);
        }
        if (!usable) {
            c->valid = 0;
            c->dirty = 1;
            continue;
        }
        if (!c->valid || c->cycle != cycle) {
            SweepColumn *prev = col > 0 ? &columns[col - 1] : 0;
            c->valid = 1;
            c->cycle = cycle;
            if (c->peak_cycle != cycle) {
                c->peak = 0;
            }
            for (ch = 0; ch < CHANNELS; ++ch) {
                c->lo[ch] = c->hi[ch] = ys[ch];
                /* Connect adjacent columns, but never bridge the screen wrap. */
                if (prev && prev->valid && prev->cycle == cycle) {
                    if (prev->last[ch] < c->lo[ch]) c->lo[ch] = prev->last[ch];
                    if (prev->last[ch] > c->hi[ch]) c->hi[ch] = prev->last[ch];
                }
            }
        }
        for (ch = 0; ch < CHANNELS; ++ch) {
            if (ys[ch] < c->lo[ch]) c->lo[ch] = ys[ch];
            if (ys[ch] > c->hi[ch]) c->hi[ch] = ys[ch];
            c->last[ch] = ys[ch];
        }
        c->dirty = 1;
    }
    if (total > next_sample) {
        int head = sample_column(total - 1);
        int gap;
        for (gap = 1; gap <= SWEEP_GAP; ++gap) {
            SweepColumn *c = &columns[(head + gap) % WAVE_W];
            if (c->valid || c->peak) {
                c->valid = c->peak = 0;
                c->dirty = 1;
            }
        }
    }
    next_sample = total;
    for (col = 0; col < WAVE_W; ++col) {
        SweepColumn *c = &columns[col];
        int x = WAVE_X0 + col;
        if (!c->dirty) continue;
        restore_column(col);
        if (c->valid) {
            for (ch = CHANNELS - 1; ch >= 0; --ch) {
                lcd_port_draw_line(x, c->lo[ch], x, c->hi[ch],
                                  ch == 0 ? LCD_COLOR_YELLOW : LCD_COLOR_GRAY);
            }
            if (c->peak && c->peak_cycle == c->cycle) {
                /* Short tick leaves the waveform readable. */
                lcd_port_draw_line(x, WAVE_Y0 + 2, x, WAVE_Y0 + 10, LCD_COLOR_RED);
            }
        }
    }
    /* Coalesce dirty columns; a wrap naturally produces two flush regions. */
    col = 0;
    while (col < WAVE_W) {
        int start;
        if (!columns[col].dirty) {
            ++col;
            continue;
        }
        start = col;
        do {
            columns[col++].dirty = 0;
        } while (col < WAVE_W && columns[col].dirty);
        lcd_port_flush_rect(WAVE_X0 + start, WAVE_Y0, col - start, WAVE_H);
    }
}
