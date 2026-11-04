/* Host test: actual sweep/ring logic with a memory-only LCD port. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/lcd_wave.c"

static u16 pixels[LCD_HEIGHT][LCD_WIDTH];
static unsigned long flush_pixels;
static int full_flushes, strings;
static EcgRing ring;
static LcdEcgInfo info;

int lcd_port_init(void) { return 0; }
void lcd_port_draw_pixel(int x, int y, u16 color)
{
    assert(x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT);
    pixels[y][x] = color;
}
void lcd_port_fill_rect(int x, int y, int w, int h, u16 color)
{
    int xx, yy;
    for (yy = y; yy < y + h; ++yy)
        for (xx = x; xx < x + w; ++xx)
            lcd_port_draw_pixel(xx, yy, color);
}
void lcd_port_clear(u16 color)
{
    lcd_port_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}
void lcd_port_draw_line(int x0, int y0, int x1, int y1, u16 color)
{
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;
    for (;;) {
        lcd_port_draw_pixel(x0,y0,color);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2>=dy) { err+=dy; x0+=sx; }
        if (e2<=dx) { err+=dx; y0+=sy; }
    }
}
void lcd_port_draw_string(int x,int y,const char *s,u16 fg,u16 bg)
{
    (void)x; (void)y; (void)s; (void)fg; (void)bg;
    ++strings;
}
void lcd_port_flush(void) { ++full_flushes; }
void lcd_port_flush_rect(int x,int y,int w,int h)
{
    assert(x>=0 && y>=0 && w>0 && h>0);
    assert(x+w<=LCD_WIDTH && y+h<=LCD_HEIGHT);
    flush_pixels += (unsigned long)w*h;
}
static void push(int n, float value)
{
    while (n-- > 0) ecg_ring_push(&ring, value, value);
}
static void restart(void)
{
    assert(lcd_wave_init()==0);
    ecg_ring_init(&ring);
    memset(&info,0,sizeof(info));
    info.cls_conf=ECG_CONFIDENCE_UNAVAILABLE;
    lcd_wave_draw(&ring,&info);
    full_flushes=strings=0;
    flush_pixels=0;
}
int main(void)
{
    int i, old_strings;
    u16 saved;
    restart();
    lcd_wave_draw(&ring,&info);
    assert(flush_pixels==0 && strings==0);
    info.recording=1;
    lcd_wave_draw(&ring,&info);
    assert(pixels[LCD_HEIGHT-20][5]==LCD_COLOR_RED);
    assert(pixels[LCD_HEIGHT-20][LCD_WIDTH/TOUCH_BUTTON_COUNT+5]==LCD_COLOR_DARKGRAY);
    info.touch_hint="TAP TOP LEFT";
    lcd_wave_draw(&ring,&info);
    assert(pixels[LCD_HEIGHT-20][5]==LCD_COLOR_BLACK);
    restart();
    push(1,0);
    lcd_wave_draw(&ring,&info);
    push(1,1);
    lcd_wave_draw(&ring,&info);
    push(1,-1);
    lcd_wave_draw(&ring,&info);
    assert(columns[0].lo[0]==f_to_y_scaled(1,LCD_FILT_WAVE_SCALE));
    assert(columns[0].hi[0]==f_to_y_scaled(-1,LCD_FILT_WAVE_SCALE));
    assert(flush_pixels < (unsigned long)WAVE_W*WAVE_H/10);
    saved=pixels[WAVE_Y0+30][WAVE_X0];
    push(30,0);
    lcd_wave_draw(&ring,&info);
    assert(pixels[WAVE_Y0+30][WAVE_X0]==saved);
    lcd_wave_mark_rpeak(5);
    lcd_wave_mark_rpeak(15);
    lcd_wave_draw(&ring,&info);
    assert(pixels[WAVE_Y0+2][WAVE_X0+sample_column(5)]==LCD_COLOR_RED);
    assert(pixels[WAVE_Y0+2][WAVE_X0+sample_column(15)]==LCD_COLOR_RED);
    flush_pixels=0;
    lcd_wave_draw(&ring,&info);
    assert(flush_pixels==0);
    info.hr_bpm=80;
    lcd_wave_draw(&ring,&info);
    assert(strings==4);
    old_strings=strings;
    push(LCD_WIN_LEN-(int)ring.total_samples,0);
    lcd_wave_draw(&ring,&info);
    assert(columns[WAVE_W-1].valid);
    assert(!columns[0].valid);
    push(1,1);
    lcd_wave_draw(&ring,&info);
    assert(columns[0].valid && columns[0].cycle==1);
    assert(columns[0].lo[0]==columns[0].hi[0]);
    assert(columns[WAVE_W-1].cycle==0);
    assert(strings==old_strings);
    push(20,0);
    lcd_wave_draw(&ring,&info);
    assert(!columns[sample_column(5)].peak);
    assert(full_flushes==0);
    push(ECG_RING_SIZE+200,0);
    lcd_wave_draw(&ring,&info);
    assert(next_sample==ring.total_samples);
    ecg_ring_init(&ring);
    lcd_wave_draw(&ring,&info);
    for (i=0;i<WAVE_W;++i) assert(!columns[i].valid);
    /* Irregular batches across many complete sweeps. */
    for (i=0;i<1000;++i) {
        push(1+i%37,(float)(i%11)/10);
        lcd_wave_draw(&ring,&info);
        assert(next_sample==ring.total_samples);
        assert(columns[sample_column(next_sample-1)].valid);
    }
    assert(full_flushes==0);
    printf("LCD sweep tests passed (%dx%d, raw=%d).\n",
           LCD_WIDTH,LCD_HEIGHT,LCD_SHOW_RAW_WAVE);
    return 0;
}
