#include <assert.h>
#include <stdio.h>
#include "../../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/ecg_touch.c"

static int tap(int x, int y)
{
    assert(contact(1, x, y) == TOUCH_NONE);
    assert(contact(1, x, y) == TOUCH_NONE);
    return contact(0, 0, 0);
}

static int oriented_tap(int x, int y, int swap, int fx, int fy)
{
    if (fx) x = LCD_WIDTH - 1 - x;
    if (fy) y = LCD_HEIGHT - 1 - y;
    return swap ? tap(y, x) : tap(x, y);
}

int main(void)
{
    int swap, fx, fy, i, y = LCD_HEIGHT - TOUCH_FOOTER_HEIGHT / 2;
    assert(tap(50, 50) == TOUCH_NONE && stage == 0); /* Held at boot. */
    for (swap = 0; swap < 2; ++swap)
        for (fx = 0; fx < 2; ++fx)
            for (fy = 0; fy < 2; ++fy) {
                stage = 0;
                assert(oriented_tap(50, 50, swap, fx, fy) == TOUCH_NONE && stage == 1);
                assert(oriented_tap(LCD_WIDTH-51, 50, swap, fx, fy) == TOUCH_NONE && stage == 2);
                assert(oriented_tap(50, LCD_HEIGHT-51, swap, fx, fy) == TOUCH_NONE && stage == 3);
                for (i = 0; i < TOUCH_BUTTON_COUNT; ++i)
                    assert(oriented_tap((2*i+1)*(LCD_WIDTH/TOUCH_BUTTON_COUNT)/2,
                           y, swap, fx, fy) == i);
                assert(oriented_tap(10, y-40, swap, fx, fy) == TOUCH_NONE);
                assert(oriented_tap(0, y, swap, fx, fy) == TOUCH_NONE);
            }
    stage = 3; swapped = flip_x = flip_y = 0;
    contact(1, 30, y); contact(1, 30, y); contact(2, 30, y);
    assert(contact(0, 0, 0) == TOUCH_NONE);
    contact(1, 30, y); contact(1, 30, y); contact(-1, 0, 0);
    assert(contact(0, 0, 0) == TOUCH_NONE);
    contact(1, 30, y); contact(1, 30, y); contact(1, 170, y);
    assert(contact(0, 0, 0) == TOUCH_NONE);
    puts("PASS touch: 8 orientations, all 6 buttons, bounds, held-at-boot, drag, multi-touch and bus fault rejection");
    return 0;
}
