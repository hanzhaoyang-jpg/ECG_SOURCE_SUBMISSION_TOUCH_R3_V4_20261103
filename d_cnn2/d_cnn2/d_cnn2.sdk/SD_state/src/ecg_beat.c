/* =========================================================
 * File: ecg_beat.c
 * ========================================================= */
#include "ecg_beat.h"

static void beat_task_clear(BeatTask *t)
{
    if (t == 0) return;

    t->state = BEAT_EMPTY;
    t->r_index = 0ULL;
    t->prev_r_index = 0ULL;
    t->rr_samples = 0U;
    t->rr_ms = 0U;
    t->hr_bpm = 0U;
    t->cls = 0U;
    t->printed = 0U;
}

void beat_queue_init(BeatQueue *q)
{
    u32 i;

    if (q == 0) return;

    for (i = 0; i < MAX_BEAT_TASKS; i++) {
        beat_task_clear(&q->tasks[i]);
    }

    q->next_insert = 0U;
}

void beat_queue_clear(BeatQueue *q)
{
    beat_queue_init(q);
}

u32 beat_task_rr_ms(u32 rr_samples)
{
    if (rr_samples == 0U) return 0U;
    return (rr_samples * 1000U) / ECG_FS_HZ;
}

u32 beat_task_hr_bpm(u32 rr_samples)
{
    if (rr_samples == 0U) return 0U;
    return (60U * ECG_FS_HZ) / rr_samples;
}

static BeatTask* beat_queue_find_free(BeatQueue *q)
{
    u32 i;

    if (q == 0) return 0;

    /*
     * First try from next_insert.
     */
    for (i = 0; i < MAX_BEAT_TASKS; i++) {
        u32 idx = (q->next_insert + i) % MAX_BEAT_TASKS;
        if (q->tasks[idx].state == BEAT_EMPTY ||
            q->tasks[idx].state == BEAT_DONE ||
            q->tasks[idx].state == BEAT_ERROR) {
            q->next_insert = (idx + 1U) % MAX_BEAT_TASKS;
            return &q->tasks[idx];
        }
    }

    return 0;
}

int beat_queue_add(BeatQueue *q, u64 r_idx, u64 prev_r_idx)
{
    BeatTask *t;
    u32 rr = 0U;

    if (q == 0) return 0;

    t = beat_queue_find_free(q);
    if (t == 0) {
        return 0;
    }

    t->state = BEAT_WAIT_FUTURE;
    t->r_index = r_idx;
    t->prev_r_index = prev_r_idx;

    if (prev_r_idx != 0ULL && r_idx > prev_r_idx) {
        rr = (u32)(r_idx - prev_r_idx);
    } else {
        rr = 0U;
    }

    t->rr_samples = rr;
    t->rr_ms = beat_task_rr_ms(rr);
    t->hr_bpm = beat_task_hr_bpm(rr);
    t->cls = 0U;
    t->printed = 0U;

    return 1;
}

void beat_queue_update_ready(BeatQueue *q, u64 total_samples)
{
    u32 i;

    if (q == 0) return;

    for (i = 0; i < MAX_BEAT_TASKS; i++) {
        BeatTask *t = &q->tasks[i];

        if (t->state == BEAT_WAIT_FUTURE) {
            /*
             * A beat becomes ready only after samples up to:
             *   r_index + BEAT_HALF_WIN
             * have arrived.
             *
             * Since total_samples is one past newest index,
             * condition is:
             *   total_samples > r_index + BEAT_HALF_WIN
             */
            if (total_samples > (t->r_index + (u64)BEAT_HALF_WIN)) {
                t->state = BEAT_READY;
            }
        }
    }
}

BeatTask* beat_queue_get_next_ready(BeatQueue *q)
{
    u32 i;

    if (q == 0) return 0;

    for (i = 0; i < MAX_BEAT_TASKS; i++) {
        if (q->tasks[i].state == BEAT_READY) {
            return &q->tasks[i];
        }
    }

    return 0;
}

void beat_queue_mark_running(BeatTask *t)
{
    if (t == 0) return;
    t->state = BEAT_RUNNING;
}

void beat_queue_mark_done(BeatTask *t, u8 cls)
{
    if (t == 0) return;

    t->cls = cls;
    t->state = BEAT_DONE;
}

void beat_queue_mark_error(BeatTask *t)
{
    if (t == 0) return;
    t->state = BEAT_ERROR;
}

void beat_queue_gc(BeatQueue *q, u64 oldest_index_to_keep)
{
    u32 i;

    if (q == 0) return;

    for (i = 0; i < MAX_BEAT_TASKS; i++) {
        BeatTask *t = &q->tasks[i];

        if (t->state == BEAT_DONE || t->state == BEAT_ERROR) {
            if (t->r_index < oldest_index_to_keep) {
                beat_task_clear(t);
            }
        }
    }
}

