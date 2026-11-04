/* =========================================================
 * File: ecg_filter.c
 * ========================================================= */
#include "ecg_filter.h"

static float median3_local(float a, float b, float c)
{
    if (((a <= b) && (b <= c)) || ((c <= b) && (b <= a))) return b;
    if (((b <= a) && (a <= c)) || ((c <= a) && (a <= b))) return a;
    return c;
}

void ecg_filter_init(EcgFilter *f)
{
    u32 i;

    if (f == 0) return;

    f->notch_x1 = 0.0f;
    f->notch_x2 = 0.0f;
    f->notch_y1 = 0.0f;
    f->notch_y2 = 0.0f;
    f->baseline = 0.0f;
    f->lp = 0.0f;
    f->prev_lp = 0.0f;
    f->legacy_lp = 0.0f;
    f->env = 0.0f;
    f->disp_x1 = 0.0f;
    f->disp_x2 = 0.0f;
    f->disp_ema = 0.0f;
    f->qrs_hp_prev_x = 0.0f;
    f->qrs_hp = 0.0f;
    f->qrs_lp = 0.0f;
    f->qrs_prev = 0.0f;
    f->qrs_mwi_sum = 0.0f;
    f->qrs_mwi_pos = 0U;
    for (i = 0U; i < ECG_QRS_MWI_LEN; i++) {
        f->qrs_mwi[i] = 0.0f;
    }
    f->initialized = 0U;
}

float ecg_filter_step(EcgFilter *f, float x)
{
    float notch;
    float disp_med;
    float disp_out;
    float hp;
    float qrs_derivative;
    float qrs_energy;

    /*
     * Display/record path:
     *   median-of-3 removes isolated ADC/contact spikes.
     *   EMA then gives a low-latency real-time smoothing output.
     * This path is intentionally causal and uses only current/past samples.
     */
    if (f == 0) return x;

    if (!f->initialized) {
        f->notch_x1 = x;
        f->notch_x2 = x;
        f->notch_y1 = x;
        f->notch_y2 = x;
        f->baseline = x;
        f->lp = 0.0f;
        f->prev_lp = 0.0f;
        f->legacy_lp = 0.0f;
        f->env = 0.0f;
        f->disp_x1 = x;
        f->disp_x2 = x;
        f->disp_ema = x;
        f->qrs_hp_prev_x = x;
        f->qrs_hp = 0.0f;
        f->qrs_lp = 0.0f;
        f->qrs_prev = 0.0f;
        f->initialized = 1U;
        return x;
    }

    disp_med = median3_local(f->disp_x2, f->disp_x1, x);
    f->disp_ema = f->disp_ema +
                  ECG_DISPLAY_EMA_ALPHA * (disp_med - f->disp_ema);
    f->disp_x2 = f->disp_x1;
    f->disp_x1 = x;
    disp_out = f->disp_ema;
    f->prev_lp = f->lp;
    f->lp = disp_out;

    /*
     * 50 Hz notch, fs=360 Hz, Q=20. Coefficients are normalized by a0.
     */
    notch = ECG_NOTCH_B0 * x
          + ECG_NOTCH_B1 * f->notch_x1
          + ECG_NOTCH_B2 * f->notch_x2
          + ECG_NOTCH_A1_FEEDBACK * f->notch_y1
          + ECG_NOTCH_A2_FEEDBACK * f->notch_y2;

    f->notch_x2 = f->notch_x1;
    f->notch_x1 = x;
    f->notch_y2 = f->notch_y1;
    f->notch_y1 = notch;

    /*
     * Old filtered path, retained only for side-by-side logging:
     * 50 Hz notch -> slow baseline subtraction -> first-order low-pass.
     */
    f->baseline = f->baseline +
                  ECG_LEGACY_BASELINE_ALPHA * (notch - f->baseline);
    hp = notch - f->baseline;
    f->legacy_lp = f->legacy_lp +
                    ECG_LEGACY_LP_ALPHA * (hp - f->legacy_lp);

    /* Dedicated 5-20 Hz QRS path for robust R-peak detection. */
    f->qrs_hp = ECG_QRS_HP_ALPHA *
                 (f->qrs_hp + notch - f->qrs_hp_prev_x);
    f->qrs_hp_prev_x = notch;
    f->qrs_lp = f->qrs_lp +
                ECG_QRS_LP_ALPHA * (f->qrs_hp - f->qrs_lp);

    qrs_derivative = f->qrs_lp - f->qrs_prev;
    f->qrs_prev = f->qrs_lp;
    qrs_energy = qrs_derivative * qrs_derivative;

    f->qrs_mwi_sum -= f->qrs_mwi[f->qrs_mwi_pos];
    f->qrs_mwi[f->qrs_mwi_pos] = qrs_energy;
    f->qrs_mwi_sum += qrs_energy;
    f->qrs_mwi_pos++;
    if (f->qrs_mwi_pos >= ECG_QRS_MWI_LEN) {
        f->qrs_mwi_pos = 0U;
    }
    f->env = f->qrs_mwi_sum / (float)ECG_QRS_MWI_LEN;

    return disp_out;
}

float ecg_filter_get_qrs(const EcgFilter *f)
{
    if (f == 0) return 0.0f;
    return f->qrs_lp;
}

float ecg_filter_get_legacy(const EcgFilter *f)
{
    if (f == 0) return 0.0f;
    return f->legacy_lp;
}

float ecg_filter_get_env(const EcgFilter *f)
{
    if (f == 0) return 0.0f;
    return f->env;
}

void ecg_remove_mean(float *x, int n)
{
    int i;
    float sum = 0.0f;
    float mean;

    if (x == 0 || n <= 0) return;

    for (i = 0; i < n; i++) {
        sum += x[i];
    }

    mean = sum / (float)n;

    for (i = 0; i < n; i++) {
        x[i] -= mean;
    }
}

void ecg_minmax_scale(float *x, int n)
{
    int i;
    float minv;
    float maxv;
    float range;

    if (x == 0 || n <= 0) return;

    minv = x[0];
    maxv = x[0];

    for (i = 1; i < n; i++) {
        if (x[i] < minv) minv = x[i];
        if (x[i] > maxv) maxv = x[i];
    }

    range = maxv - minv;
    if (range < 1e-6f) {
        for (i = 0; i < n; i++) {
            x[i] = 0.0f;
        }
        return;
    }

    for (i = 0; i < n; i++) {
        x[i] = 2.0f * ((x[i] - minv) / range) - 1.0f;
    }
}

