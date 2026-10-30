// Wii Pong - libogc homebrew. Mier Wiimotom, pouzi krizovy ovladac alebo Nunchuk!
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h> // Potrebné pre hardvérový zvuk

// Prepočítané YUV422 farby pre Wii Video Hardvér:
#define BLUE_BG     0x1F6B1FDC  // Nádherné tmavomodré pozadie
#define WHITE_PAD   0xFF80FF80  // Čistá biela pre pálky a text
#define GREEN_BALL  0x9636962C  // Výrazná retro zelená pre loptičku
#define GREY_LINE   0x80808080  // Sivá pre stredovú čiaru

static void *xfb[2]; // Opravené späť na pole dvoch ukazovateľov z tvojho pôvodného kódu
static int fb = 0;
static GXRModeObj *rmode;
static int W, H;

// Zvukové funkcie cez ASND subsystem
static void play_bounce_sound(void) {
    ASND_SetVoice(0, VOICE_MONO_16BIT, 44100, 0, NULL, 0, 120, 120, NULL);
    ASND_StatusVoice(0);
}

static void play_victory_song(void) {
    for (int t = 0; t < 3; t++) {
        ASND_SetVoice(0, VOICE_MONO_16BIT, 44100, 0, NULL, 0, 120, 120, NULL);
        for(int i=0; i<8; i++) VIDEO_WaitVSync();
    }
}

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

// Vykreslenie čísel pomocou 7-segmentového displeja
static void draw_digit(int num, int x, int y, u32 c) {
    const int segs[10][7] = {
        {1,1,1,0,1,1,1}, // 0
        {0,0,1,0,0,1,0}, // 1
        {1,0,1,1,1,0,1}, // 2
        {1,0,1,1,0,1,1}, // 3
        {0,1,1,1,0,1,0}, // 4
        {1,1,0,1,0,1,1}, // 5
        {1,1,0,1,1,1,1}, // 6
        {1,0,1,0,0,1,0}, // 7
        {1,1,1,1,1,1,1}, // 8
        {1,1,1,1,0,1,1}  // 9
    };
    int sw = 25, sh = 5, th = 45; 
    if(num < 0 || num > 9) return;

    if (segs[num][0]) rect(x, y, sw, sh, c);          
    if (segs[num][1]) rect(x, y, sh, th/2, c);       
    if (segs[num][2]) rect(x + sw - sh, y, sh, th/2, c); 
    if (segs[num][3]) rect(x, y + th/2 - sh/2, sw, sh, c); 
    if (segs[num][4]) rect(x, y + th/2, sh, th/2, c); 
    if (segs[num][5]) rect(x + sw - sh, y + th/2, sh, th/2, c); 
    if (segs[num][6]) rect(x, y + th - sh, sw, sh, c); 
}

int main(void) {
    VIDEO_Init();
    WPAD_Init();
    ASND_Init(); // Inicializácia zvuku
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
            // Hrac: IR ukazovatko ak je viditelne, inak Nunchuk alebo krizovy ovladac
            ir_t ir;
            WPAD_IR(0, &ir);
            
            // 1. Kontrola IR senzora
            if (ir.valid) {
                py = ir.y * H / rmode->viHeight;
            }
            
            // 2. Kontrola Nunchuku (analógová páčka Y)
            expansion_t exp;
            WPAD_Expansion(0, &exp);
            if (exp.type == WPAD_EXP_NUNCHUK) {
                // Páčka Y vracia hodnoty (hore kladné, dole záporné). Ak sa pohne mimo mŕtvej zóny:
                if (exp.nunchuk.js.pos.y > 15 || exp.nunchuk.js.pos.y < -15) {
                    // Odpočítavame, pretože na obrazovke je 0 hore a H dole
                    py -= (exp.nunchuk.js.pos.y * 0.08f); 
                }
            }
            
            // 3. Kontrola klasického D-Padu
            if (held & WPAD_BUTTON_UP)   py -= 7;
            if (held & WPAD_BUTTON_DOWN) py += 7;
            
            // Hranice obrazovky pre pálku
            if (py < PH / 2) py = PH / 2;
            if (py > H - PH / 2) py = H - PH / 2;

            // Jednoduche AI
            if (by > ay + 8) ay += 4; else if (by < ay - 8) ay -= 4;

            bx += vx; by += vy;
            
            if (by < 0 || by > H - BS) {
                vy = -vy;
                play_bounce_sound();
            }

            if (vx < 0 && bx < 20 + PW && bx > 20 && by + BS > py - PH / 2 && by < py + PH / 2) {
                vx = -vx * 1.05f; vy += (by - py) * 0.08f;
                play_bounce_sound();
            }
            if (vx > 0 && bx + BS > W - 20 - PW && bx < W - 20 && by + BS > ay - PH / 2 && by < ay + PH / 2) {
                vx = -vx * 1.05f; vy += (by - ay) * 0.08f;
                play_bounce_sound();
            }

            if (bx < -BS || bx > W) {
                if (bx < 0) as++; else ps++;
                bx = W / 2; by = H / 2;
                vx = (bx < 0 || as > ps) ? 5 : -5; vy = (rand() % 7) - 3;
                
                if (ps >= WIN || as >= WIN) {
                    over = 1;
                    play_victory_song();
                }
            }
        }

        // Kreslenie
        rect(0, 0, W, H, BLUE_BG); 
        for (int y = 0; y < H; y += 24) rect(W / 2 - 2, y, 4, 12, GREY_LINE);
        
        rect(20, (int)py - PH / 2, PW, PH, WHITE_PAD); 
        rect(W - 20 - PW, (int)ay - PH / 2, PW, PH, WHITE_PAD); 
        
        if (!over) rect((int)bx, (int)by, BS, BS, GREEN_BALL); 

        // Čísla skóre
        draw_digit(ps, W / 2 - 60, 25, WHITE_PAD);
        draw_digit(as, W / 2 + 35, 25, WHITE_PAD);

        if (over) rect(ps > as ? W / 4 - 40 : 3 * W / 4 - 40, H / 2 - 20, 80, 40, WHITE_PAD);

        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb ^= 1;
    }
    return 0;
}
