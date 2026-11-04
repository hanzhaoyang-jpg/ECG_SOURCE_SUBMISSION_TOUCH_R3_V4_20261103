/* =========================================================
 * File: cn121_ps.c
 * ========================================================= */
#include "cn121_ps.h"

#include "xspips.h"
#include "xgpiops.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "sleep.h"
#include "xuartps_hw.h"

/*
 * Global handles.
 */
static XGpioPs Gpio;
static XSpiPs Spi;

static u8 g_cn121_initialized = 0U;
static u8 g_cn121_gpio_ready = 0U;
/*
 * UART base address for command processing.
 *
 * Usually one of:
 *   XPAR_XUARTPS_0_BASEADDR
 *   XPAR_XUARTPS_1_BASEADDR
 *
 * If your project already defines UART_BASEADDR, this will not override it.
 */
#ifndef UART_BASEADDR
#ifdef XPAR_XUARTPS_0_BASEADDR
#define UART_BASEADDR XPAR_XUARTPS_0_BASEADDR
#endif
#endif

static int cn121_gpio_init_only(void)
{
    XGpioPs_Config *GpioConfig;
    int Status;

    GpioConfig = XGpioPs_LookupConfig(GPIO_DEVICE_ID);
    if (GpioConfig == 0) {
        xil_printf("ERROR: XGpioPs_LookupConfig failed\r\n");
        g_cn121_gpio_ready = 0U;
        return XST_FAILURE;
    }

    Status = XGpioPs_CfgInitialize(&Gpio, GpioConfig, GpioConfig->BaseAddr);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: XGpioPs_CfgInitialize failed: %d\r\n", Status);
        g_cn121_gpio_ready = 0U;
        return Status;
    }
    g_cn121_gpio_ready = 1U;
    /*
     * RESET: output.
     * LOD  : input.
     */
    XGpioPs_SetDirectionPin(&Gpio, CN121_RESET_PIN, 1);
    XGpioPs_SetOutputEnablePin(&Gpio, CN121_RESET_PIN, 1);

    XGpioPs_SetDirectionPin(&Gpio, CN121_LOD_PIN, 0);

    /*
     * Default RESET deasserted.
     * RESET is active high.
     */
    XGpioPs_WritePin(&Gpio, CN121_RESET_PIN, 0);

    return XST_SUCCESS;
}

static int cn121_spi_init_only(void)
{
    XSpiPs_Config *SpiConfig;
    int Status;

    SpiConfig = XSpiPs_LookupConfig(SPI_DEVICE_ID);
    if (SpiConfig == 0) {
        xil_printf("ERROR: XSpiPs_LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    Status = XSpiPs_CfgInitialize(&Spi, SpiConfig, SpiConfig->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: XSpiPs_CfgInitialize failed: %d\r\n", Status);
        return Status;
    }

    /*
     * This follows your reference project:
     *
     *   XSPIPS_CLK_PHASE_1_OPTION
     *
     * CN121 latches DIN on SCLK falling edge and shifts DOUT on rising edge.
     * The reference project already verified this mode for similar hardware.
     */
    XSpiPs_SetClkPrescaler(&Spi, XSPIPS_CLK_PRESCALE_64);

    XSpiPs_SetOptions(&Spi,
                      XSPIPS_MASTER_OPTION |
                      XSPIPS_CLK_PHASE_1_OPTION |
                      XSPIPS_FORCE_SSELECT_OPTION);

    /*
     * Deassert all chip selects.
     *
     * In the reference code:
     *   0x00 selects CN121
     *   0x07 de-selects all
     */
    XSpiPs_SetSlaveSelect(&Spi, 0x07);

    return XST_SUCCESS;
}

int cn121_ps_init(void)
{
    int Status;

    Status = cn121_gpio_init_only();
    if (Status != XST_SUCCESS) {
        return Status;
    }

    Status = cn121_spi_init_only();
    if (Status != XST_SUCCESS) {
        return Status;
    }

    g_cn121_initialized = 1U;

    return XST_SUCCESS;
}

void cn121_reset_pulse(void)
{
    /*
     * RESET active high.
     *
     * Reference project:
     *   RESET = 1
     *   usleep(10000)
     *   RESET = 0
     *   usleep(50000)
     */
    XGpioPs_WritePin(&Gpio, CN121_RESET_PIN, 1);
    usleep(10000);

    XGpioPs_WritePin(&Gpio, CN121_RESET_PIN, 0);
    usleep(50000);
}

void cn121_write_reg(u8 addr, u8 data)
{
    u8 tx[3];
    int status;
    tx[0] = (u8)(CN121_CMD_WREG_BASE | (addr & 0x1FU));
    tx[1] = 0x00U;
    tx[2] = data;

    XSpiPs_SetSlaveSelect(&Spi, 0x00);
    status = XSpiPs_PolledTransfer(&Spi, tx, 0, 3);
    XSpiPs_SetSlaveSelect(&Spi, 0x07);
    if (status != XST_SUCCESS) {
        xil_printf("[CN121] cn121_write_reg failed: addr=0x%02x data=0x%02x status=%d\r\n",
                   addr, data, status);
    }
}

u8 cn121_read_reg(u8 addr)
{
    u8 tx[3];
    u8 rx[3];
    int status;

    tx[0] = (u8)(CN121_CMD_RREG_BASE | (addr & 0x1FU));
    tx[1] = 0x00U;
    tx[2] = 0x00U;

    rx[0] = 0U;
    rx[1] = 0U;
    rx[2] = 0U;

    XSpiPs_SetSlaveSelect(&Spi, 0x00);

    status = XSpiPs_PolledTransfer(&Spi, tx, rx, 3);

    XSpiPs_SetSlaveSelect(&Spi, 0x07);

    if (status != XST_SUCCESS) {
        xil_printf("[CN121] cn121_read_reg failed: addr=0x%02x status=%d\r\n",
                   addr,
                   status);
        return 0xFFU;
    }

    return rx[2];
}

void cn121_write_regs(u8 start_addr, const u8 *data, u8 count)
{
    u8 tx[34];
    u8 i;

    if (data == 0 || count == 0U) {
        return;
    }

    /*
     * Enough for CN121 small register map.
     */
    if (count > 32U) {
        count = 32U;
    }

    tx[0] = (u8)(CN121_CMD_WREG_BASE | (start_addr & 0x1FU));
    tx[1] = (u8)(count - 1U);

    for (i = 0U; i < count; i++) {
        tx[2U + i] = data[i];
    }

    XSpiPs_SetSlaveSelect(&Spi, 0x00);
    XSpiPs_PolledTransfer(&Spi, tx, 0, (u32)(2U + count));
    XSpiPs_SetSlaveSelect(&Spi, 0x07);
}

void cn121_read_regs(u8 start_addr, u8 *data, u8 count)
{
    u8 tx[34];
    u8 rx[34];
    u8 i;

    if (data == 0 || count == 0U) {
        return;
    }

    if (count > 32U) {
        count = 32U;
    }

    tx[0] = (u8)(CN121_CMD_RREG_BASE | (start_addr & 0x1FU));
    tx[1] = (u8)(count - 1U);

    for (i = 0U; i < count; i++) {
        tx[2U + i] = 0x00U;
    }

    for (i = 0U; i < 34U; i++) {
        rx[i] = 0U;
    }

    XSpiPs_SetSlaveSelect(&Spi, 0x00);
    XSpiPs_PolledTransfer(&Spi, tx, rx, (u32)(2U + count));
    XSpiPs_SetSlaveSelect(&Spi, 0x07);

    for (i = 0U; i < count; i++) {
        data[i] = rx[2U + i];
    }
}

u8 cn121_get_lod(void)
{
    if (!g_cn121_gpio_ready) {
        return 1U;
    }

    return (u8)(XGpioPs_ReadPin(&Gpio, CN121_LOD_PIN) ? 1U : 0U);
}

int cn121_setup_default(Cn121Status *status)
{
    u8 cfg_after;
    u8 chset_after;


    if (cn121_ps_init() != XST_SUCCESS) {
        xil_printf("ERROR: cn121_ps_init failed\r\n");
        return XST_FAILURE;
    }

    cn121_reset_pulse();

    /*
     * CONFIG = 0x00:
     *   sensing mode
     */
    cn121_write_reg(REG_CONFIG, CN121_CONFIG_SENSING);

    /*
     * CHSET = 0x0C:
     *   HP = 4 Hz
     *   LP = 40 Hz
     *   Gain = 780
     */
    cn121_write_reg(REG_CHSET, CN121_CHSET_ECG_DEFAULT);

    usleep(1000);

    cfg_after = cn121_read_reg(REG_CONFIG);
    chset_after = cn121_read_reg(REG_CHSET);

    cn121_print_chset_decode(chset_after);

    if (status != 0) {
        status->config = cfg_after;
        status->chset = chset_after;
        status->lod = cn121_get_lod();
    }

    /*
     * Basic readback check.
     */
    if (cfg_after != CN121_CONFIG_SENSING) {
        xil_printf("WARNING: CN121 CONFIG readback mismatch, expected 0x00\r\n");
        return XST_FAILURE;
    }

    if (chset_after != CN121_CHSET_ECG_DEFAULT) {
        xil_printf("WARNING: CN121 CHSET readback mismatch, expected 0x%02x\r\n",
                   CN121_CHSET_ECG_DEFAULT);
        return XST_FAILURE;
    }

    xil_printf("CN121 Initialized. (Gain=780, HP=4Hz, LP=40Hz; readback verified)\r\n");

    return 0;
}

u8 cn121_get_chset(void)
{
    return cn121_read_reg(REG_CHSET);
}

void cn121_set_chset(u8 chset)
{
    cn121_write_reg(REG_CHSET, chset);
    usleep(1000);
}

u16 cn121_gain_code_to_value(u8 gain_code)
{
    gain_code &= 0x0FU;

    /*
     * CN121 gain table:
     * 0 -> 60
     * 1 -> 120
     * ...
     * 15 -> 960
     */
    return (u16)(60U * ((u16)gain_code + 1U));
}

void cn121_set_gain_code(u8 gain_code)
{
    u8 chset;
    u8 new_chset;

    gain_code &= 0x0FU;

    chset = cn121_read_reg(REG_CHSET);
    new_chset = (u8)((chset & 0xF0U) | gain_code);

    cn121_write_reg(REG_CHSET, new_chset);
    usleep(1000);

    xil_printf("CN121 gain changed: code=%d gain=%d CHSET=0x%02x\r\n",
               gain_code,
               cn121_gain_code_to_value(gain_code),
               cn121_read_reg(REG_CHSET));
}

void cn121_cycle_gain(void)
{
    u8 chset;
    u8 gain;
    u8 new_chset;

    chset = cn121_read_reg(REG_CHSET);
    gain = (u8)(chset & 0x0FU);
    gain = (u8)((gain + 1U) & 0x0FU);

    new_chset = (u8)((chset & 0xF0U) | gain);

    cn121_write_reg(REG_CHSET, new_chset);
    usleep(1000);

    xil_printf("-> New Gain Level: code=%d gain=%d CHSET=0x%02x\r\n",
               gain,
               cn121_gain_code_to_value(gain),
               cn121_read_reg(REG_CHSET));
}

void cn121_toggle_highpass(void)
{
    u8 chset;
    u8 new_chset;

    chset = cn121_read_reg(REG_CHSET);
    new_chset = (u8)(chset ^ 0x40U);

    cn121_write_reg(REG_CHSET, new_chset);
    usleep(1000);

    xil_printf("-> High Pass Changed. Bit6=%d ",
               (new_chset >> 6) & 1U);
    cn121_print_chset_decode(cn121_read_reg(REG_CHSET));
}

void cn121_cycle_lowpass(void)
{
    u8 chset;
    u8 lp;
    u8 new_chset;

    chset = cn121_read_reg(REG_CHSET);
    lp = (u8)((chset >> 4) & 0x03U);
    lp = (u8)((lp + 1U) & 0x03U);

    new_chset = (u8)((chset & 0xCFU) | (lp << 4));

    cn121_write_reg(REG_CHSET, new_chset);
    usleep(1000);

    xil_printf("-> Low Pass Changed. lp_code=%d ", lp);
    cn121_print_chset_decode(cn121_read_reg(REG_CHSET));
}

void cn121_print_chset_decode(u8 chset)
{
    u8 fr;
    u8 hp;
    u8 lp;
    u8 gain_code;
    u16 gain_value;
    const char *hp_str;
    const char *lp_str;

    fr = (u8)((chset >> 7) & 0x01U);
    hp = (u8)((chset >> 6) & 0x01U);
    lp = (u8)((chset >> 4) & 0x03U);
    gain_code = (u8)(chset & 0x0FU);
    gain_value = cn121_gain_code_to_value(gain_code);

    hp_str = hp ? "0.05Hz" : "4Hz";

    switch (lp) {
    case 0:
        lp_str = "40Hz";
        break;
    case 1:
        lp_str = "55Hz";
        break;
    case 2:
        lp_str = "150Hz";
        break;
    default:
        lp_str = "300Hz";
        break;
    }

    xil_printf("decode: FR=%d HP=%s LP=%s GAIN=%d(code=%d)\r\n",
               fr,
               hp_str,
               lp_str,
               gain_value,
               gain_code);
}

void cn121_show_menu(void)
{
    xil_printf("\r\n=== CN121 Control Menu ===\r\n");
    xil_printf(" [s] Show Status\r\n");
    xil_printf(" [g] Cycle Gain (+60)\r\n");
    xil_printf(" [-] Decrease Gain (-60, stops at 60)\r\n");
    xil_printf(" [h] Toggle High Pass (0.05Hz / 4Hz)\r\n");
    xil_printf(" [l] Cycle Low Pass (40/55/150/300Hz)\r\n");
    xil_printf(" [0] Set default CHSET=0x0C, HP=4Hz, LP=40Hz, Gain=780\r\n");
    xil_printf(" [m] Show this menu\r\n");
    xil_printf("==========================\r\n");
}

int cn121_handle_uart_cmd(u8 cmd)
{
    u8 current_val;

    switch (cmd) {
    case 'm':
    case 'M':
        cn121_show_menu();
        return 1;

    case 's':
    case 'S':
        current_val = cn121_read_reg(REG_CHSET);
        xil_printf("-> Status: CONFIG=0x%02x CHSET=0x%02x LOD=%d ",
                   cn121_read_reg(REG_CONFIG),
                   current_val,
                   cn121_get_lod());
        cn121_print_chset_decode(current_val);
        return 1;

    case 'g':
    case 'G':
        cn121_cycle_gain();
        return 1;

    case '-':
        current_val = (u8)(cn121_read_reg(REG_CHSET) & 0x0FU);
        cn121_set_gain_code(current_val ? (u8)(current_val - 1U) : 0U);
        return 1;

    case 'h':
    case 'H':
        cn121_toggle_highpass();
        return 1;

    case 'l':
    case 'L':
        cn121_cycle_lowpass();
        return 1;

    case '0':
        cn121_write_reg(REG_CONFIG, CN121_CONFIG_SENSING);
        cn121_write_reg(REG_CHSET, CN121_CHSET_ECG_DEFAULT);
        usleep(1000);
        xil_printf("-> CN121 default restored: CONFIG=0x%02x CHSET=0x%02x ",
                   cn121_read_reg(REG_CONFIG),
                   cn121_read_reg(REG_CHSET));
        cn121_print_chset_decode(cn121_read_reg(REG_CHSET));
        return 1;
    default:
        return 0;
    }
}

void cn121_process_uart_cmd(void)
{
#ifdef UART_BASEADDR
    u32 status;
    u8 cmd;

    status = Xil_In32(UART_BASEADDR + XUARTPS_SR_OFFSET);

    if ((status & XUARTPS_SR_RXEMPTY) != 0U) {
        return;
    }

    cmd = (u8)Xil_In32(UART_BASEADDR + XUARTPS_FIFO_OFFSET);
    (void)cn121_handle_uart_cmd(cmd);
#else
    /*
     * UART_BASEADDR is not known.
     * Do nothing.
     */

#endif
}
u32 cn121_gpio_get_baseaddr_debug(void)
{
#if defined(__GNUC__)
    
#endif

    return (u32)Gpio.GpioConfig.BaseAddr;
}

u32 cn121_gpio_get_deviceid_debug(void)
{
    return (u32)Gpio.GpioConfig.DeviceId;
}

u8 cn121_gpio_is_ready_debug(void)
{
    return g_cn121_gpio_ready;
}

void cn121_gpio_dump_debug(const char *tag)
{
    xil_printf("[CN121 GPIO DBG] %s ready=%d base=0x%08lx dev=%lu lod_pin=%lu lod=%d\r\n",
               tag,
               (int)g_cn121_gpio_ready,
               (u32)Gpio.GpioConfig.BaseAddr,
               (u32)Gpio.GpioConfig.DeviceId,
               (u32)CN121_LOD_PIN,
               (int)cn121_get_lod());
}

