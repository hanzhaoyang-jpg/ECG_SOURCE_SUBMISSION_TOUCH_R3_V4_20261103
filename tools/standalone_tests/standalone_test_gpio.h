#ifndef STANDALONE_TEST_GPIO_H
#define STANDALONE_TEST_GPIO_H
#include "xil_types.h"
#define XGPIOPS_DIRM_OFFSET 0x204U
#define XGPIOPS_OUTEN_OFFSET 0x208U
#define XGPIOPS_DATA_RO_OFFSET 0x60U
u32 mock_gpio_read(u32 address);
void mock_gpio_write(u32 address, u32 value);
#define Xil_In32 mock_gpio_read
#define Xil_Out32 mock_gpio_write
#endif
