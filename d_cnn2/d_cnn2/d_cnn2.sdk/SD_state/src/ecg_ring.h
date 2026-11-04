#ifndef ECG_RING_H
#define ECG_RING_H

#include "xil_types.h"
#include "ecg_config.h"

typedef struct {
    float raw[ECG_RING_SIZE];
    float filt[ECG_RING_SIZE];

    /*
     * wr points to the next write position.
     */
    u32 wr;

    /*
     * Total number of samples pushed since system start.
     * Global sample index of the newest sample is:
     *   total_samples - 1
     */
    u64 total_samples;
} EcgRing;

void ecg_ring_init(EcgRing *r);
void ecg_ring_push(EcgRing *r, float raw, float filt);

int  ecg_ring_has_index(const EcgRing *r, u64 gidx);
int  ecg_ring_get_raw(const EcgRing *r, u64 gidx, float *out);
int  ecg_ring_get_filt(const EcgRing *r, u64 gidx, float *out);

int  ecg_ring_copy_raw_window(const EcgRing *r, u64 start_gidx, float *dst, int n);
int  ecg_ring_copy_filt_window(const EcgRing *r, u64 start_gidx, float *dst, int n);

/*
 * Copy a beat-centered filtered window:
 *   [r_index - BEAT_HALF_WIN, r_index + BEAT_HALF_WIN]
 * Total length must be BEAT_WIN_LEN.
 */
int  ecg_ring_copy_filt_beat_window(const EcgRing *r,
                                    u64 r_index,
                                    float *dst,
                                    int n);

u64  ecg_ring_oldest_index(const EcgRing *r);
u64  ecg_ring_newest_index(const EcgRing *r);

#endif /* ECG_RING_H */
