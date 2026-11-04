/* =========================================================
 * File: ecg_rpeak.c
 * ========================================================= */
#include "ecg_rpeak.h"

static float f_abs_local(float x)
{
    return (x >= 0.0f) ? x : -x;
}

void ecg_rpeak_init(RPeakDetector *d)
{
    if (d == 0) return;

    d->x2 = 0.0f;
    d->x1 = 0.0f;
    d->x0 = 0.0f;

    d->idx2 = 0ULL;
    d->idx1 = 0ULL;
    d->idx0 = 0ULL;

    d->env_mean = 0.0f;
    d->env_peak = 0.0f;
    d->threshold = 0.0f;

    d->init_count = 0U;
    d->init_env_sum = 0.0f;
    d->init_env_max = 0.0f;

    d->last_r_index = 0ULL;
    d->prev_r_index = 0ULL;
    d->last_r_value = 0.0f;

    d->detected_r_index = 0ULL;
    d->detected_prev_r_index = 0ULL;
    d->detected_rr_samples = 0U;
    d->detected_r_value = 0.0f;

    d->initialized = 0U;
    d->has_last_r = 0U;
}

int ecg_rpeak_step(RPeakDetector *d, float x, float env, u64 gidx)
{
    int is_local_max;
    int above_threshold;
    int outside_refractory;
    int rr_reasonable;
    u32 rr = 0U;

    if (d == 0) return 0;

    /*
     * Update three-sample history.
     */
    d->x2 = d->x1;
    d->x1 = d->x0;
    d->x0 = x;

    d->idx2 = d->idx1;
    d->idx1 = d->idx0;
    d->idx0 = gidx;

    /*
     * Startup threshold initialization.
     * We collect about 2 seconds of envelope statistics.
     */
    if (!d->initialized) {
        d->init_count++;
        d->init_env_sum += env;
        if (env > d->init_env_max) {
            d->init_env_max = env;
        }

        if (d->init_count >= RPEAK_INIT_SAMPLES) {
            d->env_mean = d->init_env_sum / (float)d->init_count;
            d->env_peak = d->init_env_max;

            /*
             * Threshold is between mean and observed peak.
             */
            d->threshold = d->env_mean +
                           ECG_RPEAK_THRESHOLD_SCALE *
                           (d->env_peak - d->env_mean);

            if (d->threshold < ECG_RPEAK_MIN_THRESHOLD) {
                d->threshold = ECG_RPEAK_MIN_THRESHOLD;
            }

            d->initialized = 1U;
        }

        return 0;
    }

    /*
     * Adaptive envelope statistics.
     */
    d->env_mean = d->env_mean +
                  ECG_RPEAK_ENV_MEAN_ALPHA * (env - d->env_mean);

    if (env > d->env_peak) {
        d->env_peak = d->env_peak +
                      ECG_RPEAK_ENV_PEAK_ATTACK * (env - d->env_peak);
    } else {
        d->env_peak = d->env_peak +
                      ECG_RPEAK_ENV_PEAK_DECAY * (env - d->env_peak);
    }

    d->threshold = d->env_mean +
                   ECG_RPEAK_THRESHOLD_SCALE *
                   (d->env_peak - d->env_mean);

    if (d->threshold < ECG_RPEAK_MIN_THRESHOLD) {
        d->threshold = ECG_RPEAK_MIN_THRESHOLD;
    }

    /*
     * Local maximum on filtered ECG:
     * x1 is the candidate.
     */
    //is_local_max = (d->x1 > d->x2) && (d->x1 >= d->x0);

    /*
     * For ECG after baseline removal, R may be positive in normal wiring.
     * If your ECG polarity is inverted, this first version may detect poorly.
     * Later we can add polarity auto-detection.
     */
    //above_threshold = (env > d->threshold) && (d->x1 > 0.02f);
    float abs_x2;
    float abs_x1;
    float abs_x0;

    abs_x2 = f_abs_local(d->x2);
    abs_x1 = f_abs_local(d->x1);
    abs_x0 = f_abs_local(d->x0);
    is_local_max = (abs_x1 > abs_x2) && (abs_x1 >= abs_x0);

    above_threshold = (env > d->threshold) &&
                      (abs_x1 > ECG_RPEAK_MIN_ABS_QRS);

    if (!d->has_last_r) {
        outside_refractory = 1;
        rr_reasonable = 1;
    } else {
        u64 diff = d->idx1 - d->last_r_index;
        outside_refractory = (diff >= RPEAK_REFRACTORY_SAMPLES) ? 1 : 0;

        rr = (u32)diff;
        rr_reasonable = ((rr >= RR_MIN_SAMPLES) && (rr <= RR_MAX_SAMPLES)) ? 1 : 0;
    }

    if (is_local_max && above_threshold && outside_refractory) {
        /*
         * If RR is outside normal debug range, we still accept the first
         * detection after a long pause, but reject extremely short intervals.
         */
        if (d->has_last_r && !rr_reasonable) {
            if (rr < RR_MIN_SAMPLES) {
                return 0;
            }
        }

        d->prev_r_index = d->last_r_index;
        d->last_r_index = d->idx1;
        d->last_r_value = d->x1;

        d->detected_r_index = d->last_r_index;
        d->detected_prev_r_index = d->prev_r_index;
        d->detected_r_value = d->last_r_value;

        if (d->has_last_r) {
            d->detected_rr_samples = (u32)(d->last_r_index - d->prev_r_index);
        } else {
            d->detected_rr_samples = 0U;
        }

        d->has_last_r = 1U;

        /*
         * After a valid R peak, update peak estimate upward if needed.
         */
        if (env > d->env_peak) {
            d->env_peak = env;
        }

        return 1;
    }

    return 0;
}

u32 ecg_rpeak_last_rr_samples(const RPeakDetector *d)
{
    if (d == 0) return 0U;
    return d->detected_rr_samples;
}

u32 ecg_rpeak_last_rr_ms(const RPeakDetector *d)
{
    u32 rr;

    if (d == 0) return 0U;

    rr = d->detected_rr_samples;
    if (rr == 0U) return 0U;

    return (rr * 1000U) / ECG_FS_HZ;
}

u32 ecg_rpeak_last_hr_bpm(const RPeakDetector *d)
{
    u32 rr;

    if (d == 0) return 0U;

    rr = d->detected_rr_samples;
    if (rr == 0U) return 0U;

    return (60U * ECG_FS_HZ) / rr;
}


