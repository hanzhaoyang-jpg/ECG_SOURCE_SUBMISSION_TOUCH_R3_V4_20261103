#ifndef ECG_TOUCH_H
#define ECG_TOUCH_H

#include "xil_types.h"

enum { TOUCH_NONE = -1, TOUCH_RECORD, TOUCH_GAIN_DOWN, TOUCH_GAIN_UP,
       TOUCH_HP, TOUCH_LP, TOUCH_SAVE, TOUCH_BUTTON_COUNT };

#define TOUCH_FOOTER_HEIGHT 44

void ecg_touch_init(void);
int ecg_touch_poll(u32 now_ms);
const char *ecg_touch_hint(void); /* Null when the touch buttons are ready. */

#endif
