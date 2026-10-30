// Wii Pong - libogc homebrew. Mier Wiimotom alebo pouzi krizovy ovladac.
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#define WHITE 0xFF80FF80
#define BLACK 0x00800080
#define GREY  0x80808080

static void *xfb[2];
static int fb = 0;
static GXRModeObj *rmode;
static int W, H;

static void rect(int x, int y, int w, int h, u32 c) {
    int x0 = x & ~1, x1 = (x + w + 1) & ~1;
    if (x0 < 0) x0 = 0;
    if (x1 > W) x1 = W;
    u32 *p = (u32 *)xfb[fb];
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= H) continue;
        for (int i = x0; i < x1; i += 2) p[j * (W / 2) + i / 2] = c;
    }
}

int main(void) {
    VIDEO_Init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    rmode = VIDEO_GetPreferredMode(NULL);
    W = rmode->fbWidth;
    H = rmode->xfbHeight;
    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    const int PW = 12, PH = 80, BS = 12, WIN = 7;
    float py = H / 2, ay = H / 2;
    float bx = W / 2, by = H / 2, vx = 5, vy = 3;
    int ps = 0, as = 0, over = 0;

    while (1) {
        WPAD_ScanPads();
        u32 down = WPAD_ButtonsDown(0), held = WPAD_ButtonsHeld(0);
        if (down & WPAD_BUTTON_HOME) break;

        if (over) {
            if (down & WPAD_BUTTON_A) { ps = as = over = 0; bx = W / 2; by = H / 2; vx = 5; vy = 3; }
        } else {
            // Hrac: IR ukazovatko ak je viditelne, inak krizovy ovladac
            ir_t ir;
            WPAD_IR(0, &ir);
            if (ir.valid) py = ir.y * H / rmode->viHeight;
            if (held & WPAD_BUTTON_UP)   py -= 7;
            if (held & WPAD_BUTTON_DOWN) py += 7;
            if (py < PH / 2) py = PH / 2;
            if (py > H - PH / 2) py = H - PH / 2;

            // Jednoduche AI
            if (by > ay + 8) ay += 4; else if (by < ay - 8) ay -= 4;

            bx += vx; by += vy;
            if (by < 0 || by > H - BS) vy = -vy;

            // Odrazy od palok
            if (vx < 0 && bx < 20 + PW && bx > 20 && by + BS > py - PH / 2 && by < py + PH / 2) {
                vx = -vx * 1.05f; vy += (by - py) * 0.08f;
            }
            if (vx > 0 && bx + BS > W - 20 - PW && bx < W - 20 && by + BS > ay - PH / 2 && by < ay + PH / 2) {
                vx = -vx * 1.05f; vy += (by - ay) * 0.08f;
            }

            // Skore
            if (bx < -BS || bx > W) {
                if (bx < 0) as++; else ps++;
                bx = W / 2; by = H / 2;
                vx = (bx < 0 || as > ps) ? 5 : -5; vy = (rand() % 7) - 3;
                if (ps >= WIN || as >= WIN) over = 1;
            }
        }

        // Kreslenie
        rect(0, 0, W, H, BLACK);
        for (int y = 0; y < H; y += 24) rect(W / 2 - 2, y, 4, 12, GREY);
        rect(20, (int)py - PH / 2, PW, PH, WHITE);
        rect(W - 20 - PW, (int)ay - PH / 2, PW, PH, WHITE);
        if (!over) rect((int)bx, (int)by, BS, BS, WHITE);
        for (int i = 0; i < ps; i++) rect(W / 2 - 40 - i * 18, 20, 12, 12, WHITE);
        for (int i = 0; i < as; i++) rect(W / 2 + 28 + i * 18, 20, 12, 12, WHITE);
        if (over) rect(ps > as ? W / 4 - 40 : 3 * W / 4 - 40, H / 2 - 20, 80, 40, WHITE);

        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb ^= 1;
    }
    return 0;
}
