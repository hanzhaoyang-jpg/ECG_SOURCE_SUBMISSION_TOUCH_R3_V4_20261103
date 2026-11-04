/* =========================================================
 * File: ecg_config.h
 * Replace the previous ecg_config.h with this complete version.
 * ========================================================= */
#ifndef ECG_CONFIG_H
#define ECG_CONFIG_H

#include "xparameters.h"

/*
 * ========= AD9280 Stream AXI IP Base Address =========
 *
 * IMPORTANT:
 * Replace this macro according to your xparameters.h.
 *
 * Common possible names:
 *   XPAR_AD9280_STREAM_AXI_0_BASEADDR
 *   XPAR_AD9280_STREAM_AXI_0_S_AXI_BASEADDR
 */
#ifndef AD9280_STREAM_BASEADDR
#define AD9280_STREAM_BASEADDR  XPAR_AD9280_STREAM_AXI_0_BASEADDR
#endif

/*
 * ========= AD9280 Stream Register Map =========
 */
#define AD_REG_CONTROL            0x00U
#define AD_REG_STATUS             0x04U
#define AD_REG_FIFO_COUNT         0x08U
#define AD_REG_FIFO_DATA          0x0CU
#define AD_REG_SAMPLE_COUNTER_L   0x10U
#define AD_REG_SAMPLE_COUNTER_H   0x14U
#define AD_REG_CLK_HALF_PERIOD    0x18U
#define AD_REG_DEBUG              0x1CU

/*
 * ========= CONTROL Register Bits =========
 */
#define AD_CTRL_CAPTURE_ENABLE    (1U << 0)
#define AD_CTRL_FIFO_CLEAR        (1U << 1)
#define AD_CTRL_OVERFLOW_CLEAR    (1U << 2)

/*
 * ========= STATUS Register Bits =========
 */
#define AD_ST_RUNNING             (1U << 0)
#define AD_ST_FIFO_EMPTY          (1U << 1)
#define AD_ST_FIFO_FULL           (1U << 2)
#define AD_ST_OVERFLOW            (1U << 3)
#define AD_ST_LOD                 (1U << 4)

/*
 * ========= FIFO_DATA Format =========
 *
 * bit31 : valid
 * bit15 : AD9280 OTR
 * bit14 : CN121 LOD
 * bit7:0: ADC sample
 */
#define AD_FIFO_VALID_MASK        0x80000000U
#define AD_FIFO_OTR_MASK          0x00008000U
#define AD_FIFO_LOD_MASK          0x00004000U
#define AD_FIFO_DATA_MASK         0x000000FFU

/*
 * ========= System Clock =========
 *
 * This should match the PL clock feeding the AXI IP.
 * Usually FCLK_CLK0 = 100 MHz.
 */
#define AD_STREAM_AXI_CLK_HZ      100000000U

/*
 * ========= ECG Sampling =========
 */
#define ECG_FS_HZ                 360U
#define AD_ADC_CLK_HZ             10000000U

/*
 * AD9280 runs continuously at 10 MHz. The PL capture core independently
 * selects samples at ECG_FS_HZ for the FIFO.
 */
#define AD_DEFAULT_HALF_PERIOD    (AD_STREAM_AXI_CLK_HZ / (2U * AD_ADC_CLK_HZ))

/*
 * ========= ECG Window Configuration =========
 */
#define BEAT_HALF_WIN             431U
#define BEAT_WIN_LEN              863U
#define ECG_PACK_LEN              864U

/*
 * Causal filter profile for fs=360 Hz. The replay tool reads these numeric
 * macros directly so board and offline evaluation use the same coefficients.
 */
#define ECG_DISPLAY_EMA_ALPHA           0.2000f
#define ECG_LEGACY_BASELINE_ALPHA       0.0010f
#define ECG_LEGACY_LP_ALPHA             0.1800f
#define ECG_QRS_HP_ALPHA                0.9164f
#define ECG_QRS_LP_ALPHA                0.2940f
#define ECG_QRS_MWI_LEN                 54U
#define ECG_NOTCH_B0                    0.9812087620f
#define ECG_NOTCH_B1                   -1.2614176695f
#define ECG_NOTCH_B2                    0.9812087620f
#define ECG_NOTCH_A1_FEEDBACK           1.2614176695f
#define ECG_NOTCH_A2_FEEDBACK          -0.9624175240f

#define LCD_SECONDS               5U
#define LCD_WIN_LEN               (ECG_FS_HZ * LCD_SECONDS)

/*
 * ========= Ring Buffer =========
 *
 * 4096 samples at 360 Hz = about 11.38 seconds.
 * Enough for:
 *   - 5 s display window
 *   - R peak future window
 *   - temporary processing delay
 */
#define ECG_RING_SIZE             4096U

/*
 * ========= Task Queue =========
 */
#define MAX_BEAT_TASKS            32U

/*
 * ========= R Peak Detection =========
 */
#define RPEAK_INIT_SECONDS        2U
#define RPEAK_INIT_SAMPLES        (ECG_FS_HZ * RPEAK_INIT_SECONDS)

#define ECG_RPEAK_THRESHOLD_SCALE      0.10f
#define ECG_RPEAK_MIN_ABS_QRS          0.040f
#define ECG_RPEAK_ENV_MEAN_ALPHA       0.0020f
#define ECG_RPEAK_ENV_PEAK_ATTACK      0.1000f
#define ECG_RPEAK_ENV_PEAK_DECAY       0.0005f
#define ECG_RPEAK_MIN_THRESHOLD        0.000001f

/*
 * Refractory period:
 * 200 ms at 360 Hz = 72 samples.
 * Prevent detecting the same QRS complex multiple times.
 */
#define RPEAK_REFRACTORY_MS       360U
#define RPEAK_REFRACTORY_SAMPLES  ((ECG_FS_HZ * RPEAK_REFRACTORY_MS) / 1000U)

/*
 * Reasonable RR range for debug.
 * These are not hard medical limits; only used to reject obvious false detections.
 */
#define RR_MIN_MS                 300U
#define RR_MAX_MS                 2000U
#define RR_MIN_SAMPLES            ((ECG_FS_HZ * RR_MIN_MS) / 1000U)
#define RR_MAX_SAMPLES            ((ECG_FS_HZ * RR_MAX_MS) / 1000U)

/*
 * ========= Block Read =========
 */
#define AD_READ_BURST_MAX         256

/*
 * ========= UART Print Control =========
 */
#define PRINT_EVERY_MS            500U
#define RPEAK_PRINT_ENABLE        1U
#define READY_BEAT_PRINT_ENABLE   1U

/*
 * OTR is recorded for hardware diagnosis, but the current board does not have
 * a trustworthy OTR connection. Keep it out of SQI decisions until the new
 * PCB connection has been verified. Set to 1U after that verification.
 */
#define ECG_SQI_USE_OTR           0U

/*
 * SQI profile. All thresholds operate on raw ADC codes or on the centered
 * raw signal in approximately [-1, 1]. Keep these values synchronized with
 * the web replay tool when a labeled parameter set is accepted.
 */
#define ECG_QUALITY_WINDOW_SECONDS       2U
#define ECG_QUALITY_WINDOW_SAMPLES       (ECG_QUALITY_WINDOW_SECONDS * ECG_FS_HZ)
#define ECG_QUALITY_RECOVERY_WINDOWS     2U
#define ECG_QUALITY_CLIP_PERCENT         5U
#define ECG_QUALITY_OTR_PERCENT          2U
#define ECG_QUALITY_FLAT_PERCENT         95U
#define ECG_QUALITY_SPIKE_PERCENT        20U
#define ECG_QUALITY_HARD_CLIP_RUN        8U
/* Exact constant code at any level, including an analog rail below 255. */
#define ECG_QUALITY_CONSTANT_RUN        72U
#define ECG_LOD_EVENT_STABLE_SAMPLES    36U
#define ECG_QUALITY_LOW_HEADROOM_CODE    16U
#define ECG_QUALITY_HIGH_HEADROOM_CODE   239U
#define ECG_QUALITY_HEADROOM_PERCENT     25U
#define ECG_QUALITY_MEAN_ADC_MIN         32U
#define ECG_QUALITY_MEAN_ADC_MAX         223U
#define ECG_QUALITY_FLAT_DIFF            0.0078125f
#define ECG_QUALITY_SPIKE_DIFF           0.20f
#define ECG_QUALITY_MIN_RANGE            0.02f
#define ECG_QUALITY_MAX_MEAN_ABS_DIFF    0.10f

/* Periodic acquisition-health report on the debug UART. */
#define ECG_ADC_DIAG_PRINT_MS     1000U

/*
 * Split long recordings into bounded sample CSV files. The first hour keeps
 * the legacy RECxxxx.CSV name; later hours use RECxxxx_S001.CSV, etc. This
 * avoids the FAT32 4 GiB single-file limit during 24-72 hour monitoring.
 */
#define ECG_REC_SEGMENT_SECONDS   3600U
#define ECG_REC_SEGMENT_SAMPLES   (ECG_REC_SEGMENT_SECONDS * ECG_FS_HZ)

#endif /* ECG_CONFIG_H */
