#include "ecg_touch.h"
#include "lcd_port.h"
#include "xparameters.h"
#include "xgpiops_hw.h"
#include "xil_io.h"
#include "sleep.h"

/* GPIO bank 2 bit 0 is EMIO54. Preserve the CN121 bits 0 and 1. */
#define GPIO_BASE XPAR_XGPIOPS_0_BASEADDR
#define GPIO_DIR (GPIO_BASE + XGPIOPS_DIRM_OFFSET + 2U * XGPIOPS_REG_MASK_OFFSET)
#define GPIO_OE (GPIO_BASE + XGPIOPS_OUTEN_OFFSET + 2U * XGPIOPS_REG_MASK_OFFSET)
#define GPIO_IN (GPIO_BASE + XGPIOPS_DATA_RO_OFFSET + 8U)
#define GPIO_MASK_WRITE (GPIO_BASE + XGPIOPS_DATA_LSW_OFFSET + 16U)
#define SCL (1U << 2)
#define SDA (1U << 3)
#define RESET (1U << 4)
#define INT_PIN (1U << 5)

static u32 last_poll;
static int stage, origin_x, origin_y, swapped, flip_x, flip_y;
static int last_x, last_y, pressed_button, frames, blocked = 1, online;
static const char *bus_fault = "TOUCH: WAITING FOR I2C";

/* Open drain: output latch stays zero; release by switching to input. */
static void drive_low(u32 pin, int low)
{
    u32 dir = Xil_In32(GPIO_DIR);
    Xil_Out32(GPIO_DIR, low ? dir | pin : dir & ~pin);
    if (low) Xil_Out32(GPIO_MASK_WRITE, ((~pin & 0xFFFFU) << 16));
}

static int clock_high(void)
{
    unsigned tries;
    drive_low(SCL, 0);
    for (tries = 0; tries < 10U; ++tries) {
        usleep(5U);
        if (Xil_In32(GPIO_IN) & SCL) return 1;
    }
    bus_fault = "TOUCH: SCL LOW";
    return 0;
}

static int start(void)
{
    drive_low(SDA, 0);
    usleep(5U);
    if (!clock_high()) return 0;
    if (!(Xil_In32(GPIO_IN) & SDA)) {
        bus_fault = "TOUCH: SDA LOW";
        return 0;
    }
    drive_low(SDA, 1);
    usleep(5U);
    if (Xil_In32(GPIO_IN) & SDA) {
        bus_fault = "TOUCH: SDA DRIVE FAIL";
        return 0;
    }
    drive_low(SCL, 1);
    if (Xil_In32(GPIO_IN) & SCL) {
        bus_fault = "TOUCH: SCL DRIVE FAIL";
        return 0;
    }
    return 1;
}

static void stop(void)
{
    drive_low(SCL, 1);
    drive_low(SDA, 1);
    usleep(5U);
    (void)clock_high();
    drive_low(SDA, 0);
    usleep(5U);
}

static int bit(int send_low, int *read)
{
    drive_low(SDA, send_low);
    usleep(5U);
    if (!clock_high()) return 0;
    *read = (Xil_In32(GPIO_IN) & SDA) != 0U;
    drive_low(SCL, 1);
    return 1;
}

static int send_byte(u8 value)
{
    int i, read;
    for (i = 7; i >= 0; --i)
        if (!bit(!(value & (1U << i)), &read)) return 0;
    return bit(0, &read) && !read;
}

static int read_frame(u8 *buf)
{
    int i, j, read, ok = 0;
    /* FT5206/FT5426: 7-bit address 0x38, status + first XY. */
    if (!start()) goto done;
    bus_fault = "TOUCH: NO ACK AT 0X38";
    if (!send_byte(0x70U)) goto done;
    bus_fault = "TOUCH: REGISTER NACK";
    if (!send_byte(0x02U) || !start()) goto done;
    bus_fault = "TOUCH: READ NACK";
    if (!send_byte(0x71U)) goto done;
    for (i = 0; i < 5; ++i) {
        buf[i] = 0U;
        for (j = 0; j < 8; ++j) {
            if (!bit(0, &read)) goto done;
            buf[i] = (u8)((buf[i] << 1) | read);
        }
        if (!bit(i < 4, &read)) goto done;
    }
    ok = 1;
done:
    {
        const char *failure = bus_fault;
        stop();
        if (!ok) bus_fault = failure;
        else if (!(Xil_In32(GPIO_IN) & SCL)) {
            bus_fault = "TOUCH: SCL LOW";
            ok = 0;
        } else if (!(Xil_In32(GPIO_IN) & SDA)) {
            bus_fault = "TOUCH: SDA LOW";
            ok = 0;
        }
    }
    return ok;
}

void ecg_touch_init(void)
{
    const u32 out = SCL | SDA | RESET;
    Xil_Out32(GPIO_DIR, Xil_In32(GPIO_DIR) & ~(out | INT_PIN));
    Xil_Out32(GPIO_OE, (Xil_In32(GPIO_OE) & ~INT_PIN) | out);
    /* This GPIO bank ignores output-data writes while the pin is input. */
    Xil_Out32(GPIO_DIR, Xil_In32(GPIO_DIR) | out);
    Xil_Out32(GPIO_MASK_WRITE, ((~out & 0xFFFFU) << 16));
    drive_low(SCL, 0);
    drive_low(SDA, 0);
    usleep(12000U);
    Xil_Out32(GPIO_MASK_WRITE, ((~RESET & 0xFFFFU) << 16) | RESET);
    usleep(300000U);
}

static int magnitude(int x) { return x < 0 ? -x : x; }

static int button_at(int x, int y)
{
    int button, cell = LCD_WIDTH / TOUCH_BUTTON_COUNT;
    if (x < 0 || x >= cell * TOUCH_BUTTON_COUNT ||
        y < LCD_HEIGHT - TOUCH_FOOTER_HEIGHT + 4 || y >= LCD_HEIGHT - 4)
        return TOUCH_NONE;
    button = x / cell;
    return x % cell >= 4 && x % cell < cell - 4 ? button : TOUCH_NONE;
}

static int contact(int count, int x, int y)
{
    int action = TOUCH_NONE;
    if (count == 0) {
        if (!blocked && frames >= 2) {
            if (stage == 0) {
                origin_x = last_x; origin_y = last_y; stage = 1;
            } else if (stage == 1) {
                int dx = last_x - origin_x, dy = last_y - origin_y;
                swapped = magnitude(dy) > magnitude(dx);
                if (magnitude(swapped ? dy : dx) > LCD_WIDTH / 2 &&
                    magnitude(swapped ? dx : dy) < LCD_HEIGHT / 4) {
                    flip_x = (swapped ? dy : dx) < 0;
                    stage = 2;
                } else stage = 0;
            } else if (stage == 2) {
                int dx = last_x - origin_x, dy = last_y - origin_y;
                if (magnitude(swapped ? dx : dy) > LCD_HEIGHT / 2 &&
                    magnitude(swapped ? dy : dx) < LCD_WIDTH / 4) {
                    flip_y = (swapped ? dx : dy) < 0;
                    stage = 3;
                } else stage = 0;
            } else action = pressed_button;
        }
        blocked = frames = 0;
        return action;
    }
    if (count != 1 || x >= LCD_WIDTH || y >= LCD_WIDTH) blocked = 1;
    if (blocked) return TOUCH_NONE;
    if (stage == 3) {
        int xx = swapped ? y : x, yy = swapped ? x : y;
        if (xx >= LCD_WIDTH || yy >= LCD_HEIGHT) {
            blocked = 1;
            return TOUCH_NONE;
        }
        if (flip_x) xx = LCD_WIDTH - 1 - xx;
        if (flip_y) yy = LCD_HEIGHT - 1 - yy;
        action = button_at(xx, yy);
    }
    if (!frames) {
        last_x = x; last_y = y; pressed_button = action;
    } else if (action != pressed_button || magnitude(x - last_x) > 40 ||
               magnitude(y - last_y) > 40) {
        blocked = 1;
    }
    if (frames < 2) ++frames;
    return TOUCH_NONE;
}

int ecg_touch_poll(u32 now_ms)
{
    u8 b[5];
    if ((u32)(now_ms - last_poll) < (online ? 25U : 250U)) return TOUCH_NONE;
    last_poll = now_ms;
    online = read_frame(b);
    if (!online) return contact(-1, 0, 0);
    if ((b[0] & 15U) == 1U && (b[1] >> 6) == 1U)
        return TOUCH_NONE; /* Wait for a confirmed zero-contact frame. */
    return contact(b[0] & 15U, ((b[1] & 15U) << 8) | b[2],
                   ((b[3] & 15U) << 8) | b[4]);
}

const char *ecg_touch_hint(void)
{
    static const char *hints[] = {"TAP TOP LEFT", "TAP TOP RIGHT", "TAP BOTTOM LEFT"};
    if (!online) return bus_fault;
    return stage < 3 ? hints[stage] : 0;
}
