#include <assert.h>
#include <stdio.h>
#include "xparameters.h"
#include "standalone_test_gpio.h"
#include "ecg_standalone.h"
static u32 reg[5], writes, reads;
static int index_of(u32 a)
{
    if (a == 0xF8000730U) return 0;
    if (a == 0xF800072CU) return 1;
    if (a == XPAR_XGPIOPS_0_BASEADDR + XGPIOPS_DIRM_OFFSET) return 2;
    if (a == XPAR_XGPIOPS_0_BASEADDR + XGPIOPS_OUTEN_OFFSET) return 3;
    if (a == XPAR_XGPIOPS_0_BASEADDR + XGPIOPS_DATA_RO_OFFSET) return 4;
    return -1;
}
u32 mock_gpio_read(u32 a) { int i = index_of(a); assert(i >= 0); ++reads; return reg[i]; }
void mock_gpio_write(u32 a, u32 value)
{
    int i = index_of(a);
    ++writes;
    if (i >= 0) reg[i] = value;
    else assert((a == 0xF8000008U && value == 0xDF0DU) ||
                (a == 0xF8000004U && value == 0x767BU));
}
int main(void)
{
    assert(ecg_board_keys_init(0) == 0 && !reads && !writes);
    assert(ecg_board_keys_read() == 0 && !reads);
    reg[0] = 2;
    assert(ecg_board_keys_init(1) != 0 && !writes);
    reg[0] = reg[1] = 0;
    reg[2] = reg[3] = 0xFFFFFFFFU;
    assert(ecg_board_keys_init(1) == 0);
    assert(reg[0] == 0x1601U && reg[1] == 0x1601U);
    assert(reg[2] == (0xFFFFFFFFU & ~((1U << 12) | (1U << 11))));
    assert(reg[3] == reg[2]);
    reg[4] = (1U << 12) | (1U << 11);
    assert(ecg_board_keys_read() == 0);
    reg[4] = 1U << 11;
    assert(ecg_board_keys_read() == 1);
    reg[4] = 1U << 12;
    assert(ecg_board_keys_read() == 2);
    puts("PASS keys: disabled=no IO, mux conflict=no writes, only approved input bits changed, active-low mapping");
    return 0;
}
