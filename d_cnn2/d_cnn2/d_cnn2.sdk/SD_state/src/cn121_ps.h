/* =========================================================
 * File: cn121_ps.h
 *
 * CN121 driver using:
 *   - PS SPI  : XSpiPs
 *   - PS GPIO : XGpioPs for RESET and LOD
 *
 * This version is adapted from the reference project style:
 *
 *   XSpiPs_SetClkPrescaler(&Spi, XSPIPS_CLK_PRESCALE_64);
 *   XSpiPs_SetOptions(&Spi,
 *       XSPIPS_MASTER_OPTION |
 *       XSPIPS_CLK_PHASE_1_OPTION |
 *       XSPIPS_FORCE_SSELECT_OPTION);
 *   XSpiPs_SetSlaveSelect(&Spi, 0x07);
 *
 *   Write: { 0x40 | addr, 0x00, data }
 *   Read : { 0x20 | addr, 0x00, 0x00 } -> rx[2]
 * ========================================================= */
#ifndef CN121_PS_H
#define CN121_PS_H

#include "xil_types.h"
#include "xparameters.h"

/*
 * =========================================================
 * User configuration section
 * =========================================================
 *
 * Please modify these macros according to your xparameters.h
 * and your PS MIO pin assignment.
 *
 * In the reference project, these macros are defined elsewhere:
 *
 *   SPI_DEVICE_ID
 *   GPIO_DEVICE_ID
 *   CN121_RESET_PIN
 *   CN121_LOD_PIN
 *   REG_CONFIG
 *   REG_CHSET
 *
 * To make this driver self-contained, default aliases are provided.
 */

/*
 * SPI device ID.
 *
 * Common possible values:
 *   XPAR_XSPIPS_0_DEVICE_ID
 *   XPAR_XSPIPS_1_DEVICE_ID
 *
 * If your project already defines SPI_DEVICE_ID, this driver will use it.
 */
#ifndef SPI_DEVICE_ID
#define SPI_DEVICE_ID        XPAR_XSPIPS_0_DEVICE_ID
#endif

/*
 * PS GPIO device ID.
 *
 * Usually:
 *   XPAR_XGPIOPS_0_DEVICE_ID
 */
#ifndef GPIO_DEVICE_ID
#define GPIO_DEVICE_ID       XPAR_XGPIOPS_0_DEVICE_ID
#endif

/*
 * CN121 RESET and LOD PS GPIO pin numbers.
 *
 * IMPORTANT:
 * Replace these two values with your actual MIO/EMIO pin numbers.
 *
 * Examples:
 *   If RESET is connected to MIO 54, use 54.
 *   If LOD   is connected to MIO 55, use 55.
 *
 * If your old project has correct definitions, define them in a common
 * header or directly replace the values here.
 */
#ifndef CN121_RESET_PIN
#define CN121_RESET_PIN      54
#endif

#ifndef CN121_LOD_PIN
#define CN121_LOD_PIN        55
#endif

/*
 * CN121 register addresses.
 */
#ifndef REG_CONFIG
#define REG_CONFIG           0x00U
#endif

#ifndef REG_CHSET
#define REG_CHSET            0x01U
#endif

/*
 * CN121 command bases.
 */
#define CN121_CMD_RREG_BASE  0x20U
#define CN121_CMD_WREG_BASE  0x40U

/*
 * CONFIG register bits.
 */
#define CN121_CONFIG_PD          (1U << 6)
#define CN121_CONFIG_STANDBY     (1U << 5)

/*
 * CHSET register bits.
 *
 * bit7     CHFR
 * bit6     CHHP
 * bit5:4   CHLP[1:0]
 * bit3:0   CHGAIN[3:0]
 */
#define CN121_CHSET_CHFR         (1U << 7)
#define CN121_CHSET_HP_4HZ       (0U << 6)
#define CN121_CHSET_HP_0P05HZ    (1U << 6)

#define CN121_CHSET_LP_40HZ      (0U << 4)
#define CN121_CHSET_LP_55HZ      (1U << 4)
#define CN121_CHSET_LP_150HZ     (2U << 4)
#define CN121_CHSET_LP_300HZ     (3U << 4)

#define CN121_CHSET_GAIN_60      0x00U
#define CN121_CHSET_GAIN_120     0x01U
#define CN121_CHSET_GAIN_180     0x02U
#define CN121_CHSET_GAIN_240     0x03U
#define CN121_CHSET_GAIN_300     0x04U
#define CN121_CHSET_GAIN_360     0x05U
#define CN121_CHSET_GAIN_420     0x06U
#define CN121_CHSET_GAIN_480     0x07U
#define CN121_CHSET_GAIN_540     0x08U
#define CN121_CHSET_GAIN_600     0x09U
#define CN121_CHSET_GAIN_660     0x0AU
#define CN121_CHSET_GAIN_720     0x0BU
#define CN121_CHSET_GAIN_780     0x0CU
#define CN121_CHSET_GAIN_840     0x0DU
#define CN121_CHSET_GAIN_900     0x0EU
#define CN121_CHSET_GAIN_960     0x0FU

/*
 * User-selected standalone startup configuration:
 *
 * CONFIG = 0x00:
 *   sensing mode
 *
 * CHSET = 0x0C:
 *   bit7   = 0      fast restore disabled
 *   bit6   = 0      HP = 4 Hz
 *   bit5:4 = 00     LP = 40 Hz
 *   bit3:0 = 1100   gain = 780
 */
#define CN121_CONFIG_SENSING     0x00U
#define CN121_CHSET_ECG_DEFAULT  \
    (CN121_CHSET_HP_4HZ | CN121_CHSET_LP_40HZ | CN121_CHSET_GAIN_780)

typedef struct {
    u8 config;
    u8 chset;
    u8 lod;
} Cn121Status;

/*
 * Basic init and register access.
 */
int  cn121_ps_init(void);
int  cn121_setup_default(Cn121Status *status);

void cn121_reset_pulse(void);

void cn121_write_reg(u8 addr, u8 data);
u8   cn121_read_reg(u8 addr);

void cn121_write_regs(u8 start_addr, const u8 *data, u8 count);
void cn121_read_regs(u8 start_addr, u8 *data, u8 count);

u8   cn121_get_lod(void);
void cn121_dump_registers(void);

/*
 * Runtime control helpers.
 */
u8   cn121_get_chset(void);
void cn121_set_chset(u8 chset);

void cn121_set_gain_code(u8 gain_code);
void cn121_cycle_gain(void);
void cn121_toggle_highpass(void);
void cn121_cycle_lowpass(void);

u16  cn121_gain_code_to_value(u8 gain_code);
void cn121_print_chset_decode(u8 chset);
void cn121_show_menu(void);
int  cn121_handle_uart_cmd(u8 cmd);



u32 cn121_gpio_get_baseaddr_debug(void);
u32 cn121_gpio_get_deviceid_debug(void);
u8  cn121_gpio_is_ready_debug(void);
void cn121_gpio_dump_debug(const char *tag);
/*
 * UART command processing.
 *
 * Uses PS UART register read, similar to your reference project.
 * If your UART base address differs, define UART_BASEADDR before including.
 */
void cn121_process_uart_cmd(void);

#endif /* CN121_PS_H */
