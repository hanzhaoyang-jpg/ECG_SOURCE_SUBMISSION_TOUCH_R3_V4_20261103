/*
 * ecg_cnn_infer.h
 *
 * Hardware CNN inference wrapper for live ECG beats.
 *
 * Input format:
 *   - 863 float ECG samples
 *   - 1 RR feature appended as sample[863]
 *   - total 864 int8 values streamed to d_cnn_3 through AXI DMA MM2S
 *
 * Preprocess:
 *   - optional simple forward/backward smoothing
 *   - min-max normalization to [0, 1]
 *   - RR feature = rr_samples / ECG_FS_HZ
 *   - quantization: q = round(x / SCALE) + ZERO_POINT
 */

#ifndef ECG_CNN_INFER_H
#define ECG_CNN_INFER_H

#include "xil_types.h"
#include "ecg_config.h"
#include "ecg_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECG_CNN_FLOAT_LEN      BEAT_WIN_LEN
#define ECG_CNN_PACK_LEN       ECG_PACK_LEN

#if ECG_CNN_PACK_LEN != (ECG_CNN_FLOAT_LEN + 1U)
#error "CNN input must contain the ECG beat window followed by one RR feature"
#endif

int ecg_cnn_infer_init(void);
/* One owned DMA buffer; start returns 0 on success. Poll never waits:
 * 0=pending, 1=complete, -1=error/no job. Faults latch until board reset.
 * Discard obsolete results in the caller, but keep polling until completion.
 */
int ecg_cnn_infer_start(const float *beat_filt, u32 rr_samples);
int ecg_cnn_infer_poll(u8 *out_cls);
int ecg_cnn_infer_available(void);

/*
 * Run one CNN inference.
 *
 * beat_filt:
 *   863-point beat window. Recommended window is:
 *     [R - 431, R + 431]
 *
 * rr_samples:
 *   RR interval in samples, usually:
 *     current_r_index - previous_r_index
 *
 * out_cls:
 *   CNN output class, low 8 bits from d_cnn_3 result register.
 *
 * return:
 *   XST_SUCCESS or XST_FAILURE
 */
int ecg_cnn_infer_run_beat(const float *beat_filt,
                           u32 rr_samples,
                           u8 *out_cls);

/*
 * Optional helper for printing class name.
 */
const char *ecg_cnn_class_name(u8 cls);

#ifdef __cplusplus
}
#endif

#endif /* ECG_CNN_INFER_H */
