/* =========================================================
 * File: ecg_rpeak.h
 * ========================================================= */
#ifndef ECG_RPEAK_H
#define ECG_RPEAK_H

#include "xil_types.h"
#include "ecg_config.h"

/*
 * R peak detector.
 *
 * This is a practical first version:
 *   - Uses filtered ECG morphology
 *   - Uses QRS slope envelope for adaptive threshold
 *   - Uses local maximum detection
 *   - Uses refractory period
 *
 * It is not a full Pan-Tompkins implementation yet.
 */
typedef struct {
    /*
     * Three-point local max detection on filtered ECG.
     */
    float x2;
    float x1;
    float x0;

    u64 idx2;
    u64 idx1;
    u64 idx0;

    /*
     * Adaptive threshold based on envelope.
     */
    float env_mean;
    float env_peak;
    float threshold;

    /*
     * Initialization statistics.
     */
    u32 init_count;
    float init_env_sum;
    float init_env_max;

    /*
     * R peak memory.
     */
    u64 last_r_index;
    u64 prev_r_index;
    float last_r_value;

    /*
     * Result of current detection.
     */
    u64 detected_r_index;
    u64 detected_prev_r_index;
    u32 detected_rr_samples;
    float detected_r_value;

    u8 initialized;
    u8 has_last_r;
} RPeakDetector;

void ecg_rpeak_init(RPeakDetector *d);

/*
 * Returns 1 if a new R peak is detected.
 *
 * Parameters:
 *   x    : filtered ECG sample
 *   env  : QRS slope envelope from ecg_filter_get_env()
 *   gidx : global sample index of x
 */
int ecg_rpeak_step(RPeakDetector *d, float x, float env, u64 gidx);

u32 ecg_rpeak_last_rr_samples(const RPeakDetector *d);
u32 ecg_rpeak_last_rr_ms(const RPeakDetector *d);
u32 ecg_rpeak_last_hr_bpm(const RPeakDetector *d);

#endif /* ECG_RPEAK_H */
