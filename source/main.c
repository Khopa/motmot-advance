// Wordle GBA — entry point
#include <tonc.h>

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);

    REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;
    m3_fill(RGB15(2, 2, 2));
    m3_rect(40, 60, 200, 100, RGB15(10, 17, 9));   // wordle green block

    while (1)
        VBlankIntrWait();
    return 0;
}
