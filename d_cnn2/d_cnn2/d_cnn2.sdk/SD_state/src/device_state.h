#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include "xil_types.h"

typedef enum {
    DEV_STATE_BOOT = 0,
    DEV_STATE_IDLE,
    DEV_STATE_RECORDING,
    DEV_STATE_PAUSED,
    DEV_STATE_ERROR
} DeviceState;

typedef enum {
    DEV_ERR_NONE = 0,
    DEV_ERR_SD_INIT_FAILED,
    DEV_ERR_SD_WRITE_FAILED,
    DEV_ERR_ADC_OVERFLOW,
    DEV_ERR_CNN_FAILED,
    DEV_ERR_CN121_FAILED,
    DEV_ERR_CONFIG_INVALID,
    DEV_ERR_UNKNOWN
} DeviceError;

typedef struct {
    char patient_id[32];
    char patient_name[32];
    u8   age;
    char sex;              // 'M', 'F', or 'U'
} PatientInfo;

typedef struct {
    u8 chset;
    u8 config;
    u16 gain;
    u16 hp_x100;           // 0.05Hz -> 5, 4Hz -> 400
    u16 lp_hz;             // 40/55/150/300
} Cn121ConfigSnapshot;

typedef struct {
    DeviceState state;
    DeviceError error;

    PatientInfo patient;
    Cn121ConfigSnapshot cn121;

    u8 sd_ready;
    u8 recording_enabled;
    u8 upload_enabled;

    u32 record_id;
    u32 record_start_ms;
    u32 record_elapsed_ms;
} DeviceContext;

void device_state_init(DeviceContext *ctx);

DeviceState device_get_state(const DeviceContext *ctx);
DeviceError device_get_error(const DeviceContext *ctx);

int device_set_patient(DeviceContext *ctx,
                       const char *patient_id,
                       const char *patient_name,
                       u8 age,
                       char sex);

void device_set_cn121_snapshot(DeviceContext *ctx,
                               u8 config,
                               u8 chset,
                               u16 gain,
                               u16 hp_x100,
                               u16 lp_hz);

int device_mark_sd_ready(DeviceContext *ctx, u8 ready);

int device_start_recording(DeviceContext *ctx, u32 now_ms);
int device_pause_recording(DeviceContext *ctx);
int device_resume_recording(DeviceContext *ctx);
int device_stop_recording(DeviceContext *ctx);

void device_set_error(DeviceContext *ctx, DeviceError err);
void device_clear_error(DeviceContext *ctx);

const char *device_state_name(DeviceState s);
const char *device_error_name(DeviceError e);

#endif
