/* Exercise the actual main.c command dispatcher with hardware/SD test doubles. */
#define ECG_TOUCH_BUILD 1
#define main ecg_firmware_main_not_run
#include "../../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/main.c"
#undef main
#include <assert.h>
#include <stdarg.h>
#include <string.h>
static XTime clock_ticks;
static int start_calls, stop_calls, fail_start, fail_stop, frontend_calls;
static int save_calls, fail_save;
static EcgSettings saved_settings;
static u32 action_ms;
static int touch_action;
static u8 key_bits, register_chset = 0x0C;
void XTime_GetTime(XTime *time) { *time = clock_ticks; }
void xil_printf(const char8 *format, ...) { (void)format; }
void ad9280_stream_clear_overflow(void) { }
int ecg_recorder_get_next_record_id(EcgRecorder *rec, u32 first, u32 *next)
{ (void)rec; *next = first; return 0; }
int ecg_recorder_start(EcgRecorder *rec, const DeviceContext *dev, u32 fs)
{ (void)dev; (void)fs; ++start_calls; rec->recording = !fail_start; return fail_start ? -1 : 0; }
int ecg_recorder_stop(EcgRecorder *rec)
{ ++stop_calls; rec->recording = 0; return fail_stop ? -1 : 0; }
u8 ecg_recorder_is_recording(const EcgRecorder *rec) { return rec->recording; }
int ecg_recorder_write_diagnostic(EcgRecorder *rec, const char *type, u64 idx, u32 ms, u8 detail)
{ (void)rec; (void)type; (void)idx; (void)ms; (void)detail; return 0; }
u8 ecg_board_keys_read(void) { return key_bits; }
int ecg_touch_poll(u32 now_ms)
{ int action = touch_action; (void)now_ms; touch_action = TOUCH_NONE; return action; }
const char *ecg_touch_hint(void) { return 0; }
u8 cn121_read_reg(u8 address) { return address == REG_CONFIG ? 0 : register_chset; }
u8 cn121_get_lod(void) { return 0; }
u16 cn121_gain_code_to_value(u8 code) { return 60U * (code + 1U); }
void cn121_set_chset(u8 value) { register_chset = value; ++frontend_calls; }
int cn121_handle_uart_cmd(u8 cmd)
{
    ++frontend_calls;
    if (cmd == '-') register_chset = (register_chset & 0xF0U) | ((register_chset & 15U) - 1U);
    if (cmd == 'g') register_chset = (register_chset & 0xF0U) | ((register_chset + 1U) & 15U);
    if (cmd == 'h') register_chset ^= 0x40U;
    return 1;
}
void ecg_quality_init(EcgSignalQuality *q) { memset(q, 0, sizeof(*q)); }
const char *ecg_quality_state_name(EcgQualityState state) { (void)state; return "MOCK"; }
const char *ecg_quality_reason_name(u8 reason) { (void)reason; return "MOCK"; }
const char *ecg_quality_warning_name(u8 warning) { (void)warning; return "MOCK"; }
void ecg_rpeak_init(RPeakDetector *q) { memset(q, 0, sizeof(*q)); }
void ecg_filter_init(EcgFilter *q) { memset(q, 0, sizeof(*q)); }
void beat_queue_clear(BeatQueue *q) { memset(q, 0, sizeof(*q)); }
int ecg_settings_save_sd(const EcgSettings *s)
{ ++save_calls; saved_settings = *s; return fail_save ? -1 : 0; }

static void reset(void)
{
    memset(&g_rec, 0, sizeof(g_rec));
    memset(&g_beat_q, 0, sizeof(g_beat_q));
    device_state_init(&g_dev);
    device_mark_sd_ready(&g_dev, 1);
    ecg_settings_defaults(&g_settings);
    ecg_standalone_init(&g_control, 0, 1);
    g_settings_source = ECG_CFG_SD;
    g_cn121_ready = g_keys_ready = g_files_closed = 1;
    g_inference_active = key_bits = 0;
    fail_start = fail_stop = start_calls = stop_calls = frontend_calls = 0;
    save_calls = fail_save = 0;
    action_ms = 0;
    touch_action = TOUCH_NONE;
    g_control_note = 0;
    clock_ticks = 0;
    register_chset = 0x0C;
}

static void key_action(u8 mask, u8 long_press)
{
    key_bits = 0;
    process_standalone_controls(action_ms += 31);
    key_bits = mask;
    process_standalone_controls(action_ms += 50);
    process_standalone_controls(action_ms += 30);
    process_standalone_controls(action_ms += long_press ? 1500 : 50);
    key_bits = 0;
    process_standalone_controls(action_ms += 1);
    process_standalone_controls(action_ms += 30);
}

int main(void)
{
    reset();
    g_ring.total_samples = 100;
    process_standalone_controls(2999);
    assert(!start_calls);
    process_standalone_controls(3000);
    assert(start_calls == 1 && g_rec.recording && !g_control.auto_pending);
    process_device_command('c');
    assert(g_dev.state == DEV_STATE_RECORDING && g_rec.recording);
    assert(g_record_global_start_index == 100 && g_record_sample_count == 0 && !g_files_closed);
    process_standalone_controls(9000);
    assert(start_calls == 1);
    process_device_command('t');
    assert(stop_calls == 1 && !g_rec.recording && g_files_closed && g_dev.state == DEV_STATE_IDLE);
    process_standalone_controls(99999);
    assert(start_calls == 1);
    reset();
    process_device_command('t');
    process_standalone_controls(99999);
    assert(!start_calls && !g_control.auto_pending);
    reset();
    fail_start = 1;
    process_standalone_controls(3000);
    process_standalone_controls(6000);
    assert(start_calls == 1 && g_dev.state == DEV_STATE_ERROR && !g_rec.recording);
    reset();
    g_settings_source = ECG_CFG_INVALID;
    process_device_command('r');
    assert(!start_calls);
    reset();
    g_cn121_ready = 0;
    process_device_command('r');
    assert(!start_calls);
    reset();
    process_device_command('r');
    fail_stop = 1;
    process_device_command('t');
    assert(!g_files_closed && g_dev.state == DEV_STATE_ERROR && !g_rec.recording);
    reset();
    process_device_command('r');
    g_control.key[0].armed = g_control.key[1].armed = 1;
    key_bits = 2;
    process_standalone_controls(1);
    process_standalone_controls(31);
    process_standalone_controls(1531);
    assert(!frontend_calls); /* Recording configuration is locked. */
    process_device_command('t');
    key_bits = 0;
    process_standalone_controls(1600);
    process_standalone_controls(1630);
    key_bits = 2;
    process_standalone_controls(1700);
    process_standalone_controls(1730);
    process_standalone_controls(3230);
    assert(frontend_calls == 1 && g_dev.cn121.gain == 720 && g_frontend_changed);
    reset();
    key_bits = 3;
    process_standalone_controls(1);
    process_standalone_controls(1600);
    key_bits = 0;
    process_standalone_controls(1700);
    process_standalone_controls(1730);
    process_standalone_controls(99999);
    assert(!start_calls && !frontend_calls && !g_control.auto_pending);
    reset();
    key_action(1, 0);
    assert(start_calls == 1 && g_rec.recording && !g_control.auto_pending);
    key_action(1, 1);
    assert(stop_calls == 1 && !g_rec.recording && g_files_closed && start_calls == 1);
    key_action(1, 0);
    assert(start_calls == 2 && g_rec.recording);
    key_action(1, 0);
    assert(stop_calls == 2 && !g_rec.recording);
    key_action(2, 0);
    assert(g_control.menu == ECG_MENU_GAIN_UP);
    key_action(2, 1);
    assert(g_dev.cn121.gain == 840 && g_cn121_ready);
    key_action(2, 0);
    assert(g_control.menu == ECG_MENU_HP);
    key_action(2, 1);
    assert(g_dev.cn121.hp_x100 == 5 && g_cn121_ready);
    key_action(2, 0);
    assert(g_control.menu == ECG_MENU_LP);
    key_action(2, 1);
    assert(g_dev.cn121.lp_hz == 55 && g_cn121_ready);
    key_action(2, 0);
    assert(g_control.menu == ECG_MENU_SAVE);
    key_action(2, 1);
    assert(save_calls == 1 && saved_settings.gain == 840 && saved_settings.hp_x100 == 5);
    assert(saved_settings.lp_hz == 55 && saved_settings.key_profile == 1);
    assert(!strcmp(g_control_note, "SETTINGS SAVED"));
    fail_save = 1;
    key_action(2, 1);
    assert(save_calls == 2 && !strcmp(g_control_note, "SAVE FAILED"));
    reset();
    g_settings.auto_record = 0;
    ecg_standalone_cancel_auto(&g_control);
    g_keys_ready = 0;
    key_action(1, 0);
    key_action(2, 1);
    assert(!start_calls && !frontend_calls && !save_calls);
    reset();
    touch_action = TOUCH_RECORD;
    process_standalone_controls(10);
    assert(g_rec.recording && start_calls == 1);
    touch_action = TOUCH_GAIN_DOWN;
    process_standalone_controls(20);
    assert(frontend_calls == 0);
    touch_action = TOUCH_RECORD;
    process_standalone_controls(30);
    assert(!g_rec.recording && stop_calls == 1);
    touch_action = TOUCH_GAIN_DOWN;
    process_standalone_controls(40);
    assert(g_dev.cn121.gain == 720 && frontend_calls == 1);
    puts("PASS main integration: real autostart, start/stop, invalid-config/frontend blocks, failures, no retry, recording config lock, idle gain change");
    puts("PASS Navigator menu: key start/stop/restart, long-release exclusion, gain/HP/LP, save success/failure, disabled keys");
    puts("PASS touch routing: shared recording, configuration interlock and frontend readback");
    return 0;
}
