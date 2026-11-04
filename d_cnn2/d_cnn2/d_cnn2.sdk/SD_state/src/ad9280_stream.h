/* =========================================================
 * File: ad9280_stream.h
 * ========================================================= */
#ifndef AD9280_STREAM_H
#define AD9280_STREAM_H

#include "xil_types.h"

typedef struct {
    u8  adc;
    u8  otr;
    u8  lod;
    u8  valid;
} Ad9280Sample;

int  ad9280_stream_init(void);
void ad9280_stream_set_half_period(u32 half_period);
u32  ad9280_stream_get_half_period(void);

void ad9280_stream_enable(int en);
void ad9280_stream_clear_fifo(void);
void ad9280_stream_clear_overflow(void);

u32  ad9280_stream_get_status(void);
u32  ad9280_stream_get_fifo_count(void);
u64  ad9280_stream_get_sample_counter(void);

u32  ad9280_stream_read_raw_word(void);
int  ad9280_stream_read_sample(Ad9280Sample *s);
int  ad9280_stream_read_available(Ad9280Sample *buf, int max_n);

int  ad9280_stream_is_running(void);
int  ad9280_stream_is_fifo_empty(void);
int  ad9280_stream_is_fifo_full(void);
int  ad9280_stream_has_overflow(void);
int  ad9280_stream_lod_active(void);

float ad9280_sample_to_float_centered(u8 adc_code);
void  ad9280_stream_dump_status(void);

#endif /* AD9280_STREAM_H */
