#include "ecg_result.h"

const char* ecg_result_class_name(u8 class_id)
{
    switch (class_id) {
    case ECG_CLASS_FUSION:
        return "F";
    case ECG_CLASS_NORMAL:
        return "N";
    case ECG_CLASS_SUPRAVENTRICULAR:
        return "S";
    case ECG_CLASS_VENTRICULAR:
        return "V";
    case ECG_CLASS_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

u8 ecg_result_class_is_valid(u8 class_id)
{
    return (class_id < (u8)ECG_CLASS_COUNT) ? 1U : 0U;
}
