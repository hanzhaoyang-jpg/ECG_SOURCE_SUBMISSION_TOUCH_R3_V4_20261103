/* Link with lcd_port.c and --gc-sections; hardware init is not executed. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/lcd_port.h"
#include "xil_cache.h"

static int calls;
static UINTPTR first, last;
static u32 length;
void Xil_DCacheFlushRange(INTPTR addr, u32 len)
{
    UINTPTR base=(UINTPTR)LCD_FRAMEBUFFER_BASEADDR;
    assert((UINTPTR)addr >= base);
    assert((UINTPTR)addr + len <= base + LCD_FRAME_BYTES);
    if (!calls) first=(UINTPTR)addr;
    last=(UINTPTR)addr;
    length=len;
    ++calls;
}
int main(void)
{
    lcd_port_flush_rect(10,48,7,20);
    assert(calls==20 && length==7*LCD_BYTES_PER_PIXEL);
    assert(first==(UINTPTR)LCD_FRAMEBUFFER_BASEADDR+
                 (48*LCD_WIDTH+10)*LCD_BYTES_PER_PIXEL);
    assert(last-first==19*LCD_WIDTH*LCD_BYTES_PER_PIXEL);
    calls=0;
    lcd_port_flush_rect(-2,-3,5,7);
    assert(calls==4 && length==3*LCD_BYTES_PER_PIXEL);
    calls=0;
    lcd_port_flush_rect(LCD_WIDTH-1,LCD_HEIGHT-1,INT_MAX,INT_MAX);
    assert(calls==1 && length==LCD_BYTES_PER_PIXEL);
    calls=0;
    lcd_port_flush_rect(INT_MIN,0,INT_MAX,1);
    lcd_port_flush_rect(0,0,0,1);
    lcd_port_flush_rect(0,LCD_HEIGHT,1,1);
    assert(calls==0);
    puts("LCD partial cache flush tests passed.");
    return 0;
}
