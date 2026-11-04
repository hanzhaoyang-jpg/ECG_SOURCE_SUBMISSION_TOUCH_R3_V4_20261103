/*
 * ecg_cnn_infer.c
 */

#include <string.h>

#ifdef ECG_CNN_HOST_TEST
#include "cnn_async_mock.h"
#else
#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xstatus.h"
#include "xaxidma.h"
#include "xtime_l.h"
#endif

#include "ecg_cnn_infer.h"

/* ===================== User-adjustable config ===================== */

#ifndef ECG_FS_HZ
#define ECG_FS_HZ 360U
#endif

/*
 * Keep this consistent with your SD-card test project.
 */
#define ECG_CNN_SCALE          0.003921568859368563f
#define ECG_CNN_ZERO_POINT    (-128)

/*
 * If your live ring buffer already stores filtered ECG, keep this 0.
 * If your live beat window is raw or too noisy, try setting this to 1.
 */
#define ECG_CNN_ENABLE_SMOOTH  0

/*
 * Timeout protection. Prevents software from hanging forever if DMA or HLS IP
 * does not respond.
 */
#define ECG_CNN_TIMEOUT_MS     2000U

/* ===================== d_cnn_3 AXI-Lite registers ===================== */

#define NN_BASEADDR           XPAR_D_CNN_3_0_S_AXI_AXILITES_BASEADDR

#define NN_REG_AP_CTRL        0x00U
#define NN_REG_RESULT_DATA    0x10U
#define NN_REG_RESULT_CTRL    0x14U

#define NN_AP_START           (1U << 0)
#define NN_AP_DONE            (1U << 1)
#define NN_AP_IDLE            (1U << 2)
#define NN_AP_READY           (1U << 3)

#define NN_RESULT_VLD         (1U << 0)

static inline void nn_write(u32 off, u32 val)
{
    Xil_Out32((UINTPTR)(NN_BASEADDR + off), val);
}

static inline u32 nn_read(u32 off)
{
    return Xil_In32((UINTPTR)(NN_BASEADDR + off));
}

/* ===================== AXI DMA ===================== */

#define ECG_CNN_DMA_DEV_ID    XPAR_AXIDMA_0_DEVICE_ID

static XAxiDma g_cnn_dma;
static int g_cnn_dma_ready = 0;
static int g_cnn_active = 0;
static int g_cnn_fault = 0;
static int g_cnn_done_seen = 0;
static int g_cnn_result_seen = 0;
static u8 g_cnn_result;
static XTime g_cnn_started;

/* ===================== Buffers ===================== */

static float  g_cnn_proc[ECG_CNN_PACK_LEN] __attribute__((aligned(64)));
static int8_t g_cnn_qbuf[ECG_CNN_PACK_LEN] __attribute__((aligned(64)));

/* ===================== Local helpers ===================== */

static inline float ecg_cnn_absf(float x)
{
    return (x < 0.0f) ? -x : x;
}

static inline int ecg_cnn_roundf_to_int(float x)
{
    return (int)((x >= 0.0f) ? (x + 0.5f) : (x - 0.5f));
}

#if ECG_CNN_ENABLE_SMOOTH
static void ecg_cnn_safe_filtfilt(float *x, u32 n)
{
    static float y[ECG_CNN_FLOAT_LEN];
    static float z[ECG_CNN_FLOAT_LEN];

    if (x == 0 || n <= 1U || n > ECG_CNN_FLOAT_LEN) {
        return;
    }

    const float alpha = 0.1f;

    y[0] = x[0];
    for (u32 i = 1U; i < n; i++) {
        y[i] = alpha * x[i] + (1.0f - alpha) * y[i - 1U];
    }

    z[n - 1U] = y[n - 1U];
    for (int i = (int)n - 2; i >= 0; i--) {
        z[i] = alpha * y[i] + (1.0f - alpha) * z[i + 1];
    }

    for (u32 i = 0U; i < n; i++) {
        x[i] = z[i];
    }
}
#endif

static void ecg_cnn_scale_data(float *x, u32 n)
{
    if (x == 0 || n == 0U) {
        return;
    }

    float minv = x[0];
    float maxv = x[0];

    for (u32 i = 1U; i < n; i++) {
        if (x[i] < minv) minv = x[i];
        if (x[i] > maxv) maxv = x[i];
    }

    float d = maxv - minv;

    if (ecg_cnn_absf(d) < 1.0e-6f) {
        d = 1.0f;
    }

    for (u32 i = 0U; i < n; i++) {
        x[i] = (x[i] - minv) / d;
    }
}

static void ecg_cnn_quantize_int8(const float *in, int8_t *out, u32 n)
{
    for (u32 i = 0U; i < n; i++) {
        int q = ecg_cnn_roundf_to_int(in[i] / ECG_CNN_SCALE) + ECG_CNN_ZERO_POINT;

        if (q > 127) {
            q = 127;
        } else if (q < -128) {
            q = -128;
        }

        out[i] = (int8_t)q;
    }
}

static int ecg_cnn_timeout_expired(XTime start)
{
    XTime now;
    u64 elapsed_ms;

    XTime_GetTime(&now);
    elapsed_ms = ((u64)(now - start) * 1000ULL) / (u64)COUNTS_PER_SECOND;

    return (elapsed_ms >= ECG_CNN_TIMEOUT_MS) ? 1 : 0;
}

static int ecg_cnn_reset_dma(void)
{
    XTime start;

    XAxiDma_Reset(&g_cnn_dma);
    XTime_GetTime(&start);

    while (!XAxiDma_ResetIsDone(&g_cnn_dma)) {
        if (ecg_cnn_timeout_expired(start)) {
            xil_printf("[CNN] ERROR: DMA recovery reset timeout\r\n");
            g_cnn_dma_ready = 0;
            return XST_FAILURE;
        }
    }

    return XST_SUCCESS;
}

/* ===================== Public API ===================== */

int ecg_cnn_infer_init(void)
{
    XAxiDma_Config *cfg;
    int st;

    /* DMA reset alone cannot recover a partly-fed HLS transaction. */
    if (g_cnn_active || g_cnn_fault) return XST_FAILURE;

    cfg = XAxiDma_LookupConfig(ECG_CNN_DMA_DEV_ID);
    if (cfg == 0) {
        xil_printf("[CNN] ERROR: XAxiDma_LookupConfig failed, dev_id=%d\r\n",
                   ECG_CNN_DMA_DEV_ID);
        return XST_FAILURE;
    }

    st = XAxiDma_CfgInitialize(&g_cnn_dma, cfg);
    if (st != XST_SUCCESS) {
        xil_printf("[CNN] ERROR: XAxiDma_CfgInitialize failed: %d\r\n", st);
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(&g_cnn_dma)) {
        xil_printf("[CNN] ERROR: AXI DMA is SG mode; simple mode is required\r\n");
        return XST_FAILURE;
    }

    XAxiDma_Reset(&g_cnn_dma);

    if (ecg_cnn_reset_dma() != XST_SUCCESS) {
        return XST_FAILURE;
    }

    g_cnn_dma_ready = 1;

    xil_printf("[CNN] init OK: NN_BASE=0x%08lx DMA_DEV_ID=%d input_len=%lu bytes\r\n",
               (u32)NN_BASEADDR,
               ECG_CNN_DMA_DEV_ID,
               (u32)ECG_CNN_PACK_LEN);

    return 0;
}

int ecg_cnn_infer_available(void)
{
    return g_cnn_dma_ready && !g_cnn_active && !g_cnn_fault;
}

int ecg_cnn_infer_start(const float *beat_filt, u32 rr_samples)
{
    if (beat_filt == 0 || !ecg_cnn_infer_available()) {
        return XST_FAILURE;
    }

    if (!g_cnn_dma_ready) {
        xil_printf("[CNN] ERROR: ecg_cnn_infer_init() not called\r\n");
        return XST_FAILURE;
    }

    /*
     * 1. Copy 863 ECG samples.
     *
     * Important:
     *   This assumes beat_filt is already arranged as a centered beat:
     *     beat[0]   = R - 431
     *     beat[431] = R
     *     beat[862] = R + 431
     */
    for (u32 i = 0U; i < ECG_CNN_FLOAT_LEN; i++) {
        g_cnn_proc[i] = beat_filt[i];
    }

#if ECG_CNN_ENABLE_SMOOTH
    ecg_cnn_safe_filtfilt(g_cnn_proc, ECG_CNN_FLOAT_LEN);
#endif

    /*
     * 2. Normalize only ECG samples, same as SD-card project.
     */
    ecg_cnn_scale_data(g_cnn_proc, ECG_CNN_FLOAT_LEN);

    /*
     * 3. Append RR feature.
     *
     * In the SD-card project:
     *   EcgProcBuf[863] = (rpos[1] - rpos[0]) / 360.0f;
     *
     * In the live system, we already know RR from the R-peak detector:
     *   rr_samples = current_r_index - previous_r_index
     */
    if (rr_samples == 0U) {
        g_cnn_proc[ECG_CNN_FLOAT_LEN] = 0.0f;
    } else {
        g_cnn_proc[ECG_CNN_FLOAT_LEN] = ((float)rr_samples) / ((float)ECG_FS_HZ);
    }

    /*
     * 4. Quantize 864 floats to int8.
     */
    ecg_cnn_quantize_int8(g_cnn_proc, g_cnn_qbuf, ECG_CNN_PACK_LEN);

    /*
     * 5. Flush int8 input buffer before DMA MM2S.
     */
    Xil_DCacheFlushRange((UINTPTR)g_cnn_qbuf, ECG_CNN_PACK_LEN);

    /*
     * 6. Start HLS IP then feed stream through DMA.
     *
     * This follows your SD-card test project order:
     *   nn_write(AP_CTRL, AP_START);
     *   XAxiDma_SimpleTransfer(... DMA_TO_DEVICE);
     */
    /* Consume stale clear-on-read flags before starting the new job. */
    (void)nn_read(NN_REG_AP_CTRL);
    (void)nn_read(NN_REG_RESULT_CTRL);
    g_cnn_done_seen = 0;
    g_cnn_result_seen = 0;
    XTime_GetTime(&g_cnn_started);
    g_cnn_active = 1;
    nn_write(NN_REG_AP_CTRL, NN_AP_START);

    int st = XAxiDma_SimpleTransfer(&g_cnn_dma,
                                    (UINTPTR)g_cnn_qbuf,
                                    ECG_CNN_PACK_LEN,
                                    XAXIDMA_DMA_TO_DEVICE);
    if (st != XST_SUCCESS) {
        xil_printf("[CNN] ERROR: XAxiDma_SimpleTransfer failed: %d\r\n", st);
        g_cnn_fault = 1;
        g_cnn_active = 0;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int ecg_cnn_infer_poll(u8 *out_cls)
{
    if (!g_cnn_active || out_cls == 0) return -1;
    /* Latch flags independently; AP_DONE and result_vld may clear on read. */
    if (nn_read(NN_REG_AP_CTRL) & NN_AP_DONE) g_cnn_done_seen = 1;
    if (nn_read(NN_REG_RESULT_CTRL) & NN_RESULT_VLD) {
        g_cnn_result = (u8)(nn_read(NN_REG_RESULT_DATA) & 0xFFU);
        g_cnn_result_seen = 1;
    }
    if (g_cnn_done_seen && g_cnn_result_seen &&
        !XAxiDma_Busy(&g_cnn_dma, XAXIDMA_DMA_TO_DEVICE)) {
        g_cnn_active = 0;
        if (!ecg_result_class_is_valid(g_cnn_result)) {
            *out_cls = ECG_CLASS_UNKNOWN;
            return -1;
        }
        *out_cls = g_cnn_result;
        return 1;
    }
    if (ecg_cnn_timeout_expired(g_cnn_started)) {
        /* Do not reuse an input buffer that a stalled DMA may still read. */
        g_cnn_fault = 1;
        g_cnn_active = 0;
        xil_printf("[CNN] asynchronous timeout; CNN disabled until board reset\r\n");
        return -1;
    }
    return 0;
}

int ecg_cnn_infer_run_beat(const float *beat_filt, u32 rr_samples, u8 *out_cls)
{
    int rc;
    if (out_cls == 0 || ecg_cnn_infer_start(beat_filt, rr_samples) != 0)
        return XST_FAILURE;
    do { rc = ecg_cnn_infer_poll(out_cls); } while (rc == 0);
    return rc == 1 ? XST_SUCCESS : XST_FAILURE;
}

const char *ecg_cnn_class_name(u8 cls)
{
    return ecg_result_class_name(cls);
}

