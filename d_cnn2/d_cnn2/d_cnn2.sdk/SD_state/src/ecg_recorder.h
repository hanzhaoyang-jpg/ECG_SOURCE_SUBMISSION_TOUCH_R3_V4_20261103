#ifndef ECG_RECORDER_H
#define ECG_RECORDER_H

#include "xil_types.h"
#include "device_state.h"

/*typedef struct {
    u64 sample_index;
    u32 timestamp_ms;
    u8  raw;
    u8  lod;
} EcgRecordSample;*/
typedef struct {
    u64 sample_index;
    u32 timestamp_ms;

    
    u8 raw_adc;

    
    s16 filtered;
    s16 filtered_old;
    s16 qrs;
    s16 env;
    u8  sqi_state;
    u8  sqi_reason;
    u8  sqi_warning;

    
    u8 lod;
    u8 otr;
} EcgRecordSample;
typedef struct {
    u64 sample_index;
    u32 timestamp_ms;
    u32 rr_ms;
    u32 hr_bpm;
    u8  class_id;
    u8  confidence;
} EcgRecordEvent;

typedef struct {
    u8 initialized;
    u8 recording;

    u32 record_id;

    char sample_filename[64];
    char event_filename[64];
    char meta_filename[64];

    void *sample_file;
    void *event_file;
    void *meta_file;

    u32 sample_write_count;
    u32 event_write_count;
    u32 sample_segment_index;
    u32 sample_segment_write_count;
} EcgRecorder;

int ecg_recorder_init(EcgRecorder *rec);
int ecg_recorder_get_next_record_id(EcgRecorder *rec,
                                    u32 start_id,
                                    u32 *next_id);
int ecg_recorder_start(EcgRecorder *rec,
                       const DeviceContext *dev,
                       u32 fs_hz);
int ecg_recorder_write_sample(EcgRecorder *rec,
                              const EcgRecordSample *s);
int ecg_recorder_write_rpeak_event(EcgRecorder *rec,
                                   const EcgRecordEvent *e);
int ecg_recorder_write_cnn_event(EcgRecorder *rec,
                                 const EcgRecordEvent *e);
int ecg_recorder_write_lod_event(EcgRecorder *rec,
                                 u64 sample_index,
                                 u32 timestamp_ms,
                                 u8 lod);
int ecg_recorder_write_quality_event(EcgRecorder *rec,
                                     u64 sample_index,
                                     u32 timestamp_ms,
                                     u8 quality_good,
                                     u8 reason);
int ecg_recorder_stop(EcgRecorder *rec);
/* Detail is stored in class_id, only for the named diagnostic event types. */
int ecg_recorder_write_diagnostic(EcgRecorder *rec, const char *type,
                                  u64 index, u32 timestamp_ms, u8 detail);
u8 ecg_recorder_is_recording(const EcgRecorder *rec);

#endif
