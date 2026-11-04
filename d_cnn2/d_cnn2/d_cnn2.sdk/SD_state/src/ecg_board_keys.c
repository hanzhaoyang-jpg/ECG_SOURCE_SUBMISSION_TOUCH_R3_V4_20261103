#include "ecg_standalone.h"
#include "xparameters.h"
#ifdef ECG_KEYS_HOST_TEST
#include "standalone_test_gpio.h"
#else
#include "xgpiops_hw.h"
#include "xil_io.h"
#endif

static u8 enabled;

int ecg_board_keys_init(u8 profile)
{
    u32 a, b, base = XPAR_XGPIOPS_0_BASEADDR;
    const u32 mask = (1U << 12) | (1U << 11);
    enabled = 0U;
    if (profile == 0U) return 0; /* No register access on an unknown board. */
    if (profile != 1U) return -1;
    /* Navigator guide: PS_KEY0=MIO12, PS_KEY1=MIO11, low when pressed.
     * Refuse to take pins currently muxed to another peripheral. */
    a = Xil_In32(0xF8000730U);
    b = Xil_In32(0xF800072CU);
    if ((a & 0xFEU) || (b & 0xFEU)) return -2;
    Xil_Out32(base + XGPIOPS_DIRM_OFFSET, Xil_In32(base + XGPIOPS_DIRM_OFFSET) & ~mask);
    Xil_Out32(base + XGPIOPS_OUTEN_OFFSET, Xil_In32(base + XGPIOPS_OUTEN_OFFSET) & ~mask);
    Xil_Out32(0xF8000008U, 0xDF0DU);
    Xil_Out32(0xF8000730U, (a & ~0x3FFFU) | 0x1601U);
    Xil_Out32(0xF800072CU, (b & ~0x3FFFU) | 0x1601U);
    Xil_Out32(0xF8000004U, 0x767BU);
    enabled = 1U;
    return 0;
}

u8 ecg_board_keys_read(void)
{
    u32 data;
    if (!enabled) return 0U;
    data = Xil_In32(XPAR_XGPIOPS_0_BASEADDR + XGPIOPS_DATA_RO_OFFSET);
    return (u8)(((data & (1U << 12)) ? 0U : 1U) |
                ((data & (1U << 11)) ? 0U : 2U));
}
