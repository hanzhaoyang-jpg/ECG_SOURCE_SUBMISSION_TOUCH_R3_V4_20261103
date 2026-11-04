/* =========================================================
 * File: ecg_filter.h
 * ========================================================= */
#ifndef ECG_FILTER_H
#define ECG_FILTER_H

#include "xil_types.h"
#include "ecg_config.h"

/*
 * Simple real-time ECG filter.
 *
 * Current real-time paths:
 *   1. display/record/CNN path:
 *        median-of-3 despike + EMA smoothing
 *   2. R-peak path:
 *        notch + QRS high-pass/low-pass + slope energy envelope
 *
 * The display path intentionally avoids aggressive baseline subtraction.
 * Live data showed that the older baseline-removal path could amplify
 * contact jumps into large false-looking spikes.
 */
typedef struct {
    float notch_x1;
    float notch_x2;
    float notch_y1;
    float notch_y2;

    float baseline;
    float lp;
    float prev_lp;
    float legacy_lp;
    float env;

    float disp_x1;
    float disp_x2;
    float disp_ema;

    float qrs_hp_prev_x;
    float qrs_hp;
    float qrs_lp;
    float qrs_prev;
    float qrs_mwi[ECG_QRS_MWI_LEN];
    float qrs_mwi_sum;
    u32 qrs_mwi_pos;

    u8 initialized;
} EcgFilter;

void  ecg_filter_init(EcgFilter *f);
float ecg_filter_step(EcgFilter *f, float x);
float ecg_filter_get_legacy(const EcgFilter *f);
float ecg_filter_get_env(const EcgFilter *f);
float ecg_filter_get_qrs(const EcgFilter *f);

void  ecg_remove_mean(float *x, int n);
void  ecg_minmax_scale(float *x, int n);

#endif /* ECG_FILTER_H */
