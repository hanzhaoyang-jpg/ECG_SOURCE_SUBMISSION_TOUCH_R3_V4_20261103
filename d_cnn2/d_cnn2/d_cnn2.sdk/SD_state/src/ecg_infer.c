#include "ecg_infer.h"
#include "ecg_config.h"

static float g_beat_buf[BEAT_WIN_LEN];

int ecg_infer_init(void)
{
    return 0;
}

int ecg_infer_run_beat(const EcgRing *ring,
                       const BeatTask *beat,
                       EcgInferResult *result)
{
    if (ring == 0 || beat == 0 || result == 0) {
        return -1;
    }

    if (!ecg_ring_copy_filt_beat_window(ring,
                                        beat->r_index,
                                        g_beat_buf,
                                        (int)BEAT_WIN_LEN)) {
        result->valid = 0U;
        result->class_id = ECG_CLASS_UNKNOWN;
        result->confidence_percent = 0U;
        return -2;
    }

    /*
     * Stub inference logic for pipeline bring-up:
     *   rr_ms == 0              -> UNKNOWN
     *   hr_bpm > 110 or < 50    -> UNKNOWN
     *   otherwise               -> NORMAL
     *
     * This is only for software pipeline verification. Heart rate alone
     * cannot assign an AAMI morphology class safely.
     */
    result->valid = 1U;

    if (beat->rr_ms == 0U) {
        result->class_id = ECG_CLASS_UNKNOWN;
    } else if (beat->hr_bpm > 110U || beat->hr_bpm < 50U) {
        result->class_id = ECG_CLASS_UNKNOWN;
    } else {
        result->class_id = ECG_CLASS_NORMAL;
    }
    result->confidence_percent = ECG_CONFIDENCE_UNAVAILABLE;

    return 0;
}

