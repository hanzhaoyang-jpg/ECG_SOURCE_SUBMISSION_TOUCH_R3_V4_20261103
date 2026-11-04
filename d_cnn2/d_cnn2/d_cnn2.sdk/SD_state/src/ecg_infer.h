#ifndef ECG_INFER_H
#define ECG_INFER_H

#include "ecg_ring.h"
#include "ecg_beat.h"
#include "ecg_result.h"

int ecg_infer_init(void);

int ecg_infer_run_beat(const EcgRing *ring,
                       const BeatTask *beat,
                       EcgInferResult *result);

#endif /* ECG_INFER_H */
