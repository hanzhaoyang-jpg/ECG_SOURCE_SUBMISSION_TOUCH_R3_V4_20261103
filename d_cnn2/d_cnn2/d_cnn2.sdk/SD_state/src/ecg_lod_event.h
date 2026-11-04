#ifndef ECG_LOD_EVENT_H
#define ECG_LOD_EVENT_H
#include "ecg_config.h"
#include "xil_types.h"

typedef struct {
    u32 run;
    u8 candidate;
    u8 stable;
} EcgLodEvent;

/* Only event reporting is debounced. Raw samples and SQI are unchanged. */
static int ecg_lod_event_step(EcgLodEvent *s, u8 raw)
{
    raw = raw ? 1U : 0U;
    if (raw != s->candidate || s->run == 0U) {
        s->candidate = raw;
        s->run = 1U;
    } else if (s->run < ECG_LOD_EVENT_STABLE_SAMPLES) {
        s->run++;
    }
    if (s->run >= ECG_LOD_EVENT_STABLE_SAMPLES && s->stable != raw) {
        s->stable = raw;
        return 1;
    }
    return 0;
}
#endif
