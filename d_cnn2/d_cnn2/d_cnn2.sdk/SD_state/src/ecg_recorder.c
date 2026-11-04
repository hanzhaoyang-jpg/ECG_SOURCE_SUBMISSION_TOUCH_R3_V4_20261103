#include "ecg_recorder.h"
#include "ecg_config.h"
#include "ecg_result.h"
#include "ecg_standalone.h"
#include "ff.h"
#include "xil_printf.h"

#include <string.h>
#include <stdio.h>

#define ECG_REC_DIR        "0:/ECG"
#define ECG_REC_SYNC_EVERY 128U
#define ECG_SAMPLE_HEADER   "sample_index,timestamp_ms,raw_adc,filtered,filtered_old,qrs,env,sqi_state,sqi_reason,sqi_warning,lod,otr\r\n"

static FATFS g_fatfs;
static FIL   g_sample_fil;
static FIL   g_event_fil;
static FIL   g_meta_fil;



static int make_ecg_dir(void)
{
    FRESULT fr;

    fr = f_mkdir(ECG_REC_DIR);

    if (fr == FR_OK || fr == FR_EXIST) {
        return 0;
    }

    return -1;
}

static void make_sample_filename(EcgRecorder *rec,
                                 u32 record_id,
                                 u32 segment_index)
{
    if (segment_index == 0U) {
        sprintf(rec->sample_filename,
                "0:/ECG/REC%04lu.CSV",
                (unsigned long)record_id);
    } else {
        sprintf(rec->sample_filename,
                "0:/ECG/REC%04lu_S%03lu.CSV",
                (unsigned long)record_id,
                (unsigned long)segment_index);
    }
}

static void make_record_filenames(EcgRecorder *rec, u32 record_id)
{
    make_sample_filename(rec, record_id, 0U);
    sprintf(rec->event_filename,  "0:/ECG/REC%04lu_EVT.CSV",  (unsigned long)record_id);
    sprintf(rec->meta_filename,   "0:/ECG/REC%04lu_META.TXT", (unsigned long)record_id);
}

static int record_id_is_available(EcgRecorder *rec, u32 record_id)
{
    FILINFO info;
    FRESULT fr;

    make_record_filenames(rec, record_id);

    fr = f_stat(rec->sample_filename, &info);
    if (fr == FR_OK) {
        return 0;
    }
    if (fr != FR_NO_FILE) {
        return -1;
    }

    fr = f_stat(rec->event_filename, &info);
    if (fr == FR_OK) {
        return 0;
    }
    if (fr != FR_NO_FILE) {
        return -1;
    }

    fr = f_stat(rec->meta_filename, &info);
    if (fr == FR_OK) {
        return 0;
    }
    if (fr != FR_NO_FILE) {
        return -1;
    }

    return 1;
}

static void remove_record_files(const EcgRecorder *rec)
{
    if (!rec) {
        return;
    }

    (void)f_unlink(rec->sample_filename);
    (void)f_unlink(rec->event_filename);
    (void)f_unlink(rec->meta_filename);
}

static int file_write_buf(FIL *fil, const char *buf, UINT len)
{
    FRESULT fr;
    UINT bw = 0U;

    if ((fil == 0) || (buf == 0)) {
        return -1;
    }

    fr = f_write(fil, buf, len, &bw);
    if ((fr != FR_OK) || (bw != len)) {
        return -2;
    }

    return 0;
}

static int file_write_str(FIL *fil, const char *s)
{
    if (s == 0) {
        return -1;
    }

    return file_write_buf(fil, s, (UINT)strlen(s));
}

static int open_sample_segment(EcgRecorder *rec, u32 segment_index)
{
    FRESULT fr;

    make_sample_filename(rec, rec->record_id, segment_index);
    fr = f_open(&g_sample_fil, rec->sample_filename, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK) {
        xil_printf("[SD] open sample segment %lu failed: %d\r\n",
                   (unsigned long)segment_index,
                   fr);
        return -1;
    }

    if (file_write_str(&g_sample_fil, ECG_SAMPLE_HEADER) != 0) {
        xil_printf("[SD] write sample segment header failed\r\n");
        f_close(&g_sample_fil);
        (void)f_unlink(rec->sample_filename);
        return -2;
    }

    rec->sample_file = &g_sample_fil;
    rec->sample_segment_index = segment_index;
    rec->sample_segment_write_count = 0U;
    return 0;
}

static int rotate_sample_segment(EcgRecorder *rec)
{
    FRESULT fr;
    u32 next_segment;

    if ((rec == 0) || (rec->sample_file == 0)) {
        return -1;
    }

    fr = f_sync(&g_sample_fil);
    if (fr != FR_OK) {
        xil_printf("[SD] sample segment f_sync failed: %d\r\n", fr);
        return -2;
    }

    fr = f_close(&g_sample_fil);
    rec->sample_file = 0;
    if (fr != FR_OK) {
        xil_printf("[SD] sample segment f_close failed: %d\r\n", fr);
        return -3;
    }

    next_segment = rec->sample_segment_index + 1U;
    if (open_sample_segment(rec, next_segment) != 0) {
        return -4;
    }

    xil_printf("[SD] sample segment start: %s\r\n", rec->sample_filename);
    return 0;
}

static char *append_char(char *p, char *end, char c)
{
    if (p < end) {
        *p++ = c;
    }

    return p;
}

static char *append_str(char *p, char *end, const char *s)
{
    if (s == 0) {
        return p;
    }

    while ((*s != '\0') && (p < end)) {
        *p++ = *s++;
    }

    return p;
}

static char *append_u32_dec(char *p, char *end, u32 v)
{
    char tmp[10];
    int n = 0;

    if (v == 0U) {
        return append_char(p, end, '0');
    }

    while ((v > 0U) && (n < (int)sizeof(tmp))) {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    }

    while (n > 0) {
        p = append_char(p, end, tmp[--n]);
    }

    return p;
}
static char *append_s32_dec(char *p, char *end, s32 v)
{
    if (v < 0) {
        p = append_char(p, end, '-');

        
        if (v == (s32)0x80000000) {
            p = append_str(p, end, "2147483648");
            return p;
        }

        v = -v;
    }

    return append_u32_dec(p, end, (u32)v);
}
static char *append_u64_dec(char *p, char *end, u64 v)
{
    char tmp[20];
    int n = 0;

    if (v == 0ULL) {
        return append_char(p, end, '0');
    }

    while ((v > 0ULL) && (n < (int)sizeof(tmp))) {
        tmp[n++] = (char)('0' + (u32)(v % 10ULL));
        v /= 10ULL;
    }

    while (n > 0) {
        p = append_char(p, end, tmp[--n]);
    }

    return p;
}

static char hex_digit(u8 v)
{
    v &= 0x0FU;

    if (v < 10U) {
        return (char)('0' + v);
    }

    return (char)('A' + (v - 10U));
}

static char *append_hex2(char *p, char *end, u8 v)
{
    p = append_char(p, end, '0');
    p = append_char(p, end, 'x');
    p = append_char(p, end, hex_digit((u8)(v >> 4)));
    p = append_char(p, end, hex_digit(v));

    return p;
}

static int write_meta_str(FIL *fil, const char *key, const char *value)
{
    char line[192];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_str(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

static int write_meta_char(FIL *fil, const char *key, char value)
{
    char line[64];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_char(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

static int write_meta_u32(FIL *fil, const char *key, u32 value)
{
    char line[64];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_u32_dec(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

static int write_meta_s32(FIL *fil, const char *key, s32 value)
{
    char line[64];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_s32_dec(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

static int write_meta_u64(FIL *fil, const char *key, u64 value)
{
    char line[80];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_u64_dec(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

static int write_meta_hex8(FIL *fil, const char *key, u8 value)
{
    char line[64];
    char *p = line;
    char *end = line + sizeof(line);

    p = append_str(p, end, key);
    p = append_char(p, end, '=');
    p = append_hex2(p, end, value);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -1;
    }

    return file_write_buf(fil, line, (UINT)(p - line));
}

int ecg_recorder_init(EcgRecorder *rec)
{
    FRESULT fr;

    if (!rec) {
        return -1;
    }

    memset(rec, 0, sizeof(EcgRecorder));

    fr = f_mount(&g_fatfs, "0:/", 1);
    if (fr != FR_OK) {
        xil_printf("[SD] f_mount failed: %d\r\n", fr);
        rec->initialized = 0;
        return -2;
    }

    if (make_ecg_dir() != 0) {
        xil_printf("[SD] mkdir ECG failed\r\n");
        rec->initialized = 0;
        return -3;
    }

    rec->initialized = 1;
    rec->recording = 0;
    rec->sample_write_count = 0;
    rec->event_write_count = 0;
    rec->sample_segment_index = 0U;
    rec->sample_segment_write_count = 0U;

    xil_printf("[SD] recorder init OK\r\n");

    return 0;
}

int ecg_recorder_get_next_record_id(EcgRecorder *rec,
                                    u32 start_id,
                                    u32 *next_id)
{
    u32 id;

    if (!rec || !next_id || !rec->initialized) {
        return -1;
    }

    id = (start_id == 0U) ? 1U : start_id;

    while (id != 0U) {
        int available = record_id_is_available(rec, id);

        if (available > 0) {
            *next_id = id;
            return 0;
        }
        if (available < 0) {
            return -2;
        }
        id++;
    }

    return -3;
}

int ecg_recorder_start(EcgRecorder *rec,
                       const DeviceContext *dev,
                       u32 fs_hz)
{
    FRESULT fr;
    int rc;

    if (!rec || !dev) {
        return -1;
    }

    if (!rec->initialized) {
        return -2;
    }

    if (rec->recording) {
        return 0;
    }

    rec->record_id = dev->record_id;

    make_record_filenames(rec, rec->record_id);

    if (open_sample_segment(rec, 0U) != 0) {
        return -3;
    }

    fr = f_open(&g_event_fil, rec->event_filename, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK) {
        xil_printf("[SD] open event file failed: %d\r\n", fr);
        f_close(&g_sample_fil);
        remove_record_files(rec);
        return -4;
    }

    fr = f_open(&g_meta_fil, rec->meta_filename, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK) {
        xil_printf("[SD] open meta file failed: %d\r\n", fr);
        f_close(&g_event_fil);
        f_close(&g_sample_fil);
        remove_record_files(rec);
        return -5;
    }

    
    rc = file_write_str(&g_event_fil, "type,sample_index,timestamp_ms,rr_ms,hr_bpm,class_id,confidence_percent\r\n");
    if (rc != 0) {
        xil_printf("[SD] write event header failed\r\n");
        f_close(&g_meta_fil);
        f_close(&g_event_fil);
        f_close(&g_sample_fil);
        remove_record_files(rec);
        return -7;
    }

    
    rc = 0;
    rc |= write_meta_u64(&g_meta_fil, "record_id", dev->record_id);
    rc |= write_meta_str(&g_meta_fil, "patient_id", dev->patient.patient_id);
    rc |= write_meta_str(&g_meta_fil, "patient_name", dev->patient.patient_name);
    rc |= write_meta_u32(&g_meta_fil, "age", (u32)dev->patient.age);
    rc |= write_meta_char(&g_meta_fil, "sex", dev->patient.sex);
    rc |= write_meta_u32(&g_meta_fil, "fs_hz", fs_hz);
    rc |= write_meta_str(&g_meta_fil, "software_revision", ECG_FIRMWARE_REVISION);
    rc |= write_meta_str(&g_meta_fil, "clock_source", "SAMPLE_RELATIVE_NO_OFFLINE_RTC");

    rc |= write_meta_hex8(&g_meta_fil, "cn121_config", dev->cn121.config);
    rc |= write_meta_hex8(&g_meta_fil, "cn121_chset", dev->cn121.chset);
    rc |= write_meta_u32(&g_meta_fil, "cn121_gain", dev->cn121.gain);
    rc |= write_meta_u32(&g_meta_fil, "cn121_hp_x100", dev->cn121.hp_x100);
    rc |= write_meta_u32(&g_meta_fil, "cn121_lp_hz", dev->cn121.lp_hz);
    rc |= write_meta_u32(&g_meta_fil, "sample_index_base", 1U);
    rc |= write_meta_str(&g_meta_fil, "timestamp_origin", "FIRST_RECORDED_SAMPLE");
    rc |= write_meta_u32(&g_meta_fil, "sqi_constant_run_samples", ECG_QUALITY_CONSTANT_RUN);
    rc |= write_meta_u32(&g_meta_fil, "lod_event_stable_samples", ECG_LOD_EVENT_STABLE_SAMPLES);
    rc |= write_meta_str(&g_meta_fil, "config_event_semantics", "CN121_CHSET/CN121_CONFIG: class_id=register readback; index=last processed sample, not exact hardware switch boundary");
    rc |= write_meta_str(&g_meta_fil, "cnn_skip_semantics", "CNN_SKIP: class_id=1 quality reset,2 record stop,3 queue full,4 window unavailable,5 inference error,6 frontend change");
    rc |= write_meta_u32(&g_meta_fil, "sample_segment_seconds", ECG_REC_SEGMENT_SECONDS);
    rc |= write_meta_str(&g_meta_fil, "sample_segment_naming", "RECxxxx.CSV,RECxxxx_Snnn.CSV");
    rc |= write_meta_str(&g_meta_fil, "display_filter", "CAUSAL_MEDIAN3_EMA");
    rc |= write_meta_str(&g_meta_fil, "rpeak_filter", "NOTCH50_QRS_BAND_MWI");
    rc |= write_meta_s32(&g_meta_fil, "display_ema_alpha_ppm",
                         (s32)(ECG_DISPLAY_EMA_ALPHA * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "legacy_baseline_alpha_ppm",
                         (s32)(ECG_LEGACY_BASELINE_ALPHA * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "legacy_lp_alpha_ppm",
                         (s32)(ECG_LEGACY_LP_ALPHA * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "qrs_hp_alpha_ppm",
                         (s32)(ECG_QRS_HP_ALPHA * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "qrs_lp_alpha_ppm",
                         (s32)(ECG_QRS_LP_ALPHA * 1000000.0f));
    rc |= write_meta_u32(&g_meta_fil, "qrs_mwi_len", ECG_QRS_MWI_LEN);
    rc |= write_meta_s32(&g_meta_fil, "notch_b0_ppm",
                         (s32)(ECG_NOTCH_B0 * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "notch_b1_ppm",
                         (s32)(ECG_NOTCH_B1 * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "notch_b2_ppm",
                         (s32)(ECG_NOTCH_B2 * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "notch_a1_feedback_ppm",
                         (s32)(ECG_NOTCH_A1_FEEDBACK * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "notch_a2_feedback_ppm",
                         (s32)(ECG_NOTCH_A2_FEEDBACK * 1000000.0f));
    rc |= write_meta_u32(&g_meta_fil, "rpeak_init_seconds", RPEAK_INIT_SECONDS);
    rc |= write_meta_s32(&g_meta_fil, "rpeak_threshold_scale_ppm",
                         (s32)(ECG_RPEAK_THRESHOLD_SCALE * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "rpeak_min_abs_qrs_ppm",
                         (s32)(ECG_RPEAK_MIN_ABS_QRS * 1000000.0f));
    rc |= write_meta_u32(&g_meta_fil, "rpeak_refractory_ms", RPEAK_REFRACTORY_MS);
    rc |= write_meta_u32(&g_meta_fil, "rr_min_ms", RR_MIN_MS);
    rc |= write_meta_u32(&g_meta_fil, "rr_max_ms", RR_MAX_MS);
    rc |= write_meta_u32(&g_meta_fil, "sqi_use_otr", ECG_SQI_USE_OTR);
    rc |= write_meta_u32(&g_meta_fil, "sqi_window_seconds", ECG_QUALITY_WINDOW_SECONDS);
    rc |= write_meta_u32(&g_meta_fil, "sqi_recovery_windows", ECG_QUALITY_RECOVERY_WINDOWS);
    rc |= write_meta_u32(&g_meta_fil, "sqi_clip_percent", ECG_QUALITY_CLIP_PERCENT);
    rc |= write_meta_u32(&g_meta_fil, "sqi_spike_percent", ECG_QUALITY_SPIKE_PERCENT);
    rc |= write_meta_u32(&g_meta_fil, "sqi_hard_clip_run", ECG_QUALITY_HARD_CLIP_RUN);
    rc |= write_meta_u32(&g_meta_fil, "sqi_low_headroom_code", ECG_QUALITY_LOW_HEADROOM_CODE);
    rc |= write_meta_u32(&g_meta_fil, "sqi_high_headroom_code", ECG_QUALITY_HIGH_HEADROOM_CODE);
    rc |= write_meta_u32(&g_meta_fil, "sqi_headroom_percent", ECG_QUALITY_HEADROOM_PERCENT);
    rc |= write_meta_u32(&g_meta_fil, "sqi_mean_adc_min", ECG_QUALITY_MEAN_ADC_MIN);
    rc |= write_meta_u32(&g_meta_fil, "sqi_mean_adc_max", ECG_QUALITY_MEAN_ADC_MAX);
    rc |= write_meta_s32(&g_meta_fil, "sqi_flat_diff_ppm",
                         (s32)(ECG_QUALITY_FLAT_DIFF * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "sqi_spike_diff_ppm",
                         (s32)(ECG_QUALITY_SPIKE_DIFF * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "sqi_min_range_ppm",
                         (s32)(ECG_QUALITY_MIN_RANGE * 1000000.0f));
    rc |= write_meta_s32(&g_meta_fil, "sqi_max_mean_abs_diff_ppm",
                         (s32)(ECG_QUALITY_MAX_MEAN_ABS_DIFF * 1000000.0f));
    rc |= write_meta_str(&g_meta_fil, "cnn_class_0", "F");
    rc |= write_meta_str(&g_meta_fil, "cnn_class_1", "N");
    rc |= write_meta_str(&g_meta_fil, "cnn_class_2", "S");
    rc |= write_meta_str(&g_meta_fil, "cnn_class_3", "V");
    rc |= write_meta_str(&g_meta_fil, "cnn_confidence", "NA_ARGMAX_ONLY");

    if (rc != 0) {
        xil_printf("[SD] write meta failed\r\n");
        f_close(&g_meta_fil);
        f_close(&g_event_fil);
        f_close(&g_sample_fil);
        remove_record_files(rec);
        return -8;
    }

    fr = f_sync(&g_meta_fil);
    if (fr != FR_OK) {
        xil_printf("[SD] meta f_sync failed: %d\r\n", fr);
        f_close(&g_meta_fil);
        f_close(&g_event_fil);
        f_close(&g_sample_fil);
        remove_record_files(rec);
        return -9;
    }

    if (f_close(&g_meta_fil) != FR_OK) {
        f_close(&g_event_fil);
        f_close(&g_sample_fil);
        return -10;
    }

    rec->sample_file = &g_sample_fil;
    rec->event_file = &g_event_fil;
    rec->meta_file = 0;

    rec->sample_write_count = 0;
    rec->event_write_count = 0;
    rec->sample_segment_index = 0U;
    rec->sample_segment_write_count = 0U;
    rec->recording = 1;

    xil_printf("[SD] record start: %s\r\n", rec->sample_filename);

    return 0;
}

int ecg_recorder_write_sample(EcgRecorder *rec,
                              const EcgRecordSample *s)
{
    char line[160];
    char *p = line;
    char *end = line + sizeof(line);

    if (!rec || !s) {
        return -1;
    }

    if (!rec->recording) {
        return 0;
    }

    if ((ECG_REC_SEGMENT_SAMPLES > 0U) &&
        (rec->sample_segment_write_count >= ECG_REC_SEGMENT_SAMPLES)) {
        if (rotate_sample_segment(rec) != 0) {
            return -5;
        }
    }

    
    p = append_u64_dec(p, end, s->sample_index);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, s->timestamp_ms);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->raw_adc);
    p = append_char(p, end, ',');

    p = append_s32_dec(p, end, (s32)s->filtered);
    p = append_char(p, end, ',');

    p = append_s32_dec(p, end, (s32)s->filtered_old);
    p = append_char(p, end, ',');

    p = append_s32_dec(p, end, (s32)s->qrs);
    p = append_char(p, end, ',');

    p = append_s32_dec(p, end, (s32)s->env);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->sqi_state);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->sqi_reason);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->sqi_warning);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->lod);
    p = append_char(p, end, ',');

    p = append_u32_dec(p, end, (u32)s->otr);
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -3;
    }

    if (file_write_buf(&g_sample_fil, line, (UINT)(p - line)) != 0) {
        xil_printf("[SD] sample f_write failed\r\n");
        return -4;
    }

    rec->sample_write_count++;
    rec->sample_segment_write_count++;

    if ((rec->sample_write_count % ECG_REC_SYNC_EVERY) == 0U) {
        FRESULT fr = f_sync(&g_sample_fil);
        if (fr != FR_OK) {
            xil_printf("[SD] sample f_sync failed: %d\r\n", fr);
            return -2;
        }
    }

    return 0;
}

static int write_event_line(EcgRecorder *rec,
                            const char *type,
                            u64 sample_index,
                            u32 timestamp_ms,
                            u32 rr_ms,
                            u32 hr_bpm,
                            u8 class_id,
                            u8 confidence)
{
    char line[128];
    char *p = line;
    char *end = line + sizeof(line);

    if (!rec || !type) {
        return -1;
    }

    if (!rec->recording) {
        return 0;
    }

    
    p = append_str(p, end, type);
    p = append_char(p, end, ',');
    p = append_u64_dec(p, end, sample_index);
    p = append_char(p, end, ',');
    p = append_u32_dec(p, end, timestamp_ms);
    p = append_char(p, end, ',');
    p = append_u32_dec(p, end, rr_ms);
    p = append_char(p, end, ',');
    p = append_u32_dec(p, end, hr_bpm);
    p = append_char(p, end, ',');
    p = append_u32_dec(p, end, (u32)class_id);
    p = append_char(p, end, ',');
    if (confidence == ECG_CONFIDENCE_UNAVAILABLE) {
        p = append_str(p, end, "NA");
    } else {
        p = append_u32_dec(p, end, (u32)confidence);
    }
    p = append_char(p, end, '\r');
    p = append_char(p, end, '\n');

    if (p > end) {
        return -3;
    }

    if (file_write_buf(&g_event_fil, line, (UINT)(p - line)) != 0) {
        xil_printf("[SD] event f_write failed\r\n");
        return -4;
    }

    rec->event_write_count++;

    if ((rec->event_write_count % ECG_REC_SYNC_EVERY) == 0U) {
        FRESULT fr = f_sync(&g_event_fil);
        if (fr != FR_OK) {
            xil_printf("[SD] event f_sync failed: %d\r\n", fr);
            return -2;
        }
    }

    return 0;
}

int ecg_recorder_write_rpeak_event(EcgRecorder *rec,
                                   const EcgRecordEvent *e)
{
    if (!rec || !e) {
        return -1;
    }

    return write_event_line(rec,
                            "RPEAK",
                            e->sample_index,
                            e->timestamp_ms,
                            e->rr_ms,
                            e->hr_bpm,
                            e->class_id,
                            e->confidence);
}

int ecg_recorder_write_cnn_event(EcgRecorder *rec,
                                 const EcgRecordEvent *e)
{
    if (!rec || !e) {
        return -1;
    }

    return write_event_line(rec,
                            "CNN",
                            e->sample_index,
                            e->timestamp_ms,
                            e->rr_ms,
                            e->hr_bpm,
                            e->class_id,
                            e->confidence);
}

int ecg_recorder_write_lod_event(EcgRecorder *rec,
                                 u64 sample_index,
                                 u32 timestamp_ms,
                                 u8 lod)
{
    if (!rec) {
        return -1;
    }

    return write_event_line(rec,
                            "LOD",
                            sample_index,
                            timestamp_ms,
                            0U,
                            0U,
                            lod,
                            0U);
}

int ecg_recorder_write_quality_event(EcgRecorder *rec,
                                     u64 sample_index,
                                     u32 timestamp_ms,
                                     u8 quality_good,
                                     u8 reason)
{
    if (!rec) {
        return -1;
    }

    return write_event_line(rec,
                            quality_good ? "SQI_GOOD" : "SQI_BAD",
                            sample_index,
                            timestamp_ms,
                            0U,
                            0U,
                            reason,
                            0U);
}

int ecg_recorder_write_diagnostic(EcgRecorder *rec, const char *type,
                                  u64 index, u32 timestamp_ms, u8 detail)
{
    return write_event_line(rec, type, index, timestamp_ms, 0U, 0U, detail, 0U);
}

int ecg_recorder_stop(EcgRecorder *rec)
{
    FRESULT fr1 = FR_OK;
    FRESULT fr2 = FR_OK;
    FRESULT close1 = FR_OK, close2 = FR_OK;

    if (!rec) {
        return -1;
    }

    if (!rec->recording) {
        return 0;
    }

    if (rec->sample_file != 0) {
        fr1 = f_sync(&g_sample_fil);
    }
    if (rec->event_file != 0) {
        fr2 = f_sync(&g_event_fil);
    }

    if (fr1 != FR_OK) {
        xil_printf("[SD] final sample f_sync failed: %d\r\n", fr1);
    }

    if (fr2 != FR_OK) {
        xil_printf("[SD] final event f_sync failed: %d\r\n", fr2);
    }

    if (rec->sample_file != 0) {
        close1 = f_close(&g_sample_fil);
    }
    if (rec->event_file != 0) {
        close2 = f_close(&g_event_fil);
    }

    rec->recording = 0;
    rec->sample_file = 0;
    rec->event_file = 0;
    rec->meta_file = 0;

    xil_printf("[SD] record stop\r\n");

    if ((fr1 != FR_OK) || (fr2 != FR_OK) ||
        (close1 != FR_OK) || (close2 != FR_OK)) {
        xil_printf("[SD] close/sync failed; do not assume a complete record\r\n");
        return -2;
    }

    return 0;
}

u8 ecg_recorder_is_recording(const EcgRecorder *rec)
{
    if (!rec) {
        return 0;
    }

    return rec->recording;
}
