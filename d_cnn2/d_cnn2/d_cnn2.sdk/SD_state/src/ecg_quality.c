#include "ecg_quality.h"

static float abs_local(float x)
{
    return (x >= 0.0f) ? x : -x;
}

static void reset_window(EcgSignalQuality *q)
{
    q->count = 0U;
    q->lod_count = 0U;
    q->otr_count = 0U;
    q->clip_count = 0U;
    q->flat_count = 0U;
    q->spike_count = 0U;
    q->headroom_count = 0U;
    q->sum_adc = 0U;
    q->min_raw = 2.0f;
    q->max_raw = -2.0f;
    q->sum_abs_diff = 0.0f;
}

void ecg_quality_init(EcgSignalQuality *q)
{
    if (q == 0) return;

    q->state = ECG_QUALITY_RECOVERING;
    q->reason = 0U;
    q->warning = 0U;
    q->score = 0U;
    q->has_window = 0U;
    q->good_windows = 0U;
    q->prev_raw = 0.0f;
    q->latest_range = 0.0f;
    q->latest_mean_abs_diff = 0.0f;
    q->latest_lod_percent = 0U;
    q->latest_otr_percent = 0U;
    q->latest_clip_percent = 0U;
    q->latest_flat_percent = 0U;
    q->latest_spike_percent = 0U;
    q->latest_headroom_percent = 0U;
    q->latest_mean_adc = 128U;
    q->hard_clip_run = 0U;
    q->constant_run = 0U;
    q->previous_adc = 0U;
    reset_window(q);
}

static void evaluate_window(EcgSignalQuality *q)
{
    u8 reason = 0U;
    u32 denom;
    u32 penalty = 0U;

    if (q->count == 0U) return;

    denom = q->count;
    q->latest_range = q->max_raw - q->min_raw;
    q->latest_mean_abs_diff = q->sum_abs_diff /
                              (float)((denom > 1U) ? (denom - 1U) : 1U);
    q->latest_lod_percent = (u8)((q->lod_count * 100U) / denom);
    q->latest_otr_percent = (u8)((q->otr_count * 100U) / denom);
    q->latest_clip_percent = (u8)((q->clip_count * 100U) / denom);
    q->latest_flat_percent = (u8)((q->flat_count * 100U) / denom);
    q->latest_spike_percent = (u8)((q->spike_count * 100U) / denom);
    q->latest_headroom_percent =
        (u8)((q->headroom_count * 100U) / denom);
    q->latest_mean_adc = (u8)(q->sum_adc / denom);

    if (q->lod_count > 0U) reason |= ECG_QUALITY_REASON_LOD;
    if ((q->otr_count * 100U) >= (ECG_QUALITY_OTR_PERCENT * denom))
        reason |= ECG_QUALITY_REASON_OTR;
    if ((q->clip_count * 100U) >= (ECG_QUALITY_CLIP_PERCENT * denom))
        reason |= ECG_QUALITY_REASON_CLIPPING;
    if (((q->flat_count * 100U) >= (ECG_QUALITY_FLAT_PERCENT * denom)) ||
        (q->latest_range < ECG_QUALITY_MIN_RANGE) ||
        (q->constant_run >= ECG_QUALITY_CONSTANT_RUN))
        reason |= ECG_QUALITY_REASON_FLATLINE;
    if ((q->spike_count * 100U) >= (ECG_QUALITY_SPIKE_PERCENT * denom))
        reason |= ECG_QUALITY_REASON_SPIKES;
    if (q->latest_mean_abs_diff > ECG_QUALITY_MAX_MEAN_ABS_DIFF)
        reason |= ECG_QUALITY_REASON_NOISE;

    q->reason = reason;
    q->warning = 0U;
    if ((q->latest_headroom_percent >= ECG_QUALITY_HEADROOM_PERCENT) ||
        (q->latest_mean_adc < ECG_QUALITY_MEAN_ADC_MIN) ||
        (q->latest_mean_adc > ECG_QUALITY_MEAN_ADC_MAX)) {
        q->warning |= ECG_QUALITY_WARNING_LOW_HEADROOM;
    }

    if (reason & ECG_QUALITY_REASON_LOD) penalty += 100U;
    if (reason & ECG_QUALITY_REASON_OTR) penalty += 30U;
    if (reason & ECG_QUALITY_REASON_CLIPPING) penalty += 40U;
    if (reason & ECG_QUALITY_REASON_FLATLINE) penalty += 60U;
    if (reason & ECG_QUALITY_REASON_SPIKES) penalty += 35U;
    if (reason & ECG_QUALITY_REASON_NOISE) penalty += 35U;
    if (q->warning & ECG_QUALITY_WARNING_LOW_HEADROOM) penalty += 10U;
    q->score = (penalty >= 100U) ? 0U : (u8)(100U - penalty);

    if (reason != 0U) {
        q->state = ECG_QUALITY_BAD;
        q->good_windows = 0U;
    } else if (!q->has_window) {
        q->state = ECG_QUALITY_GOOD;
        q->good_windows = 0U;
    } else if (q->state != ECG_QUALITY_GOOD) {
        q->good_windows++;
        if (q->good_windows >= ECG_QUALITY_RECOVERY_WINDOWS) {
            q->state = ECG_QUALITY_GOOD;
            q->good_windows = 0U;
        } else {
            q->state = ECG_QUALITY_RECOVERING;
        }
    }

    q->has_window = 1U;
    reset_window(q);
}

void ecg_quality_step(EcgSignalQuality *q,
                      u8 adc,
                      float raw,
                      u8 lod,
                      u8 otr)
{
    float diff = 0.0f;

    if (q == 0) return;

    /* Do not reset this run at quality-window boundaries. Saturate it. */
    if ((q->constant_run != 0U) && (adc == q->previous_adc)) {
        if (q->constant_run < ECG_QUALITY_CONSTANT_RUN) q->constant_run++;
    } else {
        q->constant_run = 1U;
    }
    q->previous_adc = adc;

    if (q->count > 0U) {
        diff = abs_local(raw - q->prev_raw);
        q->sum_abs_diff += diff;
        if (diff < ECG_QUALITY_FLAT_DIFF) q->flat_count++;
        if (diff > ECG_QUALITY_SPIKE_DIFF) q->spike_count++;
    }

    q->prev_raw = raw;
    q->sum_adc += (u32)adc;
    if (raw < q->min_raw) q->min_raw = raw;
    if (raw > q->max_raw) q->max_raw = raw;
    if (lod) q->lod_count++;
    if (otr) q->otr_count++;
    if ((adc <= 2U) || (adc >= 253U)) {
        q->clip_count++;
        q->hard_clip_run++;
    } else {
        q->hard_clip_run = 0U;
    }
    if ((adc <= ECG_QUALITY_LOW_HEADROOM_CODE) ||
        (adc >= ECG_QUALITY_HIGH_HEADROOM_CODE)) {
        q->headroom_count++;
    }
    q->count++;

    if (lod && q->state != ECG_QUALITY_BAD) {
        q->state = ECG_QUALITY_BAD;
        q->reason = ECG_QUALITY_REASON_LOD;
        q->score = 0U;
        q->good_windows = 0U;
    }

    if (q->hard_clip_run >= ECG_QUALITY_HARD_CLIP_RUN) {
        q->state = ECG_QUALITY_BAD;
        q->reason |= ECG_QUALITY_REASON_CLIPPING;
        q->score = 0U;
        q->good_windows = 0U;
    }

    if (q->constant_run >= ECG_QUALITY_CONSTANT_RUN) {
        q->state = ECG_QUALITY_BAD;
        q->reason |= ECG_QUALITY_REASON_FLATLINE;
        q->score = 0U;
        q->good_windows = 0U;
    }

    if (q->count >= ECG_QUALITY_WINDOW_SAMPLES) {
        evaluate_window(q);
    }
}

u8 ecg_quality_is_good(const EcgSignalQuality *q)
{
    return (q != 0 && q->state == ECG_QUALITY_GOOD) ? 1U : 0U;
}

const char *ecg_quality_state_name(EcgQualityState state)
{
    switch (state) {
    case ECG_QUALITY_GOOD: return "GOOD";
    case ECG_QUALITY_BAD: return "BAD";
    case ECG_QUALITY_RECOVERING: return "RECOVERING";
    default: return "UNKNOWN";
    }
}

const char *ecg_quality_reason_name(u8 reason)
{
    if (reason == 0U) return "NONE";
    if (reason & ECG_QUALITY_REASON_LOD) return "LOD";
    if (reason & ECG_QUALITY_REASON_OTR) return "OTR";
    if (reason & ECG_QUALITY_REASON_CLIPPING) return "CLIPPED";
    if (reason & ECG_QUALITY_REASON_FLATLINE) return "FLAT";
    if (reason & ECG_QUALITY_REASON_SPIKES) return "SPIKES";
    if (reason & ECG_QUALITY_REASON_NOISE) return "NOISE";
    return "UNKNOWN";
}

const char *ecg_quality_warning_name(u8 warning)
{
    if (warning == 0U) return "NONE";
    if (warning & ECG_QUALITY_WARNING_LOW_HEADROOM) return "LOW_HEADROOM";
    return "UNKNOWN";
}
