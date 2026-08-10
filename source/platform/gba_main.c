#include <gba.h>

#include "gcalc/app.h"

static AppState app EWRAM_BSS;

static u16 appKeysFromGba(u16 keys)
{
    u16 result = 0;

    if ((keys & KEY_A) != 0)
        result |= APP_KEY_A;
    if ((keys & KEY_B) != 0)
        result |= APP_KEY_B;
    if ((keys & KEY_SELECT) != 0)
        result |= APP_KEY_SELECT;
    if ((keys & KEY_START) != 0)
        result |= APP_KEY_START;
    if ((keys & KEY_RIGHT) != 0)
        result |= APP_KEY_RIGHT;
    if ((keys & KEY_LEFT) != 0)
        result |= APP_KEY_LEFT;
    if ((keys & KEY_UP) != 0)
        result |= APP_KEY_UP;
    if ((keys & KEY_DOWN) != 0)
        result |= APP_KEY_DOWN;
    if ((keys & KEY_R) != 0)
        result |= APP_KEY_R;
    if ((keys & KEY_L) != 0)
        result |= APP_KEY_L;

    return result;
}

int main(void)
{
    GfxSurface framebuffer;

    irqInit();
    irqEnable(IRQ_VBLANK);
    SetMode(MODE_3 | BG2_ENABLE);

    framebuffer.pixels = (volatile u16 *)MODE3_FB;
    framebuffer.width = GBA_SCREEN_WIDTH;
    framebuffer.height = GBA_SCREEN_HEIGHT;
    framebuffer.stride = GBA_SCREEN_WIDTH;

    appInit(&app);

    for (;;) {
        u16 pressed;
        u16 held;

        VBlankIntrWait();
        scanKeys();
        pressed = appKeysFromGba(keysDown());
        held = appKeysFromGba(keysHeld());
        appHandleKeys(&app, pressed, held);
        appTick(&app);
        appRender(&app, &framebuffer);
    }
}
