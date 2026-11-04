#ifndef ECG_QUALITY_H
#define ECG_QUALITY_H

#include "xil_types.h"
#include "ecg_config.h"

typedef enum {
    ECG_QUALITY_RECOVERING = 0,
    ECG_QUALITY_GOOD = 1,
    ECG_QUALITY_BAD = 2
} EcgQualityState;

#define ECG_QUALITY_REASON_LOD       0x01U
#define ECG_QUALITY_REASON_OTR       0x02U
#define ECG_QUALITY_REASON_CLIPPING  0x04U
#define ECG_QUALITY_REASON_FLATLINE  0x08U
#define ECG_QUALITY_REASON_SPIKES    0x10U
#define ECG_QUALITY_REASON_NOISE     0x20U

#define ECG_QUALITY_WARNING_LOW_HEADROOM  0x01U

typedef struct {
    EcgQualityState state;
    u8 reason;
    u8 warning;
    u8 score;
    u8 has_window;
    u8 good_windows;

    u32 count;
    u32 lod_count;
    u32 otr_count;
    u32 clip_count;
    u32 flat_count;
    u32 spike_count;
    u32 hard_clip_run;
    u32 constant_run;
    u8 previous_adc;
    u32 headroom_count;
    u32 sum_adc;

    float min_raw;
    float max_raw;
    float prev_raw;
    float sum_abs_diff;
    float latest_range;
    float latest_mean_abs_diff;
    u8 latest_lod_percent;
    u8 latest_otr_percent;
    u8 latest_clip_percent;
    u8 latest_flat_percent;
    u8 latest_spike_percent;
    u8 latest_headroom_percent;
    u8 latest_mean_adc;
} EcgSignalQuality;

void ecg_quality_init(EcgSignalQuality *q);
void ecg_quality_step(EcgSignalQuality *q,
                      u8 adc,
                      float raw,
                      u8 lod,
                      u8 otr);
u8 ecg_quality_is_good(const EcgSignalQuality *q);
const char *ecg_quality_state_name(EcgQualityState state);
const char *ecg_quality_reason_name(u8 reason);
const char *ecg_quality_warning_name(u8 warning);

#endif /* ECG_QUALITY_H */
