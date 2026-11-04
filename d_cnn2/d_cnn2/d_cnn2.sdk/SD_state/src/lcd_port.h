/* =========================================================
 * File: lcd_port.h
 * LCD hardware adaptation layer
 * AXI VDMA MM2S RGB888 framebuffer output
 * ========================================================= */
#ifndef LCD_PORT_H
#define LCD_PORT_H

#include "xil_types.h"

#ifndef LCD_WIDTH
#define LCD_WIDTH     800
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT    480
#endif

#ifndef LCD_FONT_SCALE
#define LCD_FONT_SCALE 1
#endif

/*
 * VDMA MM2S stream width = 24 bits => RGB888 framebuffer.
 */
#define LCD_FB_FORMAT_RGB565     16
#define LCD_FB_FORMAT_RGB888     24
#define LCD_FB_FORMAT_XRGB8888   32

#ifndef LCD_FB_FORMAT
#define LCD_FB_FORMAT LCD_FB_FORMAT_RGB888
#endif

#ifndef LCD_BYTES_PER_PIXEL
#define LCD_BYTES_PER_PIXEL 3U
#endif

#ifndef LCD_FRAME_BYTES
#define LCD_FRAME_BYTES ((u32)LCD_WIDTH * (u32)LCD_HEIGHT * (u32)LCD_BYTES_PER_PIXEL)
#endif

/*
 * DDR framebuffer base.
 * Must not overlap code/data/heap/stack/other buffers.
 */
#ifndef LCD_FRAMEBUFFER_BASEADDR
#define LCD_FRAMEBUFFER_BASEADDR  0x10000000U
#endif

/*
 * RGB565 colors used by upper drawing layer.
 * lcd_port.c converts them to RGB888 framebuffer pixels.
 */
#define LCD_COLOR_BLACK     0x0000U
#define LCD_COLOR_WHITE     0xFFFFU
#define LCD_COLOR_RED       0xF800U
#define LCD_COLOR_GREEN     0x07E0U
#define LCD_COLOR_BLUE      0x001FU
#define LCD_COLOR_YELLOW    0xFFE0U
#define LCD_COLOR_CYAN      0x07FFU
#define LCD_COLOR_MAGENTA   0xF81FU
#define LCD_COLOR_GRAY      0x8410U
#define LCD_COLOR_DARKGRAY  0x4208U

int  lcd_port_init(void);

void lcd_port_clear(u16 color);
void lcd_port_draw_pixel(int x, int y, u16 color);
void lcd_port_draw_line(int x0, int y0, int x1, int y1, u16 color);
void lcd_port_fill_rect(int x, int y, int w, int h, u16 color);
void lcd_port_draw_char(int x, int y, char ch, u16 fg, u16 bg);
void lcd_port_draw_string(int x, int y, const char *s, u16 fg, u16 bg);

void lcd_port_flush(void);
void lcd_port_flush_rect(int x, int y, int w, int h);

#endif /* LCD_PORT_H */
