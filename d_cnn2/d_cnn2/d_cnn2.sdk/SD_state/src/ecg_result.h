#ifndef ECG_RESULT_H
#define ECG_RESULT_H

#include "xil_types.h"

typedef enum {
    /* Class order is fixed by the deployed HLS model output. */
    ECG_CLASS_FUSION           = 0,
    ECG_CLASS_NORMAL           = 1,
    ECG_CLASS_SUPRAVENTRICULAR = 2,
    ECG_CLASS_VENTRICULAR      = 3,
    ECG_CLASS_COUNT            = 4,
    ECG_CLASS_UNKNOWN          = 255
} EcgClassId;

#define ECG_CLASS_INVALID             ((u8)ECG_CLASS_UNKNOWN)
#define ECG_CONFIDENCE_UNAVAILABLE    255U

typedef struct {
    u8 valid;
    u8 class_id;
    u8 confidence_percent;
} EcgInferResult;

const char* ecg_result_class_name(u8 class_id);
u8 ecg_result_class_is_valid(u8 class_id);

#endif /* ECG_RESULT_H */
