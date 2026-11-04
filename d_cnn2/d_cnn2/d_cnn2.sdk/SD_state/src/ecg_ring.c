#include "ecg_ring.h"

void ecg_ring_init(EcgRing *r)
{
    u32 i;

    if (r == 0) return;

    for (i = 0; i < ECG_RING_SIZE; i++) {
        r->raw[i] = 0.0f;
        r->filt[i] = 0.0f;
    }

    r->wr = 0U;
    r->total_samples = 0ULL;
}

void ecg_ring_push(EcgRing *r, float raw, float filt)
{
    if (r == 0) return;

    r->raw[r->wr] = raw;
    r->filt[r->wr] = filt;

    r->wr++;
    if (r->wr >= ECG_RING_SIZE) {
        r->wr = 0U;
    }

    r->total_samples++;
}

u64 ecg_ring_oldest_index(const EcgRing *r)
{
    if (r == 0) return 0ULL;

    if (r->total_samples <= ECG_RING_SIZE) {
        return 0ULL;
    }

    return r->total_samples - ECG_RING_SIZE;
}

u64 ecg_ring_newest_index(const EcgRing *r)
{
    if (r == 0 || r->total_samples == 0ULL) {
        return 0ULL;
    }

    return r->total_samples - 1ULL;
}

int ecg_ring_has_index(const EcgRing *r, u64 gidx)
{
    u64 oldest;
    u64 newest;

    if (r == 0) return 0;
    if (r->total_samples == 0ULL) return 0;

    oldest = ecg_ring_oldest_index(r);
    newest = ecg_ring_newest_index(r);

    if (gidx < oldest) return 0;
    if (gidx > newest) return 0;

    return 1;
}

static u32 ecg_ring_pos_from_gidx(u64 gidx)
{
    return (u32)(gidx % ECG_RING_SIZE);
}

int ecg_ring_get_raw(const EcgRing *r, u64 gidx, float *out)
{
    u32 pos;

    if (r == 0 || out == 0) return 0;
    if (!ecg_ring_has_index(r, gidx)) return 0;

    pos = ecg_ring_pos_from_gidx(gidx);
    *out = r->raw[pos];

    return 1;
}

int ecg_ring_get_filt(const EcgRing *r, u64 gidx, float *out)
{
    u32 pos;

    if (r == 0 || out == 0) return 0;
    if (!ecg_ring_has_index(r, gidx)) return 0;

    pos = ecg_ring_pos_from_gidx(gidx);
    *out = r->filt[pos];

    return 1;
}

int ecg_ring_copy_raw_window(const EcgRing *r, u64 start_gidx, float *dst, int n)
{
    int i;

    if (r == 0 || dst == 0 || n <= 0) return 0;

    for (i = 0; i < n; i++) {
        if (!ecg_ring_get_raw(r, start_gidx + (u64)i, &dst[i])) {
            return 0;
        }
    }

    return 1;
}

int ecg_ring_copy_filt_window(const EcgRing *r, u64 start_gidx, float *dst, int n)
{
    int i;

    if (r == 0 || dst == 0 || n <= 0) return 0;

    for (i = 0; i < n; i++) {
        if (!ecg_ring_get_filt(r, start_gidx + (u64)i, &dst[i])) {
            return 0;
        }
    }

    return 1;
}

int ecg_ring_copy_filt_beat_window(const EcgRing *r,
                                   u64 r_index,
                                   float *dst,
                                   int n)
{
    u64 start_gidx;

    if (r == 0 || dst == 0) {
        return 0;
    }

    if (n != (int)BEAT_WIN_LEN) {
        return 0;
    }

    if (r_index < (u64)BEAT_HALF_WIN) {
        return 0;
    }

    start_gidx = r_index - (u64)BEAT_HALF_WIN;

    return ecg_ring_copy_filt_window(r, start_gidx, dst, n);
}
