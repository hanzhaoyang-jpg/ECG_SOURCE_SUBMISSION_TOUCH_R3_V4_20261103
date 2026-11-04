#ifndef ECG_NET_H
#define ECG_NET_H

#include "xil_types.h"

#define ECG_NET_BOARD_IP0       192U
#define ECG_NET_BOARD_IP1       168U
#define ECG_NET_BOARD_IP2       7U
#define ECG_NET_BOARD_IP3       10U

#define ECG_NET_PEER_IP0        192U
#define ECG_NET_PEER_IP1        168U
#define ECG_NET_PEER_IP2        7U
#define ECG_NET_PEER_IP3        1U

#define ECG_NET_UDP_PORT        5005U
#define ECG_NET_FLAG_LOD        0x01U
#define ECG_NET_FLAG_OTR        0x02U

int ecg_net_init(void);
void ecg_net_poll(u32 now_ms);
u8 ecg_net_is_ready(void);

void ecg_net_send_wave_sample(u64 sample_index,
                              u32 timestamp_ms,
                              s16 filtered,
                              u8 class_id,
                              u8 flags);

void ecg_net_send_cnn_event(u64 sample_index,
                            u32 timestamp_ms,
                            u32 rr_ms,
                            u32 hr_bpm,
                            u8 class_id,
                            u8 confidence);

u32 ecg_net_abnormal_count(void);

#endif /* ECG_NET_H */
