#include <stdio.h>
#include "platform.h"
#include "sleep.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xuartps_hw.h"

#include "ecg_config.h"
#include "ad9280_stream.h"

#include "ecg_ring.h"
#include "ecg_filter.h"
#include "ecg_rpeak.h"
#include "ecg_beat.h"
#include "ecg_result.h"
#include "ecg_infer.h"
#include "ecg_cnn_infer.h"
#include "lcd_port.h"
#include "cn121_ps.h"
#include "lcd_wave.h"
#include "ecg_recorder.h"
#include "device_state.h"
#include "ecg_net.h"
#include "ecg_quality.h"
#include "ecg_lod_event.h"
#include "ecg_standalone.h"
#include "ecg_touch.h"

#define LCD_REFRESH_MS      40U
#define ECG_SD_FILTER_SCALE  1000.0f

#ifndef STDIN_BASEADDRESS
#define STDIN_BASEADDRESS XPAR_PS7_UART_1_BASEADDR
#endif

static Ad9280Sample g_samples[AD_READ_BURST_MAX];
static EcgRing g_ring;
static EcgFilter g_filter;
static RPeakDetector g_rdet;
static BeatQueue g_beat_q;
static EcgSignalQuality g_quality;
static Cn121Status g_cn121_status;
static float g_cnn_beat_buf[ECG_CNN_FLOAT_LEN];


static DeviceContext g_dev;
static EcgRecorder   g_rec;
static EcgSettings g_settings;
static EcgSettingsSource g_settings_source;
static EcgStandalone g_control;
static u8 g_cn121_ready, g_keys_ready, g_files_closed = 1U;
static const char *g_control_note = 0;
static u32 g_control_note_ms;


static u64 g_record_sample_count = 0ULL;
static u64 g_record_global_start_index = 0ULL;


static XTime g_record_t0 = 0;

typedef struct { u64 calls, ticks, max_ticks; } RuntimeCost;
static RuntimeCost g_sd_cost, g_cnn_cost, g_lcd_cost;
static RuntimeCost g_cnn_service_cost;
static BeatTask g_active_beat;
static u8 g_inference_active = 0U;
static u8 g_inference_discard = 0U;
static XTime g_inference_started;
static u8 g_frontend_changed = 0U;
static XTime profile_start(void)
{
    XTime t;
    XTime_GetTime(&t);
    return t;
}
static void profile_end(RuntimeCost *cost, XTime start)
{
    XTime end;
    u64 dt;
    XTime_GetTime(&end);
    dt = end - start;
    cost->calls++;
    cost->ticks += dt;
    if (dt > cost->max_ticks) cost->max_ticks = dt;
}
static void print_cost(const char *name, const RuntimeCost *cost)
{
    u64 average = cost->calls ? cost->ticks / cost->calls : 0ULL;
    xil_printf("[PERF] %s calls=%lu mean_us=%lu max_us=%lu\r\n", name,
               (unsigned long)cost->calls,
               (unsigned long)(average * 1000000ULL / COUNTS_PER_SECOND),
               (unsigned long)(cost->max_ticks * 1000000ULL / COUNTS_PER_SECOND));
}

static u64 ticks_to_ms(XTime t0, XTime t1)
{
    u64 dt = (u64)(t1 - t0);
    return (dt * 1000ULL) / (u64)COUNTS_PER_SECOND;
}

static u32 record_timestamp_ms(u64 record_sample_index)
{
    if (record_sample_index == 0ULL) {
        return 0U;
    }

    return (u32)(((record_sample_index - 1ULL) * 1000ULL) /
                 (u64)ECG_FS_HZ);
}

static int global_to_record_index(u64 global_index, u64 *record_index)
{
    u64 local_index;

    if ((record_index == 0) ||
        (global_index < g_record_global_start_index)) {
        return 0;
    }

    local_index = global_index - g_record_global_start_index + 1ULL;
    if (local_index > g_record_sample_count) {
        return 0;
    }

    *record_index = local_index;
    return 1;
}


static void log_diagnostic(const char *type, u64 index, u8 detail)
{
    if (ecg_recorder_is_recording(&g_rec) &&
        ecg_recorder_write_diagnostic(&g_rec, type, index,
                                      record_timestamp_ms(index), detail) != 0) {
        ecg_recorder_stop(&g_rec);
        device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
    }
}

static void log_skipped_beat(u64 global_index, u8 reason)
{
    u64 index;
    if (global_to_record_index(global_index, &index))
        log_diagnostic("CNN_SKIP", index, reason);
}

static void log_pending_beats(u8 reason)
{
    u32 i;
    if (g_inference_active && !g_inference_discard) {
        log_skipped_beat(g_active_beat.r_index, reason);
        g_inference_discard = 1U;
    }
    for (i = 0; i < MAX_BEAT_TASKS; ++i) {
        BeatTask *t = &g_beat_q.tasks[i];
        if (t->state == BEAT_WAIT_FUTURE || t->state == BEAT_READY)
            log_skipped_beat(t->r_index, reason);
    }
}

static s16 ecg_float_to_s16_scaled(float x)
{
    float y;

    y = x * ECG_SD_FILTER_SCALE;

    if (y > 32767.0f) {
        return 32767;
    }

    if (y < -32768.0f) {
        return -32768;
    }

    if (y >= 0.0f) {
        return (s16)(y + 0.5f);
    } else {
        return (s16)(y - 0.5f);
    }
}

static int uart_try_get_char(u8 *ch)
{
    if (XUartPs_IsReceiveData(STDIN_BASEADDRESS)) {
        *ch = XUartPs_RecvByte(STDIN_BASEADDRESS);
        return 1;
    }

    return 0;
}

static void update_cn121_snapshot(void)
{
    if (!g_cn121_ready) return;
    u8 config = cn121_read_reg(REG_CONFIG);
    u8 chset = cn121_read_reg(REG_CHSET);
    u8 hp_code = (u8)((chset >> 6) & 0x01U);
    u8 lp_code = (u8)((chset >> 4) & 0x03U);
    static const u16 lp_hz[4] = {40U, 55U, 150U, 300U};
    if (config != CN121_CONFIG_SENSING || chset == 0xFFU) {
        log_pending_beats(6U);
        beat_queue_clear(&g_beat_q);
        if (ecg_recorder_is_recording(&g_rec)) (void)ecg_recorder_stop(&g_rec);
        g_cn121_ready = 0U;
        g_frontend_changed = 1U;
        g_files_closed = 0U;
        device_set_error(&g_dev, DEV_ERR_CN121_FAILED);
        return;
    }

    device_set_cn121_snapshot(&g_dev,
                              config,
                              chset,
                              cn121_gain_code_to_value(chset & 0x0FU),
                              hp_code ? 5U : 400U,
                              lp_hz[lp_code]);
}


static void process_device_command(u8 ch)
{
    XTime t_now;
    u32 now_ms;

    XTime_GetTime(&t_now);
    now_ms = (u32)ticks_to_ms(0, t_now);

    switch (ch) {
    case 'r':
    case 'R':
    {
        u32 next_record_id;
        ecg_standalone_cancel_auto(&g_control);
        if (!g_cn121_ready || g_settings_source == ECG_CFG_INVALID) {
            xil_printf("[DEV] record blocked: frontend/configuration invalid\r\n");
            break;
        }

        if ((device_get_state(&g_dev) == DEV_STATE_RECORDING) ||
            (device_get_state(&g_dev) == DEV_STATE_PAUSED)) {
            xil_printf("[DEV] recording already active, record_id=%lu\r\n",
                       (unsigned long)g_dev.record_id);
            break;
        }

        if (ecg_recorder_get_next_record_id(&g_rec,
                                            g_dev.record_id + 1U,
                                            &next_record_id) != 0) {
            device_set_error(&g_dev, DEV_ERR_SD_INIT_FAILED);
            xil_printf("[SD] cannot allocate a record id\r\n");
            break;
        }

        g_dev.record_id = next_record_id - 1U;
        ad9280_stream_clear_overflow();

        if (device_start_recording(&g_dev, now_ms) == 0) {
            if (ecg_recorder_start(&g_rec, &g_dev, ECG_FS_HZ) != 0) {
                device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
                xil_printf("[DEV] start record failed\r\n");
            } else {
                g_record_sample_count = 0ULL;
                g_record_global_start_index = g_ring.total_samples;
                g_record_t0 = t_now;
                g_files_closed = 0U;

                xil_printf("[DEV] recording started, record_id=%lu\r\n",
                           (unsigned long)g_dev.record_id);
            }
        } else {
            xil_printf("[DEV] cannot start, state=%s err=%s\r\n",
                       device_state_name(device_get_state(&g_dev)),
                       device_error_name(device_get_error(&g_dev)));
        }
        break;
    }

    case 't':
    case 'T':
    {
        XTime t_stop;
        u32 duration_ms;
        u32 actual_fs;
        int stop_rc;
        ecg_standalone_cancel_auto(&g_control);
        if (!ecg_recorder_is_recording(&g_rec)) break;

        XTime_GetTime(&t_stop);
        duration_ms = (u32)ticks_to_ms(g_record_t0, t_stop);

        if (duration_ms > 0U) {
            actual_fs = (u32)((g_record_sample_count * 1000ULL) / duration_ms);
        } else {
            actual_fs = 0U;
        }

        log_pending_beats(2U);
        stop_rc = ecg_recorder_stop(&g_rec);
        if (stop_rc == 0 && device_get_error(&g_dev) == DEV_ERR_NONE) {
            device_stop_recording(&g_dev);
            g_files_closed = 1U;
        } else {
            g_files_closed = 0U;
            device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
        }

        xil_printf("[DEV] recording stopped, samples=%lu duration_ms=%lu actual_fs=%lu Hz\r\n",
                   (unsigned long)g_record_sample_count,
                   (unsigned long)duration_ms,
                   (unsigned long)actual_fs);
    }
        break;

    case 'p':
    case 'P':
        if (device_pause_recording(&g_dev) == 0) {
            xil_printf("[DEV] recording paused\r\n");
        } else {
            xil_printf("[DEV] pause failed, state=%s\r\n",
                       device_state_name(device_get_state(&g_dev)));
        }
        break;

    case 'v':
    case 'V':
        if (device_resume_recording(&g_dev) == 0) {
            xil_printf("[DEV] recording resumed\r\n");
        } else {
            xil_printf("[DEV] resume failed, state=%s err=%s\r\n",
                       device_state_name(device_get_state(&g_dev)),
                       device_error_name(device_get_error(&g_dev)));
        }
        break;

    case 'd':
    case 'D':
        xil_printf("[DEV] state=%s err=%s sd=%u rec=%u rec_id=%lu patient=%s rec_samples=%lu sqi=%s score=%u reason=%s/0x%02x warning=%s/0x%02x\r\n",
                   device_state_name(device_get_state(&g_dev)),
                   device_error_name(device_get_error(&g_dev)),
                   g_dev.sd_ready,
                   g_dev.recording_enabled,
                   (unsigned long)g_dev.record_id,
                   g_dev.patient.patient_id,
                   (unsigned long)g_record_sample_count,
                   ecg_quality_state_name(g_quality.state),
                   g_quality.score,
                   ecg_quality_reason_name(g_quality.reason),
                   g_quality.reason,
                   ecg_quality_warning_name(g_quality.warning),
                   g_quality.warning);
        break;

    case 'c':
    case 'C':
        if (ecg_recorder_is_recording(&g_rec)) {
            xil_printf("[DEV] stop record before clearing state\r\n");
            break;
        }
        device_clear_error(&g_dev);
        xil_printf("[DEV] error cleared\r\n");
        break;

    default:
    {
        u8 old_config = g_dev.cn121.config;
        u8 old_chset = g_dev.cn121.chset;
        if (!g_cn121_ready) break;
        if (cn121_handle_uart_cmd(ch)) {
            update_cn121_snapshot();
            if (old_config != g_dev.cn121.config)
                log_diagnostic("CN121_CONFIG", g_record_sample_count, g_dev.cn121.config);
            if (old_chset != g_dev.cn121.chset)
                log_diagnostic("CN121_CHSET", g_record_sample_count, g_dev.cn121.chset);
            if (old_config != g_dev.cn121.config || old_chset != g_dev.cn121.chset) {
                log_pending_beats(6U);
                beat_queue_clear(&g_beat_q);
                ecg_quality_init(&g_quality);
                ecg_rpeak_init(&g_rdet);
                ecg_filter_init(&g_filter);
                g_frontend_changed = 1U;
            }
        }
        break;
    }
    }
}

static void process_device_uart_cmd(void)
{
    u8 command;
    if (uart_try_get_char(&command)) process_device_command(command);
}

static void control_note(const char *note, u32 now_ms)
{
    g_control_note = note;
    g_control_note_ms = now_ms;
}

static void apply_standalone_menu(u8 menu, u32 now_ms)
{
    if (ecg_recorder_is_recording(&g_rec) || !g_cn121_ready ||
        device_get_error(&g_dev) != DEV_ERR_NONE) {
        control_note("STOP RECORD BEFORE CONFIG", now_ms);
    } else if (menu == ECG_MENU_SAVE) {
        g_settings.gain = g_dev.cn121.gain;
        g_settings.hp_x100 = g_dev.cn121.hp_x100;
        g_settings.lp_hz = g_dev.cn121.lp_hz;
        if (g_dev.sd_ready && ecg_settings_save_sd(&g_settings) == 0) {
            g_settings_source = ECG_CFG_SD;
            control_note("SETTINGS SAVED", now_ms);
        } else {
            control_note("SAVE FAILED", now_ms);
        }
    } else {
        u8 command = menu == ECG_MENU_GAIN_DOWN ? '-' :
            menu == ECG_MENU_GAIN_UP ? 'g' : 'h';
        u8 expected = g_dev.cn121.chset;
        if (menu == ECG_MENU_GAIN_DOWN)
            expected = (u8)((expected & 0xF0U) | ((expected & 15U) ? (expected & 15U) - 1U : 0U));
        else if (menu == ECG_MENU_GAIN_UP)
            expected = (u8)((expected & 0xF0U) | ((expected + 1U) & 0x0FU));
        else if (menu == ECG_MENU_HP) expected ^= 0x40U;
        if (menu == ECG_MENU_LP) {
            /* On-device menu only offers admitted 40/55-Hz profiles. */
            u8 value = (u8)(g_dev.cn121.chset & ~0x30U);
            if (g_dev.cn121.lp_hz != 55U) value |= 0x10U;
            expected = value;
            cn121_set_chset(value);
            update_cn121_snapshot();
            ecg_filter_init(&g_filter);
            ecg_quality_init(&g_quality);
            ecg_rpeak_init(&g_rdet);
            log_pending_beats(6U);
            beat_queue_clear(&g_beat_q);
            g_frontend_changed = 1U;
        } else {
            process_device_command(command);
        }
        if (cn121_read_reg(REG_CONFIG) != CN121_CONFIG_SENSING ||
            cn121_read_reg(REG_CHSET) != expected) {
            g_cn121_ready = 0U;
            device_set_error(&g_dev, DEV_ERR_CN121_FAILED);
            control_note("CN121 READBACK ERROR", now_ms);
        }
    }
}

static void process_standalone_controls(u32 now_ms)
{
    u8 raw = ecg_board_keys_read();
    if (raw == 3U) {
        /* A two-key chord must not stop and change gain in the same poll. */
        g_control.key[0] = (EcgButton){0};
        g_control.key[1] = (EcgButton){0};
        ecg_standalone_cancel_auto(&g_control);
        return;
    }
    u8 first = ecg_button_step(&g_control.key[0], raw & 1U, now_ms);
    u8 second = ecg_button_step(&g_control.key[1], raw & 2U, now_ms);
    if (raw) ecg_standalone_cancel_auto(&g_control);
    if (g_keys_ready && first == ECG_BUTTON_LONG) {
        ecg_standalone_cancel_auto(&g_control);
        process_device_command('t');
    } else if (g_keys_ready && first == ECG_BUTTON_SHORT) {
        process_device_command(ecg_recorder_is_recording(&g_rec) ? 't' : 'r');
    }
    if (g_keys_ready && second == ECG_BUTTON_SHORT)
        g_control.menu = (u8)((g_control.menu + 1U) % ECG_MENU_COUNT);
    if (g_keys_ready && second == ECG_BUTTON_LONG)
        apply_standalone_menu(g_control.menu, now_ms);
#if ECG_TOUCH_BUILD
    {
        int action = ecg_touch_poll(now_ms);
        if (action == TOUCH_RECORD)
            process_device_command(ecg_recorder_is_recording(&g_rec) ? 't' : 'r');
        else if (action >= TOUCH_GAIN_DOWN && action <= TOUCH_SAVE)
            apply_standalone_menu((u8)(action - TOUCH_GAIN_DOWN), now_ms);
    }
#endif
    if (ecg_standalone_auto_due(&g_control, &g_settings, now_ms))
        process_device_command('r');
}

int main(void)
{
    XTime t_start;
    XTime t_now;
    XTime t_last_print;
    XTime t_last_lcd;
    XTime t_last_overflow_print;
    XTime t_last_adc_diag;

    u64 total_read = 0ULL;
    u64 total_rpeaks = 0ULL;
    u64 total_ready_beats = 0ULL;
    u64 overflow_count = 0ULL;

    EcgLodEvent lod_event = {0U, 0U, 0U};
    XTime last_perf_print = 0;
    u32 adc_min = 255U;
    u32 adc_max = 0U;
    u32 adc_sum = 0U;
    u32 adc_count = 0U;
    u32 otr_count = 0U;
    u8 latest_sample_otr = 0U;
    u8 latest_sample_lod = 0U;
    u32 lod_count = 0U;
    u32 low_rail_count = 0U;
    u32 high_rail_count = 0U;
    u32 max_fifo_count = 0U;
    u64 estimated_lost_samples = 0ULL;

    u32 latest_rr_ms = 0U;
    u32 latest_hr_bpm = 0U;
    u64 latest_r_index = 0ULL;

    u8 latest_cls_valid = 0U;
    u8 latest_cls_id = ECG_CLASS_UNKNOWN;
    u8 latest_cls_conf = 0U;

    init_platform();
    xil_printf("[FW] %s\r\n", ECG_FIRMWARE_REVISION);

    if (cn121_setup_default(&g_cn121_status) != 0) {
        xil_printf("WARNING: CN121 init failed\r\n");
    } else {
        g_cn121_ready = 1U;
        xil_printf("CN121 init OK: CONFIG=0x%02x CHSET=0x%02x LOD=%d\r\n",
                   g_cn121_status.config,
                   g_cn121_status.chset,
                   g_cn121_status.lod);
    }

    cn121_show_menu();

    Xil_Out32(AD9280_STREAM_BASEADDR + AD_REG_DEBUG, 0x12345678U);
    Xil_Out32(AD9280_STREAM_BASEADDR + AD_REG_CLK_HALF_PERIOD, AD_DEFAULT_HALF_PERIOD);

    ad9280_stream_init();
    ad9280_stream_set_half_period(AD_DEFAULT_HALF_PERIOD);
    ad9280_stream_clear_fifo();
    ad9280_stream_clear_overflow();

    ecg_ring_init(&g_ring);
    ecg_filter_init(&g_filter);
    ecg_rpeak_init(&g_rdet);
    beat_queue_init(&g_beat_q);
    ecg_quality_init(&g_quality);

    device_state_init(&g_dev);

    update_cn121_snapshot();

    if (ecg_recorder_init(&g_rec) == 0) {
        u32 next_record_id;

        device_mark_sd_ready(&g_dev, 1U);
        if (ecg_recorder_get_next_record_id(&g_rec, 1U, &next_record_id) == 0) {
            g_dev.record_id = next_record_id - 1U;
        } else {
            device_set_error(&g_dev, DEV_ERR_SD_INIT_FAILED);
            xil_printf("[SD] no free record id\r\n");
        }
        xil_printf("[SD] recorder init OK\r\n");
    } else {
        device_mark_sd_ready(&g_dev, 0U);
        xil_printf("[SD] recorder init failed\r\n");
    }

    device_set_patient(&g_dev, "P0000", "UNKNOWN", 0, 'U');

    ecg_settings_defaults(&g_settings);
    g_settings_source = ECG_CFG_NO_SD;
    if (g_dev.sd_ready) {
        if (ecg_settings_load_sd(&g_settings, &g_settings_source) != 0)
            device_set_error(&g_dev, DEV_ERR_CONFIG_INVALID);
    } else {
        g_settings.auto_record = 0U;
    }
    if (g_cn121_ready) {
        u8 expected = ecg_settings_chset(&g_settings);
        cn121_set_chset(expected);
        if (cn121_read_reg(REG_CONFIG) != CN121_CONFIG_SENSING ||
            cn121_read_reg(REG_CHSET) != expected) g_cn121_ready = 0U;
        update_cn121_snapshot();
    }
    if (!g_cn121_ready) device_set_error(&g_dev, DEV_ERR_CN121_FAILED);
    if (ecg_board_keys_init(g_settings.key_profile) == 0) {
        g_keys_ready = g_settings.key_profile ? 1U : 0U;
    } else {
        device_set_error(&g_dev, DEV_ERR_CONFIG_INVALID);
        xil_printf("[KEY] requested profile refused; automatic record blocked\r\n");
    }
    xil_printf("[BOOT] cfg=%s gain=%u hp_x100=%u lp=%u keys=%u auto=%u limit_s=%lu\r\n",
        ecg_settings_source_name(g_settings_source), g_settings.gain, g_settings.hp_x100,
        g_settings.lp_hz, g_keys_ready, g_settings.auto_record,
        (unsigned long)g_settings.record_seconds);

    if (ecg_cnn_infer_init() != 0) {
        xil_printf("ECG CNN infer init failed\r\n");
        xil_printf("[CNN] disabled; acquisition/display/SD remain available\r\n");
    }

#if ECG_TOUCH_BUILD
    ecg_touch_init();
#endif

    if (ecg_infer_init() != 0) {
        xil_printf("WARNING: ecg_infer_init failed\r\n");
    } else {
        xil_printf("ECG infer init OK\r\n");
    }

    if (lcd_wave_init() != 0) {
        xil_printf("WARNING: lcd_wave_init failed\r\n");
    } else {
        xil_printf("LCD wave init OK\r\n");
#if ECG_WORKSTATION_BUILD
        lcd_port_draw_string(10, 22, "UDP STARTING - CONNECT ETHERNET",
                             LCD_COLOR_YELLOW, LCD_COLOR_BLACK);
        lcd_port_flush_rect(0, 16, LCD_WIDTH, 32);
#endif
    }

#if ECG_WORKSTATION_BUILD
    if (ecg_net_init() != 0) {
        xil_printf("[NET] init failed, continuing without Ethernet stream\r\n");
    }
#endif
    ad9280_stream_enable(1);
    ad9280_stream_dump_status();

    xil_printf("\r\n");
    xil_printf("Device commands:\r\n");
    xil_printf("  r : start record\r\n");
    xil_printf("  t : stop record\r\n");
    xil_printf("  p : pause record\r\n");
    xil_printf("  v : resume record\r\n");
    xil_printf("  d : device status\r\n");
    xil_printf("  c : clear error\r\n");
    xil_printf("\r\n");

    xil_printf("NOTE:\r\n");
    xil_printf("  Poor signal quality gates R-peak detection and CNN only.\r\n");
    xil_printf("  Acquisition, filtering, LCD, and SD recording continue.\r\n");
    xil_printf("  SQI OTR gate: %s (raw OTR is still recorded).\r\n",
               ECG_SQI_USE_OTR ? "ENABLED" : "DISABLED");
    xil_printf("  UART command reading is non-blocking now.\r\n");
    xil_printf("  Ethernet UDP v1: %s\r\n", ecg_net_is_ready() ? "READY" : "OFF");
    xil_printf("  Abnormal event rule: CNN class_id != ECG_CLASS_NORMAL(N).\r\n");
    xil_printf("\r\n");

    XTime_GetTime(&t_start);
    t_last_print = t_start;
    t_last_lcd = t_start;
    t_last_overflow_print = t_start;
    t_last_adc_diag = t_start;
    g_record_t0 = t_start;
    ecg_standalone_init(&g_control, (u32)ticks_to_ms(0, t_start),
        g_settings.auto_record && g_cn121_ready &&
        device_get_error(&g_dev) == DEV_ERR_NONE);

    while (1) {
        int n;
        int i;
        u32 fifo_before_read;

        process_device_uart_cmd();
        XTime_GetTime(&t_now);
        process_standalone_controls((u32)ticks_to_ms(0, t_now));
        if (g_frontend_changed) {
            latest_rr_ms = 0U;
            latest_hr_bpm = 0U;
            latest_cls_valid = 0U;
            latest_cls_id = ECG_CLASS_UNKNOWN;
            latest_cls_conf = 0U;
            g_frontend_changed = 0U;
        }
        ecg_net_poll((u32)ticks_to_ms(0, t_now));

        /*
         * 1. Read samples from PL FIFO.
         */
        fifo_before_read = ad9280_stream_get_fifo_count();
        if (fifo_before_read > max_fifo_count) {
            max_fifo_count = fifo_before_read;
        }
        n = ad9280_stream_read_available(g_samples, AD_READ_BURST_MAX);

        if (n < 0) {
            
            n = 0;
        } else if (n > AD_READ_BURST_MAX) {
            n = AD_READ_BURST_MAX;
        }

        for (i = 0; i < n; i++) {
            float raw;
            float filt;
            float filt_old;
            float qrs;
            float env;
            u64 gidx;
            EcgQualityState quality_before;
            u8 quality_reason_before;
            u8 quality_warning_before;
            u8 quality_state_changed;
            u8 quality_report_changed;

            if (!g_samples[i].valid) {
                continue;
            }

            total_read++;
            latest_sample_otr = g_samples[i].otr ? 1U : 0U;
            latest_sample_lod = g_samples[i].lod ? 1U : 0U;

            if ((u32)g_samples[i].adc < adc_min) {
                adc_min = g_samples[i].adc;
            }

            if ((u32)g_samples[i].adc > adc_max) {
                adc_max = g_samples[i].adc;
            }

            adc_sum += g_samples[i].adc;
            adc_count++;

            if (g_samples[i].otr) {
                otr_count++;
            }

            if (g_samples[i].lod) {
                lod_count++;
            }

            if (g_samples[i].adc <= 2U) {
                low_rail_count++;
            }

            if (g_samples[i].adc >= 253U) {
                high_rail_count++;
            }

            raw = ad9280_sample_to_float_centered(g_samples[i].adc);

            filt = ecg_filter_step(&g_filter, raw);
            filt_old = ecg_filter_get_legacy(&g_filter);
            qrs = ecg_filter_get_qrs(&g_filter);
            env = ecg_filter_get_env(&g_filter);

            ecg_ring_push(&g_ring, raw, filt);

            gidx = g_ring.total_samples - 1ULL;

            quality_before = g_quality.state;
            quality_reason_before = g_quality.reason;
            quality_warning_before = g_quality.warning;
            ecg_quality_step(&g_quality,
                             g_samples[i].adc,
                             raw,
                             g_samples[i].lod ? 1U : 0U,
                             (ECG_SQI_USE_OTR && g_samples[i].otr) ? 1U : 0U);
            quality_state_changed =
                (quality_before != g_quality.state) ? 1U : 0U;
            quality_report_changed =
                (quality_state_changed ||
                 (quality_reason_before != g_quality.reason) ||
                 (quality_warning_before != g_quality.warning)) ? 1U : 0U;

            {
                u8 net_flags = 0U;
                u32 net_ts_ms = (u32)((gidx * 1000ULL) / (u64)ECG_FS_HZ);

                if (g_samples[i].lod) {
                    net_flags |= ECG_NET_FLAG_LOD;
                }
                if (g_samples[i].otr) {
                    net_flags |= ECG_NET_FLAG_OTR;
                }

                ecg_net_send_wave_sample(gidx,
                                         net_ts_ms,
                                         ecg_float_to_s16_scaled(filt),
                                         latest_cls_valid ? latest_cls_id : ECG_CLASS_UNKNOWN,
                                         net_flags);
            }

            
            if ((device_get_state(&g_dev) == DEV_STATE_RECORDING) &&
                ecg_recorder_is_recording(&g_rec)) {
                EcgRecordSample rs;

                g_record_sample_count++;

                rs.sample_index = g_record_sample_count;
                rs.timestamp_ms = record_timestamp_ms(g_record_sample_count);

                
                rs.raw_adc = g_samples[i].adc;

                
                rs.filtered = ecg_float_to_s16_scaled(filt);
                rs.filtered_old = ecg_float_to_s16_scaled(filt_old);
                rs.qrs = ecg_float_to_s16_scaled(qrs);
                rs.env = ecg_float_to_s16_scaled(env);
                rs.sqi_state = (u8)g_quality.state;
                rs.sqi_reason = g_quality.reason;
                rs.sqi_warning = g_quality.warning;

                
                rs.lod = g_samples[i].lod ? 1U : 0U;
                rs.otr = g_samples[i].otr ? 1U : 0U;

                XTime write_start = profile_start();
                int write_rc = ecg_recorder_write_sample(&g_rec, &rs);
                profile_end(&g_sd_cost, write_start);
                if (write_rc != 0) {
                    ecg_recorder_stop(&g_rec);
                    device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
                    xil_printf("[SD] write sample failed\r\n");
                }
            }

            if (ecg_lod_event_step(&lod_event, latest_sample_lod)) {
                xil_printf("LOD stable: %u\r\n", lod_event.stable);
                if (device_get_state(&g_dev) == DEV_STATE_RECORDING)
                    log_diagnostic("LOD", g_record_sample_count, lod_event.stable);
            }

            if (quality_state_changed) {
                log_pending_beats(1U);
                beat_queue_clear(&g_beat_q);
                ecg_rpeak_init(&g_rdet);
                latest_cls_valid = 0U;
                latest_cls_id = ECG_CLASS_UNKNOWN;
                latest_cls_conf = 0U;
            }

            if (quality_report_changed) {
                xil_printf("[SQI] %s score=%u reason=%s/0x%02x warning=%s/0x%02x range=%d mean_diff=%d mean_adc=%u headroom=%u%% lod=%u%% otr=%u%% clip=%u%% flat=%u%% spike=%u%%\r\n",
                           ecg_quality_state_name(g_quality.state),
                           g_quality.score,
                           ecg_quality_reason_name(g_quality.reason),
                           g_quality.reason,
                           ecg_quality_warning_name(g_quality.warning),
                           g_quality.warning,
                           (int)(g_quality.latest_range * 1000.0f),
                           (int)(g_quality.latest_mean_abs_diff * 1000.0f),
                           g_quality.latest_mean_adc,
                           g_quality.latest_headroom_percent,
                           g_quality.latest_lod_percent,
                           g_quality.latest_otr_percent,
                           g_quality.latest_clip_percent,
                           g_quality.latest_flat_percent,
                           g_quality.latest_spike_percent);

                if ((g_quality.state != ECG_QUALITY_RECOVERING) &&
                    (device_get_state(&g_dev) == DEV_STATE_RECORDING) &&
                    ecg_recorder_is_recording(&g_rec)) {
                    (void)ecg_recorder_write_quality_event(
                        &g_rec,
                        g_record_sample_count,
                        record_timestamp_ms(g_record_sample_count),
                        ecg_quality_is_good(&g_quality),
                        g_quality.reason);
                }
            }

            
            if (g_cn121_ready && ecg_quality_is_good(&g_quality) &&
                ecg_rpeak_step(&g_rdet, qrs, env, gidx)) {
                total_rpeaks++;

                latest_r_index = g_rdet.detected_r_index;
                lcd_wave_mark_rpeak(latest_r_index);
                latest_rr_ms = ecg_rpeak_last_rr_ms(&g_rdet);
                latest_hr_bpm = ecg_rpeak_last_hr_bpm(&g_rdet);

                if (!beat_queue_add(&g_beat_q,
                               g_rdet.detected_r_index,
                               g_rdet.detected_prev_r_index))
                    log_skipped_beat(g_rdet.detected_r_index, 3U);

                
                if ((device_get_state(&g_dev) == DEV_STATE_RECORDING) &&
                    ecg_recorder_is_recording(&g_rec)) {
                    EcgRecordEvent ev;
                    u64 record_r_index;

                    if (global_to_record_index(latest_r_index,
                                               &record_r_index)) {
                        ev.sample_index = record_r_index;
                        ev.timestamp_ms = record_timestamp_ms(record_r_index);
                        ev.rr_ms = latest_rr_ms;
                        ev.hr_bpm = latest_hr_bpm;
                        ev.class_id = ECG_CLASS_INVALID;
                        ev.confidence = 0U;

                        if (ecg_recorder_write_rpeak_event(&g_rec, &ev) != 0) {
                            ecg_recorder_stop(&g_rec);
                            device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
                            xil_printf("[SD] write rpeak event failed\r\n");
                        }
                    }
                }
            }
            if (ecg_recorder_is_recording(&g_rec) &&
                ecg_standalone_stop_due(&g_settings, g_record_sample_count, ECG_FS_HZ)) {
                process_device_command('t');
            }
        }

        
        if (g_cn121_ready && ecg_quality_is_good(&g_quality)) {
            beat_queue_update_ready(&g_beat_q, g_ring.total_samples);
        } else {
            beat_queue_clear(&g_beat_q);
        }

        {
            XTime service_start = profile_start();
            if (g_inference_active) {
                u8 cnn_cls = ECG_CLASS_UNKNOWN;
                int rc = ecg_cnn_infer_poll(&cnn_cls);
                BeatTask *t = &g_active_beat;
                if (rc != 0) {
                    profile_end(&g_cnn_cost, g_inference_started);
                    g_inference_active = 0U;
                }
                if (rc == 1 && !g_inference_discard && ecg_quality_is_good(&g_quality)) {
                    latest_cls_valid = 1U;
                    latest_cls_id = cnn_cls;
                    latest_cls_conf = ECG_CONFIDENCE_UNAVAILABLE;

                    if ((device_get_state(&g_dev) == DEV_STATE_RECORDING) &&
                        ecg_recorder_is_recording(&g_rec)) {
                        EcgRecordEvent ev;
                        u64 record_r_index;

                        if (global_to_record_index(t->r_index,
                                                   &record_r_index)) {
                            ev.sample_index = record_r_index;
                            ev.timestamp_ms = record_timestamp_ms(record_r_index);
                            ev.rr_ms = t->rr_ms;
                            ev.hr_bpm = t->hr_bpm;
                            ev.class_id = latest_cls_id;
                            ev.confidence = latest_cls_conf;

                            if (ecg_recorder_write_cnn_event(&g_rec, &ev) != 0) {
                                ecg_recorder_stop(&g_rec);
                                device_set_error(&g_dev, DEV_ERR_SD_WRITE_FAILED);
                                xil_printf("[SD] write cnn event failed\r\n");
                            }
                        }
                    }

                    ecg_net_send_cnn_event(t->r_index,
                                           (u32)((t->r_index * 1000ULL) / (u64)ECG_FS_HZ),
                                           t->rr_ms,
                                           t->hr_bpm,
                                           latest_cls_id,
                                           latest_cls_conf);
                } else if (rc != 0) {
                    latest_cls_valid = 0U;
                    latest_cls_id = ECG_CLASS_UNKNOWN;
                    latest_cls_conf = 0U;

                    if (!g_inference_discard) log_skipped_beat(t->r_index, 5U);
                }
            }
            if (!g_inference_active && g_cn121_ready && ecg_quality_is_good(&g_quality)) {
                BeatTask *t = beat_queue_get_next_ready(&g_beat_q);
                if (t != 0) {
                    int rc;
                    total_ready_beats++;
                    rc = ecg_ring_copy_filt_beat_window(&g_ring, t->r_index,
                                                       g_cnn_beat_buf, ECG_CNN_FLOAT_LEN);
                    if (!rc) {
                        log_skipped_beat(t->r_index, 4U);
                    } else if (ecg_cnn_infer_available()) {
                        g_inference_started = profile_start();
                        rc = ecg_cnn_infer_start(g_cnn_beat_buf, t->rr_samples);
                        if (rc == 0) {
                            /* Own a copy: SQI may clear/reuse the queue slot. */
                            g_active_beat = *t;
                            g_inference_discard = 0U;
                            g_inference_active = 1U;
                        } else {
                            log_skipped_beat(t->r_index, 5U);
                        }
                    } else {
                        log_skipped_beat(t->r_index, 5U);
                    }
                    /* Ownership transferred or failure recorded; release slot. */
                    t->state = BEAT_EMPTY;
                }
            }
            profile_end(&g_cnn_service_cost, service_start);
        }

        
        if (ad9280_stream_has_overflow()) {
            overflow_count++;
            ad9280_stream_clear_overflow();

            XTime_GetTime(&t_now);

            
            if (ticks_to_ms(t_last_overflow_print, t_now) >= 1000U) {
                xil_printf("WARNING: FIFO overflow count=%lu, total_read=%lu, rec_samples=%lu\r\n",
                           (unsigned long)overflow_count,
                           (unsigned long)total_read,
                           (unsigned long)g_record_sample_count);
                t_last_overflow_print = t_now;
            }
        }

        
        XTime_GetTime(&t_now);

        
        if (ticks_to_ms(t_last_lcd, t_now) >= LCD_REFRESH_MS) {
            LcdEcgInfo info;
            u32 st = ad9280_stream_get_status();

            info.rr_ms = latest_rr_ms;
            info.hr_bpm = latest_hr_bpm;

            
            /* Same sampled status as CSV, not the one-second OTR accumulator. */
            info.lod = latest_sample_lod;
            info.otr = latest_sample_otr;

            
            info.overflow = (st & AD_ST_OVERFLOW) ? 1U : 0U;
            info.running = (st & AD_ST_RUNNING) ? 1U : 0U;

            info.r_index = latest_r_index;
            info.total_samples = g_ring.total_samples;

            info.cls_valid = latest_cls_valid;
            info.cls_id = latest_cls_id;
            info.cls_conf = latest_cls_conf;
            info.gain = g_dev.cn121.gain;
            info.hp_x100 = g_dev.cn121.hp_x100;
            info.lp_hz = g_dev.cn121.lp_hz;
            info.sd_ready = g_dev.sd_ready;
            info.recording = !ecg_recorder_is_recording(&g_rec) ? 0U :
                device_get_state(&g_dev) == DEV_STATE_PAUSED ? 2U : 1U;
            info.files_closed = g_files_closed;
            info.error = (u8)device_get_error(&g_dev);
            info.frontend_ready = g_cn121_ready;
            info.record_id = g_dev.record_id;
            info.elapsed_s = (u32)(g_record_sample_count / ECG_FS_HZ);
            info.cfg_source = ecg_settings_source_name(g_settings_source);
            info.keys_ready = g_keys_ready;
            info.net_ready = ecg_net_is_ready();
            info.menu = g_control.menu;
            info.touch_hint = ECG_TOUCH_BUILD ? ecg_touch_hint() : "USE PS KEYS";
            info.note = g_control_note && (u32)((u32)ticks_to_ms(0, t_now) - g_control_note_ms) < 3000U
                ? g_control_note : 0;

            {
                XTime draw_start = profile_start();
                lcd_wave_draw(&g_ring, &info);
                profile_end(&g_lcd_cost, draw_start);
            }

            t_last_lcd = t_now;
        }

        
        if (ticks_to_ms(t_last_adc_diag, t_now) >= ECG_ADC_DIAG_PRINT_MS) {
            u64 before = ad9280_stream_get_sample_counter();
            u32 fifo_count = ad9280_stream_get_fifo_count();
            u64 produced = ad9280_stream_get_sample_counter();
            u64 accounted = total_read + (u64)fifo_count;
            if (fifo_count > max_fifo_count) max_fifo_count = fifo_count;

            /* Reject a sample arriving between these separate AXI reads. */
            if ((before == produced) && (produced > accounted) &&
                ((produced - accounted) > estimated_lost_samples)) {
                estimated_lost_samples = produced - accounted;
            }

            if (ticks_to_ms(last_perf_print, t_now) >= 5000U) {
                print_cost("sd_sample_write", &g_sd_cost);
                print_cost("cnn_latency", &g_cnn_cost);
                print_cost("cnn_service", &g_cnn_service_cost);
                print_cost("lcd_draw", &g_lcd_cost);
                xil_printf("[ADC_SNAPSHOT] valid=%u\r\n", before == produced);
                last_perf_print = t_now;
            }

            if (adc_count > 0U) {
                u32 mean_x10 = (adc_sum * 10U) / adc_count;
                u32 low_rail_x10 = (low_rail_count * 1000U) / adc_count;
                u32 high_rail_x10 = (high_rail_count * 1000U) / adc_count;

                xil_printf("[ADC] n=%lu min=%lu max=%lu mean=%lu.%lu rail_lo=%lu.%lu%% rail_hi=%lu.%lu%% lod=%lu otr=%lu fifo=%lu/%lu produced=%lu read=%lu lost_est=%lu ovr=%lu\r\n",
                           (unsigned long)adc_count,
                           (unsigned long)adc_min,
                           (unsigned long)adc_max,
                           (unsigned long)(mean_x10 / 10U),
                           (unsigned long)(mean_x10 % 10U),
                           (unsigned long)(low_rail_x10 / 10U),
                           (unsigned long)(low_rail_x10 % 10U),
                           (unsigned long)(high_rail_x10 / 10U),
                           (unsigned long)(high_rail_x10 % 10U),
                           (unsigned long)lod_count,
                           (unsigned long)otr_count,
                           (unsigned long)fifo_count,
                           (unsigned long)max_fifo_count,
                           (unsigned long)produced,
                           (unsigned long)total_read,
                           (unsigned long)estimated_lost_samples,
                           (unsigned long)overflow_count);
            } else {
                xil_printf("[ADC] no samples fifo=%lu produced=%lu read=%lu lost_est=%lu ovr=%lu\r\n",
                           (unsigned long)fifo_count,
                           (unsigned long)produced,
                           (unsigned long)total_read,
                           (unsigned long)estimated_lost_samples,
                           (unsigned long)overflow_count);
            }

            adc_min = 255U;
            adc_max = 0U;
            adc_sum = 0U;
            adc_count = 0U;
            otr_count = 0U;
            lod_count = 0U;
            low_rail_count = 0U;
            high_rail_count = 0U;
            max_fifo_count = fifo_count;

            t_last_adc_diag = t_now;
        }

        if (ticks_to_ms(t_last_print, t_now) >= PRINT_EVERY_MS) {

            beat_queue_gc(&g_beat_q, ecg_ring_oldest_index(&g_ring));

            t_last_print = t_now;
        }

        
        /* usleep(100); */
    }

    cleanup_platform();
    return 0;
}
