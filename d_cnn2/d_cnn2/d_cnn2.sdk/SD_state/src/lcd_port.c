/* =========================================================
 * File: lcd_port.c
 * LCD port implementation for AXI VDMA MM2S RGB888 framebuffer
 * ========================================================= */
#include "lcd_port.h"

#include "xil_printf.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xstatus.h"
#include "sleep.h"

#include "display_ctrl/display_ctrl.h"
#include "vdma_api/vdma_api.h"

/*
 * Your xparameters.h has:
 *   #define XPAR_AXI_DYNCLK_0_BASEADDR 0x43C20000
 */
#ifndef LCD_DYNCLK_BASEADDR
#ifdef XPAR_AXI_DYNCLK_0_BASEADDR
#define LCD_DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#else
#define LCD_DYNCLK_BASEADDR 0x43C20000U
#endif
#endif

/*
 * Set to 1 for power-on color test:
 *   RED -> GREEN -> BLUE -> BLACK
 *
 * After LCD is verified, you can change this to 0.
 */
#ifndef LCD_PORT_COLOR_TEST_ENABLE
#define LCD_PORT_COLOR_TEST_ENABLE 1
#endif

/*
 * 5x7 font scale.
 * Scale 1 = 5x7 pixels + spacing.
 * Scale 2 = 10x14 pixels + spacing.
 *
 * For 480x272 status bar, scale 1 is safer.
 */

/*
 * If color appears red/blue swapped, change this to 1.
 */
#ifndef LCD_PORT_BGR_ORDER
#define LCD_PORT_BGR_ORDER 0
#endif

static DisplayCtrl g_disp;
static XAxiVdma g_vdma;

static u8 *g_fb = (u8 *)LCD_FRAMEBUFFER_BASEADDR;

/* =========================================================
 * RGB565 -> RGB888 helpers
 * ========================================================= */
static u8 rgb565_to_r8(u16 c)
{
    u8 r5 = (u8)((c >> 11) & 0x1FU);
    return (u8)((r5 << 3) | (r5 >> 2));
}

static u8 rgb565_to_g8(u16 c)
{
    u8 g6 = (u8)((c >> 5) & 0x3FU);
    return (u8)((g6 << 2) | (g6 >> 4));
}

static u8 rgb565_to_b8(u16 c)
{
    u8 b5 = (u8)(c & 0x1FU);
    return (u8)((b5 << 3) | (b5 >> 2));
}

/* =========================================================
 * Minimal 5x7 font
 *
 * Glyph format:
 *   5 columns, each column uses lower 7 bits as vertical pixels.
 *   bit0 = top pixel, bit6 = bottom pixel.
 *
 * Lowercase letters are converted to uppercase.
 * Unsupported characters become space.
 * ========================================================= */
static void lcd_font_get_5x7(char ch, u8 col[5])
{
    int i;

    for (i = 0; i < 5; i++) {
        col[i] = 0x00U;
    }

    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }

    switch (ch) {
    case '0': col[0]=0x3EU; col[1]=0x51U; col[2]=0x49U; col[3]=0x45U; col[4]=0x3EU; break;
    case '1': col[0]=0x00U; col[1]=0x42U; col[2]=0x7FU; col[3]=0x40U; col[4]=0x00U; break;
    case '2': col[0]=0x42U; col[1]=0x61U; col[2]=0x51U; col[3]=0x49U; col[4]=0x46U; break;
    case '3': col[0]=0x21U; col[1]=0x41U; col[2]=0x45U; col[3]=0x4BU; col[4]=0x31U; break;
    case '4': col[0]=0x18U; col[1]=0x14U; col[2]=0x12U; col[3]=0x7FU; col[4]=0x10U; break;
    case '5': col[0]=0x27U; col[1]=0x45U; col[2]=0x45U; col[3]=0x45U; col[4]=0x39U; break;
    case '6': col[0]=0x3CU; col[1]=0x4AU; col[2]=0x49U; col[3]=0x49U; col[4]=0x30U; break;
    case '7': col[0]=0x01U; col[1]=0x71U; col[2]=0x09U; col[3]=0x05U; col[4]=0x03U; break;
    case '8': col[0]=0x36U; col[1]=0x49U; col[2]=0x49U; col[3]=0x49U; col[4]=0x36U; break;
    case '9': col[0]=0x06U; col[1]=0x49U; col[2]=0x49U; col[3]=0x29U; col[4]=0x1EU; break;

    case 'A': col[0]=0x7EU; col[1]=0x11U; col[2]=0x11U; col[3]=0x11U; col[4]=0x7EU; break;
    case 'B': col[0]=0x7FU; col[1]=0x49U; col[2]=0x49U; col[3]=0x49U; col[4]=0x36U; break;
    case 'C': col[0]=0x3EU; col[1]=0x41U; col[2]=0x41U; col[3]=0x41U; col[4]=0x22U; break;
    case 'D': col[0]=0x7FU; col[1]=0x41U; col[2]=0x41U; col[3]=0x22U; col[4]=0x1CU; break;
    case 'E': col[0]=0x7FU; col[1]=0x49U; col[2]=0x49U; col[3]=0x49U; col[4]=0x41U; break;
    case 'F': col[0]=0x7FU; col[1]=0x09U; col[2]=0x09U; col[3]=0x09U; col[4]=0x01U; break;
    case 'G': col[0]=0x3EU; col[1]=0x41U; col[2]=0x49U; col[3]=0x49U; col[4]=0x7AU; break;
    case 'H': col[0]=0x7FU; col[1]=0x08U; col[2]=0x08U; col[3]=0x08U; col[4]=0x7FU; break;
    case 'I': col[0]=0x00U; col[1]=0x41U; col[2]=0x7FU; col[3]=0x41U; col[4]=0x00U; break;
    case 'J': col[0]=0x20U; col[1]=0x40U; col[2]=0x41U; col[3]=0x3FU; col[4]=0x01U; break;
    case 'K': col[0]=0x7FU; col[1]=0x08U; col[2]=0x14U; col[3]=0x22U; col[4]=0x41U; break;
    case 'L': col[0]=0x7FU; col[1]=0x40U; col[2]=0x40U; col[3]=0x40U; col[4]=0x40U; break;
    case 'M': col[0]=0x7FU; col[1]=0x02U; col[2]=0x0CU; col[3]=0x02U; col[4]=0x7FU; break;
    case 'N': col[0]=0x7FU; col[1]=0x04U; col[2]=0x08U; col[3]=0x10U; col[4]=0x7FU; break;
    case 'O': col[0]=0x3EU; col[1]=0x41U; col[2]=0x41U; col[3]=0x41U; col[4]=0x3EU; break;
    case 'P': col[0]=0x7FU; col[1]=0x09U; col[2]=0x09U; col[3]=0x09U; col[4]=0x06U; break;
    case 'Q': col[0]=0x3EU; col[1]=0x41U; col[2]=0x51U; col[3]=0x21U; col[4]=0x5EU; break;
    case 'R': col[0]=0x7FU; col[1]=0x09U; col[2]=0x19U; col[3]=0x29U; col[4]=0x46U; break;
    case 'S': col[0]=0x46U; col[1]=0x49U; col[2]=0x49U; col[3]=0x49U; col[4]=0x31U; break;
    case 'T': col[0]=0x01U; col[1]=0x01U; col[2]=0x7FU; col[3]=0x01U; col[4]=0x01U; break;
    case 'U': col[0]=0x3FU; col[1]=0x40U; col[2]=0x40U; col[3]=0x40U; col[4]=0x3FU; break;
    case 'V': col[0]=0x1FU; col[1]=0x20U; col[2]=0x40U; col[3]=0x20U; col[4]=0x1FU; break;
    case 'W': col[0]=0x7FU; col[1]=0x20U; col[2]=0x18U; col[3]=0x20U; col[4]=0x7FU; break;
    case 'X': col[0]=0x63U; col[1]=0x14U; col[2]=0x08U; col[3]=0x14U; col[4]=0x63U; break;
    case 'Y': col[0]=0x07U; col[1]=0x08U; col[2]=0x70U; col[3]=0x08U; col[4]=0x07U; break;
    case 'Z': col[0]=0x61U; col[1]=0x51U; col[2]=0x49U; col[3]=0x45U; col[4]=0x43U; break;

    case ':': col[0]=0x00U; col[1]=0x36U; col[2]=0x36U; col[3]=0x00U; col[4]=0x00U; break;
    case '%': col[0]=0x23U; col[1]=0x13U; col[2]=0x08U; col[3]=0x64U; col[4]=0x62U; break;
    case '.': col[0]=0x00U; col[1]=0x60U; col[2]=0x60U; col[3]=0x00U; col[4]=0x00U; break;
    case '-': col[0]=0x08U; col[1]=0x08U; col[2]=0x08U; col[3]=0x08U; col[4]=0x08U; break;
    case '+': col[0]=0x08U; col[1]=0x08U; col[2]=0x3EU; col[3]=0x08U; col[4]=0x08U; break;
    case '/': col[0]=0x20U; col[1]=0x10U; col[2]=0x08U; col[3]=0x04U; col[4]=0x02U; break;
    case '_': col[0]=0x40U; col[1]=0x40U; col[2]=0x40U; col[3]=0x40U; col[4]=0x40U; break;
    case '#': col[0]=0x14U; col[1]=0x7FU; col[2]=0x14U; col[3]=0x7FU; col[4]=0x14U; break;
    case '[': col[0]=0x00U; col[1]=0x7FU; col[2]=0x41U; col[3]=0x41U; col[4]=0x00U; break;
    case ']': col[0]=0x00U; col[1]=0x41U; col[2]=0x41U; col[3]=0x7FU; col[4]=0x00U; break;
    case '=': col[0]=0x14U; col[1]=0x14U; col[2]=0x14U; col[3]=0x14U; col[4]=0x14U; break;
    case ',': col[0]=0x00U; col[1]=0x50U; col[2]=0x30U; col[3]=0x00U; col[4]=0x00U; break;
    case ' ':
    default:
        break;
    }
}

static void lcd_port_draw_char_5x7(int x, int y, char ch, u16 fg, u16 bg)
{
    u8 glyph[5];
    int col;
    int row;
    int sx;
    int sy;
    int scale = LCD_FONT_SCALE;

    if (scale < 1) {
        scale = 1;
    }

    lcd_font_get_5x7(ch, glyph);

    /*
     * Character cell: 6 columns x 8 rows.
     * 5x7 glyph + 1 col spacing + 1 row spacing.
     */
    for (col = 0; col < 6; col++) {
        for (row = 0; row < 8; row++) {
            u16 color = bg;

            if (col < 5 && row < 7) {
                if ((glyph[col] >> row) & 0x01U) {
                    color = fg;
                }
            }

            for (sy = 0; sy < scale; sy++) {
                for (sx = 0; sx < scale; sx++) {
                    lcd_port_draw_pixel(x + col * scale + sx,
                                        y + row * scale + sy,
                                        color);
                }
            }
        }
    }
}

/* =========================================================
 * Public functions
 * ========================================================= */
void lcd_port_flush(void)
{
    Xil_DCacheFlushRange((UINTPTR)LCD_FRAMEBUFFER_BASEADDR,
                         (u32)LCD_FRAME_BYTES);
}

void lcd_port_flush_rect(int x, int y, int w, int h)
{
    int row;
    if (w <= 0 || h <= 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x < 0) {
        if (x <= -w) return;
        w += x;
        x = 0;
    }
    if (y < 0) {
        if (y <= -h) return;
        h += y;
        y = 0;
    }
    if (w > LCD_WIDTH - x) w = LCD_WIDTH - x;
    if (h > LCD_HEIGHT - y) h = LCD_HEIGHT - y;
    /* VDMA still scans full frames; only CPU-modified rows need flushing. */
    for (row = y; row < y + h; ++row) {
        UINTPTR addr = (UINTPTR)LCD_FRAMEBUFFER_BASEADDR +
            ((UINTPTR)row * LCD_WIDTH + (UINTPTR)x) * LCD_BYTES_PER_PIXEL;
        Xil_DCacheFlushRange(addr, (u32)w * LCD_BYTES_PER_PIXEL);
    }
}

int lcd_port_init(void)
{
    int status;

    /*xil_printf("lcd_port_init(): start\r\n");
    xil_printf("  LCD_WIDTH              = %d\r\n", LCD_WIDTH);
    xil_printf("  LCD_HEIGHT             = %d\r\n", LCD_HEIGHT);
    xil_printf("  LCD_FB_FORMAT          = RGB888 / 24-bit\r\n");
    xil_printf("  LCD_FRAMEBUFFER_BASE   = 0x%08lx\r\n", (u32)LCD_FRAMEBUFFER_BASEADDR);
    xil_printf("  LCD_FRAME_BYTES        = %lu\r\n", (u32)LCD_FRAME_BYTES);
    xil_printf("  LCD_DYNCLK_BASEADDR    = 0x%08lx\r\n", (u32)LCD_DYNCLK_BASEADDR);
    xil_printf("  VDMA_DEVICE_ID         = %d\r\n", XPAR_AXIVDMA_0_DEVICE_ID);
    xil_printf("  VTC_DEVICE_ID          = %d\r\n", XPAR_V_TC_0_DEVICE_ID);
    xil_printf("  VDMA_MM2S_TDATA_WIDTH  = %lu\r\n",
               (u32)XPAR_AXIVDMA_0_M_AXIS_MM2S_TDATA_WIDTH);*/

    if (XPAR_AXIVDMA_0_M_AXIS_MM2S_TDATA_WIDTH != 24U) {
        xil_printf("WARNING: this lcd_port.c expects VDMA MM2S 24-bit RGB888\r\n");
    }

    /*
     * 1. Initialize display controller:
     *    - dynclk
     *    - VTC driver
     */
    status = DisplayInitialize(&g_disp,
                               XPAR_V_TC_0_DEVICE_ID,
                               LCD_DYNCLK_BASEADDR);
    if (status != XST_SUCCESS) {
        xil_printf("DisplayInitialize failed: %d\r\n", status);
        return -1;
    }

    /*
     * 2. Set display mode.
     *
     * Make sure lcd_modes.h contains selected VideoMode.
     */
#if (LCD_WIDTH == 480) && (LCD_HEIGHT == 272)
    status = DisplaySetMode(&g_disp, &VMODE_480x272);
#elif (LCD_WIDTH == 800) && (LCD_HEIGHT == 480)
    status = DisplaySetMode(&g_disp, &VMODE_800x480);
#else
    /*
     * If your resolution is not listed here, add the corresponding
     * VideoMode in lcd_modes.h and add another #elif branch.
     */
    status = DisplaySetMode(&g_disp, &VMODE_800x480);
#endif

    if (status != XST_SUCCESS) {
        xil_printf("DisplaySetMode failed: %d\r\n", status);
        return -2;
    }

    /*
     * 3. Clear framebuffer before VDMA starts reading it.
     */
    lcd_port_clear(LCD_COLOR_BLACK);
    lcd_port_flush();

    /*
     * 4. Start VDMA MM2S/read channel.
     *
     * Your VDMA:
     *   INCLUDE_MM2S = 1
     *   INCLUDE_S2MM = 0
     *
     * Therefore ONLY_READ is required.
     *
     * hsize is in pixels. vdma_api.c converts it to bytes:
     *   hsize * (Config->Mm2SStreamWidth >> 3)
     *
     * Since Mm2SStreamWidth = 24:
     *   stride = LCD_WIDTH * 3
     */
    status = run_vdma_frame_buffer(&g_vdma,
                                   XPAR_AXIVDMA_0_DEVICE_ID,
                                   LCD_WIDTH,
                                   LCD_HEIGHT,
                                   LCD_FRAMEBUFFER_BASEADDR,
                                   0,
                                   0,
                                   ONLY_READ);
    if (status != XST_SUCCESS) {
        xil_printf("run_vdma_frame_buffer ONLY_READ failed: %d\r\n", status);
        return -3;
    }

    /*
     * 5. Start timing output.
     */
    status = DisplayStart(&g_disp);
    if (status != XST_SUCCESS) {
        xil_printf("DisplayStart failed: %d\r\n", status);
        return -4;
    }

#if LCD_PORT_COLOR_TEST_ENABLE
    /*
     * Simple color test.
     * If red/blue swapped, set LCD_PORT_BGR_ORDER to 1.
     */
    lcd_port_clear(LCD_COLOR_RED);
    lcd_port_flush();
    usleep(200000);

    lcd_port_clear(LCD_COLOR_GREEN);
    lcd_port_flush();
    usleep(200000);

    lcd_port_clear(LCD_COLOR_BLUE);
    lcd_port_flush();
    usleep(200000);

    lcd_port_clear(LCD_COLOR_BLACK);
    lcd_port_flush();
#endif

    lcd_port_draw_string(10, 10, "LCD OK", LCD_COLOR_GREEN, LCD_COLOR_BLACK);
    lcd_port_flush();

    xil_printf("lcd_port_init(): OK\r\n");

    return 0;
}

void lcd_port_clear(u16 color)
{
    lcd_port_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void lcd_port_draw_pixel(int x, int y, u16 color)
{
    u32 offset;
    u8 r;
    u8 g;
    u8 b;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
        return;
    }

    r = rgb565_to_r8(color);
    g = rgb565_to_g8(color);
    b = rgb565_to_b8(color);

    /*
     * RGB888 framebuffer:
     *   byte0 = R
     *   byte1 = G
     *   byte2 = B
     *
     * If panel shows red/blue swapped, set LCD_PORT_BGR_ORDER to 1.
     */
    offset = (((u32)y * (u32)LCD_WIDTH) + (u32)x) * 3U;

#if LCD_PORT_BGR_ORDER
    g_fb[offset + 0U] = b;
    g_fb[offset + 1U] = g;
    g_fb[offset + 2U] = r;
#else
    g_fb[offset + 0U] = r;
    g_fb[offset + 1U] = g;
    g_fb[offset + 2U] = b;
#endif
}

void lcd_port_draw_line(int x0, int y0, int x1, int y1, u16 color)
{
    int dx;
    int sx;
    int dy;
    int sy;
    int err;
    int e2;

    dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    sx = (x0 < x1) ? 1 : -1;
    dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    sy = (y0 < y1) ? 1 : -1;
    err = dx + dy;

    while (1) {
        lcd_port_draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void lcd_port_fill_rect(int x, int y, int w, int h, u16 color)
{
    int xx;
    int yy;
    u8 r;
    u8 g;
    u8 b;

    if (w <= 0 || h <= 0) {
        return;
    }

    r = rgb565_to_r8(color);
    g = rgb565_to_g8(color);
    b = rgb565_to_b8(color);

    for (yy = 0; yy < h; yy++) {
        int py = y + yy;

        if (py < 0 || py >= LCD_HEIGHT) {
            continue;
        }

        for (xx = 0; xx < w; xx++) {
            int px = x + xx;
            u32 offset;

            if (px < 0 || px >= LCD_WIDTH) {
                continue;
            }

            offset = (((u32)py * (u32)LCD_WIDTH) + (u32)px) * 3U;

#if LCD_PORT_BGR_ORDER
            g_fb[offset + 0U] = b;
            g_fb[offset + 1U] = g;
            g_fb[offset + 2U] = r;
#else
            g_fb[offset + 0U] = r;
            g_fb[offset + 1U] = g;
            g_fb[offset + 2U] = b;
#endif
        }
    }
}

void lcd_port_draw_string(int x, int y, const char *s, u16 fg, u16 bg)
{
    int cursor_x;
    int char_w;

    if (s == 0) {
        return;
    }

    cursor_x = x;
    char_w = 6 * LCD_FONT_SCALE;

    while (*s != '\0') {
        if (*s == '\n') {
            y += 8 * LCD_FONT_SCALE;
            cursor_x = x;
        } else {
            lcd_port_draw_char_5x7(cursor_x, y, *s, fg, bg);
            cursor_x += char_w;
        }

        s++;
    }
}
