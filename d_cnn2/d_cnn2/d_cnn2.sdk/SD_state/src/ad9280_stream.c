/* =========================================================
 * File: ad9280_stream.c
 * ========================================================= */
#include "ad9280_stream.h"
#include "ecg_config.h"

#include "xil_io.h"
#include "xil_printf.h"

#define AD_REG32(off)   Xil_In32(AD9280_STREAM_BASEADDR + (off))
#define AD_WR32(off, v) Xil_Out32(AD9280_STREAM_BASEADDR + (off), (v))

static inline u32 reg_read(u32 off)
{
    return AD_REG32(off);
}

static inline void reg_write(u32 off, u32 val)
{
    AD_WR32(off, val);
}

int ad9280_stream_init(void)
{
    /*
     * 1) Disable capture
     * 2) Set sample clock half period
     * 3) Clear FIFO
     * 4) Clear overflow
     * 5) Keep disabled, let main decide when to enable
     */
    ad9280_stream_enable(0);
    ad9280_stream_set_half_period(AD_DEFAULT_HALF_PERIOD);
    ad9280_stream_clear_fifo();
    ad9280_stream_clear_overflow();
    ad9280_stream_enable(0);

    return 0;
}

void ad9280_stream_set_half_period(u32 half_period)
{
    if (half_period < 2U) {
        half_period = AD_DEFAULT_HALF_PERIOD;
    }
    reg_write(AD_REG_CLK_HALF_PERIOD, half_period);
}

u32 ad9280_stream_get_half_period(void)
{
    return reg_read(AD_REG_CLK_HALF_PERIOD);
}

void ad9280_stream_enable(int en)
{
    if (en) {
        reg_write(AD_REG_CONTROL, AD_CTRL_CAPTURE_ENABLE);
    } else {
        reg_write(AD_REG_CONTROL, 0U);
    }
}

void ad9280_stream_clear_fifo(void)
{
    /*
     * Pulse write:
     * if capture currently enabled, preserve enable bit
     */
    u32 ctrl = ad9280_stream_is_running() ? AD_CTRL_CAPTURE_ENABLE : 0U;
    reg_write(AD_REG_CONTROL, ctrl | AD_CTRL_FIFO_CLEAR);
    reg_write(AD_REG_CONTROL, ctrl);
}

void ad9280_stream_clear_overflow(void)
{
    u32 ctrl = ad9280_stream_is_running() ? AD_CTRL_CAPTURE_ENABLE : 0U;
    reg_write(AD_REG_CONTROL, ctrl | AD_CTRL_OVERFLOW_CLEAR);
    reg_write(AD_REG_CONTROL, ctrl);
}

u32 ad9280_stream_get_status(void)
{
    return reg_read(AD_REG_STATUS);
}

u32 ad9280_stream_get_fifo_count(void)
{
    return reg_read(AD_REG_FIFO_COUNT) & 0xFFFFU;
}

u64 ad9280_stream_get_sample_counter(void)
{
    u32 lo1, hi1, lo2, hi2;

    /*
     * Read stable 64-bit value
     */
    do {
        hi1 = reg_read(AD_REG_SAMPLE_COUNTER_H);
        lo1 = reg_read(AD_REG_SAMPLE_COUNTER_L);
        hi2 = reg_read(AD_REG_SAMPLE_COUNTER_H);
        lo2 = reg_read(AD_REG_SAMPLE_COUNTER_L);
    } while ((hi1 != hi2) || (lo1 != lo2));

    return (((u64)hi1) << 32) | (u64)lo1;
}

u32 ad9280_stream_read_raw_word(void)
{
    return reg_read(AD_REG_FIFO_DATA);
}

int ad9280_stream_read_sample(Ad9280Sample *s)
{
    u32 w;

    if (s == 0) return 0;

    w = ad9280_stream_read_raw_word();

    s->valid = (w & AD_FIFO_VALID_MASK) ? 1U : 0U;
    s->otr   = (w & AD_FIFO_OTR_MASK)   ? 1U : 0U;
    s->lod   = (w & AD_FIFO_LOD_MASK)   ? 1U : 0U;
    s->adc   = (u8)(w & AD_FIFO_DATA_MASK);

    return s->valid ? 1 : 0;
}

int ad9280_stream_read_available(Ad9280Sample *buf, int max_n)
{
    u32 cnt;
    int n = 0;

    if (buf == 0 || max_n <= 0) return 0;

    cnt = ad9280_stream_get_fifo_count();
    if ((u32)max_n < cnt) {
        cnt = (u32)max_n;
    }

    while (n < (int)cnt) {
        if (!ad9280_stream_read_sample(&buf[n])) {
            break;
        }
        n++;
    }

    return n;
}

int ad9280_stream_is_running(void)
{
    return (ad9280_stream_get_status() & AD_ST_RUNNING) ? 1 : 0;
}

int ad9280_stream_is_fifo_empty(void)
{
    return (ad9280_stream_get_status() & AD_ST_FIFO_EMPTY) ? 1 : 0;
}

int ad9280_stream_is_fifo_full(void)
{
    return (ad9280_stream_get_status() & AD_ST_FIFO_FULL) ? 1 : 0;
}

int ad9280_stream_has_overflow(void)
{
    return (ad9280_stream_get_status() & AD_ST_OVERFLOW) ? 1 : 0;
}

int ad9280_stream_lod_active(void)
{
    return (ad9280_stream_get_status() & AD_ST_LOD) ? 1 : 0;
}

float ad9280_sample_to_float_centered(u8 adc_code)
{
    /*
     * Map 8-bit code [0,255] to roughly [-1.0, +1.0)
     * useful for quick debug view or later ECG pipeline.
     */
    return ((float)adc_code - 128.0f) / 128.0f;
}

void ad9280_stream_dump_status(void)
{
    u32 st   = ad9280_stream_get_status();
    u32 cnt  = ad9280_stream_get_fifo_count();
    u32 half = ad9280_stream_get_half_period();
    u32 scl  = Xil_In32(AD9280_STREAM_BASEADDR + AD_REG_SAMPLE_COUNTER_L);
    u32 sch  = Xil_In32(AD9280_STREAM_BASEADDR + AD_REG_SAMPLE_COUNTER_H);

    xil_printf("[AD9280] STATUS=0x%08lx RUN=%d EMPTY=%d FULL=%d OVR=%d LOD=%d FIFO=%lu HALF=%lu SCNT_H=0x%08lx SCNT_L=0x%08lx\r\n",
               st,
               (st & AD_ST_RUNNING) ? 1 : 0,
               (st & AD_ST_FIFO_EMPTY) ? 1 : 0,
               (st & AD_ST_FIFO_FULL) ? 1 : 0,
               (st & AD_ST_OVERFLOW) ? 1 : 0,
               (st & AD_ST_LOD) ? 1 : 0,
               cnt,
               half,
               sch,
               scl);
}
