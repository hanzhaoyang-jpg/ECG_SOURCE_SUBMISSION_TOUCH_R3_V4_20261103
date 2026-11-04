#include "device_state.h"
#include "cn121_ps.h"
#include <string.h>

void device_state_init(DeviceContext *ctx)
{
    if (!ctx) return;

    memset(ctx, 0, sizeof(DeviceContext));

    ctx->state = DEV_STATE_BOOT;
    ctx->error = DEV_ERR_NONE;

    strcpy(ctx->patient.patient_id, "P0000");
    strcpy(ctx->patient.patient_name, "UNKNOWN");
    ctx->patient.age = 0;
    ctx->patient.sex = 'U';

    ctx->cn121.config = 0x00;
    ctx->cn121.chset  = CN121_CHSET_ECG_DEFAULT;
    ctx->cn121.gain   = 60U * ((CN121_CHSET_ECG_DEFAULT & 15U) + 1U);
    ctx->cn121.hp_x100 = (CN121_CHSET_ECG_DEFAULT & 64U) ? 5U : 400U;
    ctx->cn121.lp_hz   = 40;

    ctx->sd_ready = 0;
    ctx->recording_enabled = 0;
    ctx->upload_enabled = 0;
    ctx->record_id = 0;
    ctx->record_start_ms = 0;
    ctx->record_elapsed_ms = 0;

    ctx->state = DEV_STATE_IDLE;
}

DeviceState device_get_state(const DeviceContext *ctx)
{
    if (!ctx) return DEV_STATE_ERROR;
    return ctx->state;
}

DeviceError device_get_error(const DeviceContext *ctx)
{
    if (!ctx) return DEV_ERR_UNKNOWN;
    return ctx->error;
}

int device_set_patient(DeviceContext *ctx,
                       const char *patient_id,
                       const char *patient_name,
                       u8 age,
                       char sex)
{
    if (!ctx) return -1;

    if (ctx->state == DEV_STATE_RECORDING) {
        return -2;
    }

    if (patient_id) {
        strncpy(ctx->patient.patient_id, patient_id, sizeof(ctx->patient.patient_id) - 1);
        ctx->patient.patient_id[sizeof(ctx->patient.patient_id) - 1] = '\0';
    }

    if (patient_name) {
        strncpy(ctx->patient.patient_name, patient_name, sizeof(ctx->patient.patient_name) - 1);
        ctx->patient.patient_name[sizeof(ctx->patient.patient_name) - 1] = '\0';
    }

    ctx->patient.age = age;
    ctx->patient.sex = sex;

    return 0;
}

void device_set_cn121_snapshot(DeviceContext *ctx,
                               u8 config,
                               u8 chset,
                               u16 gain,
                               u16 hp_x100,
                               u16 lp_hz)
{
    if (!ctx) return;

    ctx->cn121.config = config;
    ctx->cn121.chset = chset;
    ctx->cn121.gain = gain;
    ctx->cn121.hp_x100 = hp_x100;
    ctx->cn121.lp_hz = lp_hz;
}

int device_mark_sd_ready(DeviceContext *ctx, u8 ready)
{
    if (!ctx) return -1;

    ctx->sd_ready = ready ? 1 : 0;

    if (!ctx->sd_ready && ctx->state == DEV_STATE_RECORDING) {
        ctx->state = DEV_STATE_ERROR;
        ctx->error = DEV_ERR_SD_WRITE_FAILED;
        ctx->recording_enabled = 0;
        return -2;
    }

    return 0;
}

int device_start_recording(DeviceContext *ctx, u32 now_ms)
{
    if (!ctx) return -1;

    if (ctx->state == DEV_STATE_RECORDING) {
        return 0;
    }

    if (ctx->state == DEV_STATE_ERROR) {
        return -2;
    }

    if (!ctx->sd_ready) {
        ctx->state = DEV_STATE_ERROR;
        ctx->error = DEV_ERR_SD_INIT_FAILED;
        return -3;
    }

    ctx->record_id++;
    ctx->record_start_ms = now_ms;
    ctx->record_elapsed_ms = 0;
    ctx->recording_enabled = 1;
    ctx->state = DEV_STATE_RECORDING;

    return 0;
}

int device_pause_recording(DeviceContext *ctx)
{
    if (!ctx) return -1;

    if (ctx->state != DEV_STATE_RECORDING) {
        return -2;
    }

    ctx->recording_enabled = 0;
    ctx->state = DEV_STATE_PAUSED;

    return 0;
}

int device_resume_recording(DeviceContext *ctx)
{
    if (!ctx) return -1;

    if (ctx->state != DEV_STATE_PAUSED) {
        return -2;
    }

    if (!ctx->sd_ready) {
        ctx->state = DEV_STATE_ERROR;
        ctx->error = DEV_ERR_SD_WRITE_FAILED;
        return -3;
    }

    ctx->recording_enabled = 1;
    ctx->state = DEV_STATE_RECORDING;

    return 0;
}

int device_stop_recording(DeviceContext *ctx)
{
    if (!ctx) return -1;

    if (ctx->state == DEV_STATE_RECORDING || ctx->state == DEV_STATE_PAUSED) {
        ctx->recording_enabled = 0;
        ctx->state = DEV_STATE_IDLE;
        return 0;
    }

    return -2;
}

void device_set_error(DeviceContext *ctx, DeviceError err)
{
    if (!ctx) return;

    ctx->error = err;
    ctx->recording_enabled = 0;
    ctx->state = DEV_STATE_ERROR;
}

void device_clear_error(DeviceContext *ctx)
{
    if (!ctx) return;

    ctx->error = DEV_ERR_NONE;
    ctx->recording_enabled = 0;
    ctx->state = DEV_STATE_IDLE;
}

const char *device_state_name(DeviceState s)
{
    switch (s) {
    case DEV_STATE_BOOT:      return "BOOT";
    case DEV_STATE_IDLE:      return "IDLE";
    case DEV_STATE_RECORDING: return "RECORDING";
    case DEV_STATE_PAUSED:    return "PAUSED";
    case DEV_STATE_ERROR:     return "ERROR";
    default:                  return "UNKNOWN";
    }
}

const char *device_error_name(DeviceError e)
{
    switch (e) {
    case DEV_ERR_NONE:           return "NONE";
    case DEV_ERR_SD_INIT_FAILED: return "SD_INIT_FAILED";
    case DEV_ERR_SD_WRITE_FAILED:return "SD_WRITE_FAILED";
    case DEV_ERR_ADC_OVERFLOW:   return "ADC_OVERFLOW";
    case DEV_ERR_CNN_FAILED:     return "CNN_FAILED";
    case DEV_ERR_CN121_FAILED:   return "CN121_FAILED";
    case DEV_ERR_CONFIG_INVALID: return "CONFIG_INVALID";
    case DEV_ERR_UNKNOWN:        return "UNKNOWN";
    default:                     return "UNKNOWN";
    }
}
