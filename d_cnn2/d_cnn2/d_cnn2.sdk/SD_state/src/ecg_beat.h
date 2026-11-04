/* =========================================================
 * File: ecg_beat.h
 * ========================================================= */
#ifndef ECG_BEAT_H
#define ECG_BEAT_H

#include "xil_types.h"
#include "ecg_config.h"

typedef enum {
    BEAT_EMPTY = 0,
    BEAT_WAIT_FUTURE = 1,
    BEAT_READY = 2,
    BEAT_RUNNING = 3,
    BEAT_DONE = 4,
    BEAT_ERROR = 5
} BeatState;

typedef struct {
    BeatState state;

    u64 r_index;
    u64 prev_r_index;

    u32 rr_samples;
    u32 rr_ms;
    u32 hr_bpm;

    /*
     * CNN class will be filled in later.
     * For this second batch, cls is unused.
     */
    u8 cls;

    /*
     * For debug print.
     */
    u8 printed;
} BeatTask;

typedef struct {
    BeatTask tasks[MAX_BEAT_TASKS];
    u32 next_insert;
} BeatQueue;

void beat_queue_init(BeatQueue *q);
void beat_queue_clear(BeatQueue *q);

int  beat_queue_add(BeatQueue *q, u64 r_idx, u64 prev_r_idx);
void beat_queue_update_ready(BeatQueue *q, u64 total_samples);

BeatTask* beat_queue_get_next_ready(BeatQueue *q);
void beat_queue_mark_running(BeatTask *t);
void beat_queue_mark_done(BeatTask *t, u8 cls);
void beat_queue_mark_error(BeatTask *t);

void beat_queue_gc(BeatQueue *q, u64 oldest_index_to_keep);

u32 beat_task_rr_ms(u32 rr_samples);
u32 beat_task_hr_bpm(u32 rr_samples);

#endif /* ECG_BEAT_H */
