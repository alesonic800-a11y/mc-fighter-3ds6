/*  MONCLOA'S FIGHTER - port nativo a Nintendo 3DS (5 luchadores)
 *  Render por software directo al framebuffer (sin GPU): sprites ya escalados y con el orden de
 *  memoria del framebuffer, de modo que dibujar un sprite es una copia secuencial por columnas.
 *  Mismo codigo para 3DS (libctru) y para pruebas en PC (-DHOST).                                  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "audio.h"
#ifdef __3DS__
#include <3ds.h>
#endif

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16;
#define SW 400
#define SH 240
#define BW 320
#define GROUND 218                      /* y del suelo en pantalla (655/3 del juego web) */
#define GRAV 0.5f
#define JUMPV (-8.2f)
#define NCH 6

/* ------------------------------------------------------------------ plataforma */
enum { B_A = 1, B_B = 2, B_X = 4, B_Y = 8, B_L = 16, B_R = 32, B_START = 64, B_SELECT = 128, B_UP = 256, B_DOWN = 512, B_LEFT = 1024, B_RIGHT = 2048 };
static u8 *FB_TOP, *FB_BOT;
#ifdef __3DS__
static u8 *FB_TOP_RIGHT;
#endif
static unsigned K_HELD, K_DOWN;

#ifdef __3DS__
static void plat_init(void) { gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false); gfxSet3D(true); romfsInit(); osSetSpeedupEnable(true); }
static void plat_quit(void) { audio_quit(); romfsExit(); gfxExit(); }
static int plat_running(void) { return aptMainLoop(); }
static unsigned mapkeys(u32 k) {
    unsigned r = 0;
    if (k & KEY_A) r |= B_A; if (k & KEY_B) r |= B_B; if (k & KEY_X) r |= B_X; if (k & KEY_Y) r |= B_Y;
    if (k & KEY_L) r |= B_L; if (k & KEY_R) r |= B_R; if (k & KEY_START) r |= B_START; if (k & KEY_SELECT) r |= B_SELECT;
    if (k & KEY_DUP) r |= B_UP; if (k & KEY_DDOWN) r |= B_DOWN; if (k & KEY_DLEFT) r |= B_LEFT; if (k & KEY_DRIGHT) r |= B_RIGHT;
    return r;
}
static void plat_begin(void) {
    u16 w, h; FB_TOP = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h); FB_TOP_RIGHT = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, &w, &h); FB_BOT = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    hidScanInput(); K_HELD = mapkeys(hidKeysHeld()); K_DOWN = mapkeys(hidKeysDown());
    circlePosition cp; hidCircleRead(&cp);                       /* el stick tambien mueve */
    unsigned st = 0; if (cp.dx > 40) st |= B_RIGHT; if (cp.dx < -40) st |= B_LEFT; if (cp.dy > 40) st |= B_UP; if (cp.dy < -40) st |= B_DOWN;
    static unsigned prev; K_DOWN |= st & ~prev; prev = st; K_HELD |= st;
}
static void plat_end(void) { gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
static u8 *load_file(const char *name, long *sz) {
    char p[96]; snprintf(p, sizeof p, "romfs:/%s", name); FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); u8 *b = (u8 *)malloc(n + 4); if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; } fclose(f); if (sz) *sz = n; return b;
}
#else /* ---------- PC: framebuffers en memoria, entrada por script, capturas PPM ---------- */
static u8 topbuf[SW * SH * 3], botbuf[BW * SH * 3];
static int host_frame, host_quit; static unsigned (*host_script)(int);
static void plat_init(void) {}
static void plat_quit(void) {}
static int plat_running(void) { return !host_quit; }
static void plat_begin(void) {
    FB_TOP = topbuf; FB_BOT = botbuf; unsigned h = host_script ? host_script(host_frame) : 0;
    K_DOWN = h & ~K_HELD; K_HELD = h; host_frame++;
}
static void plat_end(void) {}
static u8 *load_file(const char *name, long *sz) {
    char p[96]; snprintf(p, sizeof p, "romfs/%s", name); FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); u8 *b = (u8 *)malloc(n + 4);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; } fclose(f); if (sz) *sz = n; return b;
}
static void dump_ppm(const u8 *fb, int w, const char *path) {
    FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", w, SH);
    for (int y = 0; y < SH; y++) for (int x = 0; x < w; x++) { const u8 *p = fb + (x * SH + (SH - 1 - y)) * 3; fputc(p[2], f); fputc(p[1], f); fputc(p[0], f); }
    fclose(f);
}
#endif

/* ------------------------------------------------------------------ dibujo */
static inline void put(u8 *fb, int fbw, int x, int y, int r, int g, int b) {
    if ((unsigned)x >= (unsigned)fbw || (unsigned)y >= SH) return;
    u8 *p = fb + (x * SH + (SH - 1 - y)) * 3; p[0] = b; p[1] = g; p[2] = r;
}
static void rect(u8 *fb, int fbw, int x, int y, int w, int h, int r, int g, int b) {
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; } if (x + w > fbw) w = fbw - x; if (y + h > SH) h = SH - y; if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) { u8 *p = fb + ((x + i) * SH + (SH - y - h)) * 3; for (int j = 0; j < h; j++, p += 3) { p[0] = b; p[1] = g; p[2] = r; } }
}
static void rect_a(u8 *fb, int fbw, int x, int y, int w, int h, int r, int g, int b, int a /*0..256*/) {   /* rectangulo translucido */
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; } if (x + w > fbw) w = fbw - x; if (y + h > SH) h = SH - y; if (w <= 0 || h <= 0) return;
    int ia = 256 - a;
    for (int i = 0; i < w; i++) { u8 *p = fb + ((x + i) * SH + (SH - y - h)) * 3; for (int j = 0; j < h; j++, p += 3) { p[0] = (p[0] * ia + b * a) >> 8; p[1] = (p[1] * ia + g * a) >> 8; p[2] = (p[2] * ia + r * a) >> 8; } }
}
static u8 FONT[95 * 12];
static void text(u8 *fb, int fbw, int x, int y, const char *s, int sc, int r, int g, int b) {
    for (; *s; s++, x += 8 * sc) {
        int c = (unsigned char)*s; if (c < 32 || c > 126) continue; const u8 *gl = FONT + (c - 32) * 12;
        for (int j = 0; j < 12; j++) { u8 bits = gl[j]; if (!bits) continue; for (int i = 0; i < 8; i++) if (bits & (0x80 >> i)) rect(fb, fbw, x + i * sc, y + j * sc, sc, sc, r, g, b); }
    }
}
static int tw(const char *s, int sc) { return (int)strlen(s) * 8 * sc; }
static void text_o(u8 *fb, int fbw, int x, int y, const char *s, int sc, int r, int g, int b) {   /* con sombra */
    text(fb, fbw, x + sc, y + sc, s, sc, 0, 0, 0); text(fb, fbw, x, y, s, sc, r, g, b);
}
static void text_c(u8 *fb, int fbw, int y, const char *s, int sc, int r, int g, int b) { text_o(fb, fbw, (fbw - tw(s, sc)) / 2, y, s, sc, r, g, b); }

/* ------------------------------------------------------------------ sprites */
enum { P_IDLE, P_IDLE2, P_WALK, P_W1, P_W2, P_W3, P_W4, P_CROUCH, P_JUMP, P_BLOCK, P_HIT, P_FALL, P_KO, P_GETUP, P_WIN, P_PUNCH, P_KICK, P_LOW, P_AIR, P_SP1, P_SP2, P_FA5, P_SUPER, P_PROJ, P_FACE, NP };
static const char *PNAME[NP] = { "idle", "idle2", "walk", "w1", "w2", "w3", "w4", "crouch", "jump", "block", "hit", "fall", "ko", "getup", "win", "punch", "kick", "low", "air", "sp1", "sp2", "fa5", "super", "proj", "face" };
typedef struct { int w, h, ax; const u32 *px; } Spr;
static Spr SPR[NCH][NP];
static u8 *SPRBLOB;

static int load_sprites(void) {
    long n; SPRBLOB = load_file("sprites.bin", &n); if (!SPRBLOB || memcmp(SPRBLOB, "SPR1", 4)) return 0;
    const u8 *p = SPRBLOB + 8;
    for (int c = 0; c < NCH; c++) {
        u32 np; memcpy(&np, p, 4); p += 4;
        for (u32 i = 0; i < np; i++) {
            char name[13]; memcpy(name, p, 12); name[12] = 0; u16 w, h; s16 ax; memcpy(&w, p + 12, 2); memcpy(&h, p + 14, 2); memcpy(&ax, p + 16, 2); p += 20;
            for (int k = 0; k < NP; k++) if (!strcmp(name, PNAME[k])) { SPR[c][k].w = w; SPR[c][k].h = h; SPR[c][k].ax = ax; SPR[c][k].px = (const u32 *)p; }
            p += (size_t)w * h * 4;
        }
    }
    return 1;
}
static void blit(u8 *fb, const Spr *s, int x0, int y0, int flip, int white) {
    if (!s->px) return;
    int fy = SH - 1 - (y0 + s->h - 1), j0 = 0, j1 = s->h; if (fy < 0) j0 = -fy; if (fy + j1 > SH) j1 = SH - fy; if (j0 >= j1) return;
    for (int i = 0; i < s->w; i++) {
        int cx = x0 + i; if ((unsigned)cx >= SW) continue;
        const u32 *col = s->px + (size_t)(flip ? s->w - 1 - i : i) * s->h; u8 *d = fb + ((size_t)cx * SH + fy + j0) * 3;
        if (white) { for (int j = j0; j < j1; j++, d += 3) if (col[j] >> 24) { d[0] = d[1] = d[2] = 255; } }
        else for (int j = j0; j < j1; j++, d += 3) { u32 q = col[j]; if (q >> 24) { d[0] = (u8)q; d[1] = (u8)(q >> 8); d[2] = (u8)(q >> 16); } }
    }
}

/* ------------------------------------------------------------------ datos de luchadores */
typedef struct {
    const char *id, *name, *title; u8 col[3]; int hp; float speed, power; int height;
    const char *sp1n, *sp2n, *supn; int sp1dmg; float sp1spd; int sp2t; int sp2dmg; float sp2vx; int supt; int supdmg; u8 fx[3];
} Def;
enum { SP_RUSH, SP_SPIN };            /* sp2 */
enum { SU_NOVA, SU_BEAM, SU_RAIN, SU_STAMP };
static const Def DEFS[NCH] = {
    { "sanchez", "PERRO SANXE", "EL RESISTENTE", { 227, 6, 19 }, 100, 4.6f, 1.0f, 425, "MANUAL DE RESISTENCIA", "VUELO EN FALCON", "ATERRIZAJE EN FALCON", 10, 11, SP_RUSH, 15, 15, SU_STAMP, 30, { 255, 51, 68 } },
    { "feijoo", "ALBERTO FREIJOO", "EL MODERADO", { 29, 132, 206 }, 100, 4.3f, 1.0f, 410, "GAFAS FUERA", "EMPUJON DE CENTRO", "MAREA GALLEGA", 9, 12, SP_RUSH, 14, 11, SU_BEAM, 30, { 57, 168, 255 } },
    { "ayuso", "ISA DIAZ AYUSA", "LA LIBERTARIA", { 11, 178, 240 }, 95, 5.2f, 0.95f, 395, "CANA LIBERTARIA", "CARGA A LA MADRILENA", "LIBERTAD!", 10, 12, SP_RUSH, 13, 13, SU_RAIN, 28, { 255, 210, 63 } },
    { "almeida", "MARTINEZ-ALMEJA", "EL ALCALDE", { 42, 92, 170 }, 92, 5.4f, 0.9f, 355, "BALONAZO", "CHOTIS GIRATORIO", "BASTONAZO MUNICIPAL", 11, 14, SP_SPIN, 5, 4, SU_NOVA, 30, { 255, 255, 255 } },
    { "rajoy", "M. RAJOY", "EL REGISTRADOR", { 0, 86, 167 }, 108, 3.9f, 1.1f, 420, "PLASMAZO", "MARCHA RAPIDA", "COMPARECENCIA EN PLASMA", 12, 9, SP_RUSH, 13, 10, SU_BEAM, 30, { 57, 168, 255 } },
    { "alesonic", "ALESONIC", "EL BLOQUE TURQUESA", { 35, 164, 157 }, 102, 4.8f, 1.05f, 410, "PUÑO TURQUESA", "GIRO PIXELADO", "SUPREMO BLOQUE", 11, 12, SP_RUSH, 14, 12, SU_BEAM, 32, { 65, 225, 210 } },
};

/* movimientos: tiempos en fotogramas a 60 fps, como en la version web */
typedef struct { int pose, st, ac, rc; float dmg, b0, b1; int push, stun, kd; float vx; int both, multi; } Move;
enum { MV_PUNCH, MV_KICK, MV_LOW, MV_AIR, MV_AIRP, MV_SP1, MV_SP2, MV_SUPER, NMV };
static const Move BASE[NMV] = {
    { P_PUNCH, 4, 3, 8, 6, 0.45f, 0.95f, 5, 13, 0, 0, 0, 0 },
    { P_KICK, 6, 4, 11, 9, 0.30f, 0.95f, 8, 16, 0, 0, 0, 0 },
    { P_LOW, 5, 4, 11, 7, 0.00f, 0.40f, 5, 14, 0, 0, 0, 0 },
    { P_AIR, 2, 16, 0, 8, -0.1f, 0.80f, 6, 16, 0, 0, 0, 0 },
    { P_PUNCH, 2, 12, 0, 7, 0.00f, 0.75f, 5, 14, 0, 0, 0, 0 },
    { P_SP1, 12, 1, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { P_SP2, 10, 26, 16, 0, 0.1f, 0.9f, 8, 18, 1, 0, 0, 0 },
    { P_SUPER, 0, 1, 20, 0, 0, 0, 0, 0, 1, 0, 0, 0 },
};

/* ------------------------------------------------------------------ estado del juego */
enum { S_IDLE, S_ATK, S_HIT, S_BLOCK, S_FALL, S_DOWN, S_GETUP, S_WIN, S_DEAD };
typedef struct { int l, r, u, d, punch, kick, sp1, sp2, sup, guard; } Inp;
typedef struct {
    const Def *d; int ci; float x, y, vx, vy; int dir, st, t, stun, grounded, crouch, guard;
    float hp, maxhp, meter; int wins, mv, mt, hitdone, combo, flash, invul, cd1, walkt, cpu;
    Move m; int aiT, aiMode; float hw, hh; int spawned;
} F;
typedef struct { int on; float x, y, vx, dmg; F *owner; int ci, t; } Proj;
typedef struct { int on, t; float x, y; u8 c[3]; } Spark;
typedef struct { int on, t, type; float x; int dir; u8 c[3]; } SupFx;

static F P[2]; static Proj PR[8]; static Spark SPK[24]; static SupFx SFX;
static float CAM, STAGEW; static u8 *STAGEBUF; static const u8 *BG; static int BGW;
static unsigned RNG = 12345;
static int rnd(int n) { RNG ^= RNG << 13; RNG ^= RNG >> 17; RNG ^= RNG << 5; return (int)((RNG >> 8) % (unsigned)n); }
static float fclampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

static const Spr *spr(const F *f, int pose) { const Spr *s = &SPR[f->ci][pose]; return s->px ? s : &SPR[f->ci][P_IDLE]; }
static float reach(const F *f, int pose) { const Spr *s = spr(f, pose); float r = (float)(s->w - s->ax); return r < 24 ? 24 : r; }
static void spark(float x, float y, int r, int g, int b) {
    for (int i = 0; i < 24; i++) if (!SPK[i].on) { SPK[i] = (Spark){ 1, 0, x, y, { (u8)r, (u8)g, (u8)b } }; return; }
}
static void load_stage(int ci) {                          /* un unico escenario para todos los combates */
    (void)ci; if (STAGEBUF) return; STAGEBUF = load_file("stage.bin", NULL);
    if (STAGEBUF) { BGW = STAGEBUF[0] | (STAGEBUF[1] << 8); BG = STAGEBUF + 4; STAGEW = (float)BGW; }
}
static void fighter_reset(F *f, int ci, float x, int dir, int keepwins) {
    int w = keepwins ? f->wins : 0, cpu = f->cpu; memset(f, 0, sizeof *f);
    f->d = &DEFS[ci]; f->ci = ci; f->x = x; f->dir = dir; f->grounded = 1; f->mv = -1; f->maxhp = f->hp = (float)f->d->hp; f->wins = w; f->cpu = cpu;
    f->hw = 0.17f * f->d->height / 3.f; f->hh = 0.85f * f->d->height / 3.f; f->invul = 0;
}

/* aplica un golpe de 'a' sobre 'dd'. 0 = nada, 1 = entra, 2 = bloqueado */
static int strike(F *a, F *dd, float dmg, int push, int stun, int kd, int special, float hx, float hy) {
    if (dd->invul > 0 || dd->st == S_FALL || dd->st == S_DOWN || dd->st == S_GETUP || dd->st == S_DEAD || dd->st == S_WIN) return 0;
    dd->dir = a->x >= dd->x ? 1 : -1;
    if (dd->guard && dd->grounded && (dd->st == S_IDLE || dd->st == S_BLOCK)) {
        dd->hp -= special ? dmg * 0.18f : 0; if (dd->hp < 1) dd->hp = 1; dd->st = S_BLOCK; dd->stun = 10; dd->vx = -dd->dir * push * 0.27f; dd->t = 0;
        audio_play("sfx_block1"); spark(hx, hy, 160, 216, 255); dd->meter = fminf(100, dd->meter + 3); return 2;
    }
    int chained = (dd->st == S_HIT); int n = chained ? dd->combo + 1 : 1; dmg *= fmaxf(0.4f, 1.f - 0.1f * (n - 1));
    dd->hp -= dmg; dd->flash = 4; dd->meter = fminf(100, dd->meter + dmg * 0.45f); a->meter = fminf(100, a->meter + dmg * 0.7f);
    int ko = dd->hp <= 0; if (ko) dd->hp = 0;
    if (ko || kd || !dd->grounded) { dd->st = S_FALL; dd->vy = ko ? -5.f : -4.f; dd->y += 1; dd->grounded = 0; dd->vx = -dd->dir * (ko ? 3.0f : 2.2f); }
    else { dd->st = S_HIT; dd->stun = stun ? stun : 16; dd->vx = -dd->dir * push * 0.27f; }
    dd->t = 0; dd->combo = n; dd->mv = -1;
    static unsigned sound_take;
    if (ko) audio_voice("ko");
    if (a->ci == 5 && special) audio_play("alesonic_hit");
    else if (special) audio_play("sfx_heavy1");
    else if (a->ci == 5 && (a->mv == MV_KICK || a->mv == MV_LOW || a->mv == MV_AIR)) audio_play("alesonic_kick");
    else if (a->ci == 5) audio_play("alesonic_punch");
    else if (a->mv == MV_KICK || a->mv == MV_LOW || a->mv == MV_AIR) audio_play((sound_take++ & 1) ? "sfx_kick1" : "sfx_kick2");
    else { unsigned take = sound_take++ % 3; audio_play(take == 0 ? "sfx_punch1" : (take == 1 ? "sfx_punch2" : "sfx_punch3")); }
    if (ko || kd) audio_play("sfx_fall1");
    spark(hx, hy, 255, 240, 120); return 1;
}

static void start_move(F *f, int mv) {
    if (f->ci == 5) {
        if (mv == MV_SUPER) audio_play("alesonic_super");
        else if (mv == MV_SP1 || mv == MV_SP2) audio_play("alesonic_special");
        else if (mv == MV_KICK || mv == MV_LOW || mv == MV_AIR) audio_play("alesonic_kick");
        else audio_play("alesonic_punch");
    } else {
        if (mv == MV_SUPER) audio_play("sfx_heavy2");
        else if (mv == MV_SP1) audio_play("sfx_whoosh1");
        else if (mv == MV_SP2) audio_play("sfx_whoosh2");
    }
    f->mv = mv; f->mt = 0; f->hitdone = 0; f->st = S_ATK; f->t = 0; f->m = BASE[mv]; f->spawned = 0;
    if (mv == MV_SP2) { f->m.dmg = f->d->sp2dmg * f->d->power; if (f->d->sp2t == SP_SPIN) { f->m.st = 6; f->m.ac = 42; f->m.rc = 14; f->m.both = 1; f->m.multi = 10; f->m.kd = 0; f->m.stun = 14; f->m.push = 4; f->m.b0 = 0; f->m.b1 = 0.9f; } }
    else if (mv != MV_SP1 && mv != MV_SUPER) f->m.dmg *= f->d->power;
}
static int pose_of(const F *f) {
    switch (f->st) {
    case S_ATK: if (f->mv == MV_SP2 && f->ci == 0 && SPR[f->ci][P_FA5].px) return P_FA5; return f->m.pose;
    case S_HIT: return P_HIT; case S_BLOCK: return SPR[f->ci][P_BLOCK].px ? P_BLOCK : P_CROUCH;
    case S_FALL: return SPR[f->ci][P_FALL].px ? P_FALL : P_HIT; case S_DOWN: case S_DEAD: return P_KO;
    case S_GETUP: return SPR[f->ci][P_GETUP].px ? P_GETUP : P_CROUCH; case S_WIN: return P_WIN;
    default: break;
    }
    if (!f->grounded) return P_JUMP;
    if (f->guard && SPR[f->ci][P_BLOCK].px) return P_BLOCK;
    if (f->crouch) return P_CROUCH;
    if (fabsf(f->vx) > 0.3f) { int k = (f->walkt / 6) & 3; if (SPR[f->ci][P_W1 + k].px) return P_W1 + k; return P_WALK; }
    if (((f->t / 30) & 1) && SPR[f->ci][P_IDLE2].px) return P_IDLE2;
    return P_IDLE;
}

/* supers: tras la congelacion se aplica el efecto */
static struct { int t; F *f; } FREEZE;
static void apply_super(F *a, F *b) {
    const Def *d = a->d; SFX = (SupFx){ 1, 0, d->supt, a->x, a->dir, { d->fx[0], d->fx[1], d->fx[2] } };
    float dx = fabsf(b->x - a->x); int inrange = d->supt == SU_NOVA ? dx < 150 : (d->supt == SU_BEAM ? (b->x - a->x) * a->dir > -20 : 1);
    if (inrange) strike(a, b, d->supdmg * d->power, 6, 30, 1, 1, b->x, GROUND - b->hh * 0.6f);
}

static void update_fighter(F *a, F *b, const Inp *in) {
    const Def *d = a->d; float walk = d->speed * 1.6f / 3.f;
    if (a->flash > 0) a->flash--; if (a->invul > 0) a->invul--; if (a->cd1 > 0) a->cd1--;
    int back = (a->dir == 1 && in->l) || (a->dir == -1 && in->r);
    a->guard = a->grounded && (a->st == S_IDLE || a->st == S_BLOCK) && (in->guard || back);
    a->t++;
    switch (a->st) {
    case S_IDLE: {
        if (!a->grounded) a->crouch = 0;
        if (a->grounded) {
            a->crouch = in->d; float mv = 0; if (!a->crouch) { if (in->l) mv -= 1; if (in->r) mv += 1; }
            a->vx = mv * walk * (a->guard ? 0.5f : 1.f); if (mv != 0) a->walkt++;
            a->dir = b->x >= a->x ? 1 : -1;
            if (in->u && !a->crouch) { a->vy = JUMPV; a->grounded = 0; a->vx = mv * walk; }
        }
        int act = -1;
        if (in->sup && a->meter >= 100 && !FREEZE.t) act = MV_SUPER;
        else if (in->sp2) act = MV_SP2; else if (in->sp1 && a->cd1 == 0) act = MV_SP1;
        else if (in->kick) act = !a->grounded ? MV_AIR : (a->crouch ? MV_LOW : MV_KICK);
        else if (in->punch) act = !a->grounded ? MV_AIRP : MV_PUNCH;
        if (act == MV_SUPER) { a->meter = 0; start_move(a, MV_SUPER); FREEZE.t = 55; FREEZE.f = a; a->invul = 60; }
        else if (act >= 0) { start_move(a, act); if (act == MV_SP1) a->cd1 = 40; if (a->grounded && act != MV_SP2) a->vx = 0; }
    } break;
    case S_ATK: {
        Move *m = &a->m; a->mt++;
        if (a->grounded && a->mv != MV_SP2) a->vx *= 0.8f;
        if (a->mv == MV_SP2 && a->mt > m->st && a->mt <= m->st + m->ac) a->vx = a->dir * d->sp2vx / 3.f * (d->sp2t == SP_SPIN ? 1.4f : 1.f);
        else if (a->mv == MV_SP2 && a->mt > m->st + m->ac) a->vx *= 0.7f;
        if (a->mv == MV_SP1 && !a->spawned && a->mt > m->st) {
            a->spawned = 1; for (int i = 0; i < 8; i++) if (!PR[i].on) {
                PR[i] = (Proj){ 1, a->x + a->dir * reach(a, P_SP1) * 0.7f, a->y + d->height * 0.58f / 3.f, a->dir * d->sp1spd * 1.35f / 3.f, d->sp1dmg * d->power, a, a->ci, 0 }; break; }
        }
        if (a->mv == MV_SUPER) { if (!FREEZE.t && a->mt > 40) { a->st = S_IDLE; a->mv = -1; } break; }
        if (m->multi && a->mt > m->st && (a->mt - m->st) % m->multi == 0) a->hitdone = 0;
        /* caja de golpe activa */
        if (a->mv != MV_SP1 && !a->hitdone && a->mt > m->st && a->mt <= m->st + m->ac) {
            float r = reach(a, pose_of(a)), x0 = m->both ? a->x - r : (a->dir == 1 ? a->x - 4 : a->x - r), x1 = m->both ? a->x + r : (a->dir == 1 ? a->x + r : a->x + 4);
            float hh = d->height / 3.f, y0 = a->y + hh * m->b0, y1 = a->y + hh * m->b1;     /* alturas sobre el suelo */
            float dy0 = b->y, dy1 = b->y + (b->crouch ? b->hh * 0.65f : b->hh);
            if (x1 > b->x - b->hw && x0 < b->x + b->hw && y1 > dy0 && y0 < dy1) {
                int rs = strike(a, b, m->dmg, m->push, m->stun, m->kd, a->mv == MV_SP2, (a->x + b->x) / 2, GROUND - (y0 + y1) / 2);
                if (rs) a->hitdone = 1;
            }
        }
        int total = m->st + m->ac + m->rc;
        if (a->mv == MV_AIR || a->mv == MV_AIRP) { if (a->grounded) { a->st = S_IDLE; a->mv = -1; } }
        else if (a->mt >= total) { a->st = S_IDLE; a->mv = -1; }
    } break;
    case S_HIT: case S_BLOCK: a->vx *= 0.85f; if (--a->stun <= 0) { a->st = S_IDLE; a->combo = 0; } break;
    case S_FALL: if (a->grounded) { a->st = a->hp <= 0 ? S_DEAD : S_DOWN; a->t = 0; a->vx = 0; } break;
    case S_DOWN: a->vx = 0; if (a->t > 36) { a->st = S_GETUP; a->t = 0; a->combo = 0; } break;
    case S_GETUP: if (a->t > 14) { a->st = S_IDLE; a->invul = 30; } break;
    default: a->vx = 0; break;
    }
    /* fisica */
    a->x += a->vx; if (!a->grounded || a->y > 0) { a->vy += GRAV; a->y -= a->vy; if (a->y <= 0 && a->vy > 0) { a->y = 0; a->vy = 0; a->grounded = 1; if (a->st == S_IDLE) a->vx = 0; } }
    a->x = fclampf(a->x, 14, STAGEW - 14);
}

static void update_proj(void) {
    for (int i = 0; i < 8; i++) if (PR[i].on) {
        Proj *p = &PR[i]; p->x += p->vx; p->t++; F *t = p->owner == &P[0] ? &P[1] : &P[0];
        if (p->x < -20 || p->x > STAGEW + 20) { p->on = 0; continue; }
        float hy = t->y + (t->crouch ? t->hh * 0.65f : t->hh);
        if (fabsf(p->x - t->x) < t->hw + 12 && p->y > t->y && p->y < hy + 4) { if (strike(p->owner, t, p->dmg, 6, 16, 0, 1, p->x, GROUND - p->y)) p->on = 0; }
    }
}

/* ------------------------------------------------------------------ IA */
static void ai(F *me, F *op, Inp *o) {
    memset(o, 0, sizeof *o); float dx = op->x - me->x, dist = fabsf(dx); int fwd = dx > 0 ? 1 : -1;
    if (--me->aiT <= 0) { me->aiMode = rnd(6); me->aiT = 12 + rnd(28); }
    float rc = reach(me, P_PUNCH) + 10;
    if (op->st == S_ATK && dist < 110 && rnd(100) < 40) o->guard = 1;
    if (me->meter >= 100 && dist < 200 && rnd(100) < 4) { o->sup = 1; return; }
    switch (me->aiMode) {
    case 0: case 1: case 2:
        if (dist > rc) { if (fwd > 0) o->r = 1; else o->l = 1; }
        else { int r = rnd(100); if (r < 30) o->punch = 1; else if (r < 55) o->kick = 1; else if (r < 65) { o->d = 1; o->kick = 1; } }
        break;
    case 3: if (dist < 140) { if (fwd > 0) o->l = 1; else o->r = 1; } break;
    case 4: if (me->grounded && rnd(100) < 8) { o->u = 1; if (fwd > 0) o->r = 1; else o->l = 1; } break;
    case 5: if (dist > 120 && rnd(100) < 7) o->sp1 = 1; else if (dist > 60 && dist < 220 && rnd(100) < 5) o->sp2 = 1; break;
    }
}

/* ------------------------------------------------------------------ flujo de juego */
enum { G_TITLE, G_SELECT, G_INTRO, G_FIGHT, G_ROUNDEND, G_MATCHEND, G_PAUSE };
static int G = G_TITLE, GT, ROUND, TIMER, SEL, LEVEL_IDX, PLAYER_CI, OPP_LIST[5], OPP_N, WINNER = -1, CHAMPION, AUTOPLAY, TICK;


static void new_round(void) {
    fighter_reset(&P[0], P[0].ci, 150, 1, 1); fighter_reset(&P[1], P[1].ci, 280, -1, 1);
    memset(PR, 0, sizeof PR); memset(SPK, 0, sizeof SPK); SFX.on = 0; FREEZE.t = 0; TIMER = 99 * 60; G = G_INTRO; GT = 0;
}
static void start_match(int pci) {                       /* arcade: te enfrentas a los otros 4 */
    { char voice[48]; snprintf(voice, sizeof voice, "nombre_%s", DEFS[pci].id); audio_voice(voice); }
    PLAYER_CI = pci; OPP_N = 0; for (int i = 0; i < NCH; i++) if (i != pci) OPP_LIST[OPP_N++] = i;
    for (int i = OPP_N - 1; i > 0; i--) { int j = rnd(i + 1), t = OPP_LIST[i]; OPP_LIST[i] = OPP_LIST[j]; OPP_LIST[j] = t; }
    LEVEL_IDX = 0; CHAMPION = 0;
}
static void start_fight(void) {
    audio_voice("pelear");
    P[0].cpu = AUTOPLAY; P[1].cpu = 1; P[0].wins = P[1].wins = 0; P[0].ci = PLAYER_CI; P[1].ci = OPP_LIST[LEVEL_IDX]; ROUND = 1;
    load_stage(P[1].ci); new_round();
}

static void read_player(Inp *i) {
    memset(i, 0, sizeof *i); i->l = !!(K_HELD & B_LEFT); i->r = !!(K_HELD & B_RIGHT); i->u = !!(K_HELD & B_UP); i->d = !!(K_HELD & B_DOWN);
    i->punch = !!(K_DOWN & B_A); i->kick = !!(K_DOWN & B_B); i->sp1 = !!(K_DOWN & B_X); i->sp2 = !!(K_DOWN & B_Y); i->sup = !!(K_DOWN & B_R); i->guard = !!(K_HELD & B_L);
}

static void update_fight(void) {
    Inp in[2];
    if (P[0].cpu) ai(&P[0], &P[1], &in[0]); else read_player(&in[0]);
    ai(&P[1], &P[0], &in[1]);
    if (G == G_INTRO) { memset(in, 0, sizeof in); }
    if (G == G_ROUNDEND) { memset(in, 0, sizeof in); }
    if (FREEZE.t) {                                         /* congelacion de super: solo cuenta atras */
        if (--FREEZE.t == 0) apply_super(FREEZE.f, FREEZE.f == &P[0] ? &P[1] : &P[0]);
        return;
    }
    if (SFX.on && ++SFX.t > 40) SFX.on = 0;
    update_fighter(&P[0], &P[1], &in[0]); update_fighter(&P[1], &P[0], &in[1]); update_proj();
    float dx = P[1].x - P[0].x; if (fabsf(dx) < 22 && P[0].grounded && P[1].grounded) { float s = (22 - fabsf(dx)) / 2 * (dx >= 0 ? 1 : -1); P[0].x -= s; P[1].x += s; }
    for (int i = 0; i < 24; i++) if (SPK[i].on && ++SPK[i].t > 12) SPK[i].on = 0;
    float mid = (P[0].x + P[1].x) / 2; CAM = fclampf(mid - SW / 2, 0, STAGEW - SW);
}

static void next_step(void) {
    if (G == G_INTRO) { if (++GT > 110) { G = G_FIGHT; GT = 0; } }
    else if (G == G_FIGHT) {
        int w = -1;
        if (P[0].st == S_DEAD) w = 1; else if (P[1].st == S_DEAD) w = 0;
        else if (--TIMER <= 0) w = (P[0].hp / P[0].maxhp >= P[1].hp / P[1].maxhp) ? 0 : 1;
        if (w >= 0) { WINNER = w; P[w].wins++; G = G_ROUNDEND; GT = 0; }
    } else if (G == G_ROUNDEND) {
        GT++; if (GT == 40) { F *w = &P[WINNER]; if (w->st != S_FALL && w->st != S_DEAD) { w->st = S_WIN; w->t = 0; } }
        if (GT > 150) { if (P[WINNER].wins >= 2) { G = G_MATCHEND; GT = 0; if (WINNER == 0) audio_voice("victoria"); } else { ROUND++; new_round(); } }
    }
}

/* ------------------------------------------------------------------ render */
static void draw_bg(u8 *fb) {
    if (!BG) { rect(fb, SW, 0, 0, SW, SH, 40, 40, 60); return; }
    int c = (int)CAM; if (c < 0) c = 0; if (c > BGW - SW) c = BGW - SW; memcpy(fb, BG + (size_t)c * SH * 3, (size_t)SW * SH * 3);
}
static void draw_fighter(u8 *fb, const F *f) {
    int pose = pose_of(f); const Spr *s = spr(f, pose); int sx = (int)(f->x - CAM), sy = GROUND - (int)f->y;
    /* sombra */
    int sw = (int)(f->hw * 1.6f) - (int)(f->y * 0.1f); if (sw < 10) sw = 10;
    for (int i = -sw; i <= sw; i++) { int hgt = (int)(4.f * sqrtf(1.f - (float)(i * i) / (sw * sw))); rect_a(fb, SW, sx + i, GROUND - hgt + 1, 1, hgt * 2 - 1, 0, 0, 0, 110); }
    int x0 = f->dir == 1 ? sx - s->ax : sx - (s->w - s->ax);
    blit(fb, s, x0, sy - s->h, f->dir == -1, f->flash > 0 && f->flash % 2);
    if (f->invul > 0 && (f->invul & 2) && f->st == S_GETUP) {}
}
static void draw_proj(u8 *fb) {
    for (int i = 0; i < 8; i++) if (PR[i].on) { const Spr *s = &SPR[PR[i].ci][P_PROJ]; if (s->px) blit(fb, s, (int)(PR[i].x - CAM) - s->w / 2, GROUND - (int)PR[i].y - s->h / 2, PR[i].vx < 0, 0); else rect(fb, SW, (int)(PR[i].x - CAM) - 6, GROUND - (int)PR[i].y - 6, 12, 12, 255, 255, 255); }
    for (int i = 0; i < 24; i++) if (SPK[i].on) {
        int x = (int)(SPK[i].x - CAM), y = (int)SPK[i].y, r = 3 + SPK[i].t; int a = 256 - SPK[i].t * 20; if (a < 0) a = 0;
        rect_a(fb, SW, x - r, y - 1, r * 2, 3, SPK[i].c[0], SPK[i].c[1], SPK[i].c[2], a); rect_a(fb, SW, x - 1, y - r, 3, r * 2, SPK[i].c[0], SPK[i].c[1], SPK[i].c[2], a);
        rect_a(fb, SW, x - r / 2, y - r / 2, r, r, 255, 255, 255, a / 2);
    }
}
static void draw_superfx(u8 *fb) {
    if (!SFX.on) return; float k = SFX.t / 40.f; int a = (int)(220 * (1 - k)); int cx = (int)(SFX.x - CAM);
    switch (SFX.type) {
    case SU_NOVA: { int r = (int)(20 + k * 160); rect_a(fb, SW, cx - r, GROUND - 90, r * 2, 90, SFX.c[0], SFX.c[1], SFX.c[2], a / 2); rect_a(fb, SW, cx - r, GROUND - 6, r * 2, 6, 255, 255, 255, a); } break;
    case SU_BEAM: { int h = (int)(40 * sinf(k * 3.1416f)) + 6; if (SFX.dir > 0) rect_a(fb, SW, cx, GROUND - 90 - h / 2, SW, h, SFX.c[0], SFX.c[1], SFX.c[2], a); else rect_a(fb, SW, 0, GROUND - 90 - h / 2, cx, h, SFX.c[0], SFX.c[1], SFX.c[2], a); } break;
    case SU_RAIN: for (int i = 0; i < 14; i++) { int x = (i * 37 + (int)(SFX.t * 3 + i * 11)) % SW, y = (int)(k * (GROUND + 30)) - (i % 5) * 28; rect_a(fb, SW, x, y, 8, 14, SFX.c[0], SFX.c[1], SFX.c[2], a + 30); } break;
    case SU_STAMP: { int y = (int)(k < 0.5f ? (k * 2) * GROUND : GROUND); rect_a(fb, SW, cx + SFX.dir * 40 - 30, y - 50, 60, 50, SFX.c[0], SFX.c[1], SFX.c[2], a); } break;
    }
}
static void draw_bar(u8 *fb, int x, int y, int w, int h, float frac, int left, const u8 *c, float lag) {
    rect(fb, SW, x - 1, y - 1, w + 2, h + 2, 0, 0, 0); rect(fb, SW, x, y, w, h, 60, 20, 20);
    int fw = (int)(w * fclampf(frac, 0, 1)), lw = (int)(w * fclampf(lag, 0, 1)); if (lw > fw) { if (left) rect(fb, SW, x, y, lw, h, 255, 220, 120); else rect(fb, SW, x + w - lw, y, lw, h, 255, 220, 120); }
    if (left) rect(fb, SW, x, y, fw, h, c[0], c[1], c[2]); else rect(fb, SW, x + w - fw, y, fw, h, c[0], c[1], c[2]);
}
static float LAG[2];
static void draw_hud(u8 *fb) {
    for (int i = 0; i < 2; i++) {
        F *f = &P[i]; int left = i == 0; float fr = f->hp / f->maxhp; if (LAG[i] > fr) LAG[i] -= 0.006f; else LAG[i] = fr;
        const Spr *fc = &SPR[f->ci][P_FACE]; blit(fb, fc, left ? 4 : SW - 4 - fc->w, 4, !left, 0);
        int bx = left ? 40 : 218, bw = 142; draw_bar(fb, bx, 10, bw, 12, fr, left, f->d->col, LAG[i]);
        text_o(fb, SW, left ? 40 : SW - 40 - tw(f->d->name, 1), 25, f->d->name, 1, 255, 255, 255);
        for (int k = 0; k < 2; k++) rect(fb, SW, left ? 40 + k * 12 : SW - 40 - 10 - k * 12, 38, 10, 5, 0, 0, 0), rect(fb, SW, (left ? 41 + k * 12 : SW - 39 - 10 - k * 12) , 39, 8, 3, f->wins > k ? 255 : 70, f->wins > k ? 210 : 70, f->wins > k ? 40 : 70);
        int full = f->meter >= 100; int mw = 110, mx = left ? 8 : SW - 8 - mw; rect(fb, SW, mx - 1, SH - 15, mw + 2, 8, 0, 0, 0);
        int fw = (int)(mw * f->meter / 100); if (left) rect(fb, SW, mx, SH - 14, fw, 6, full && (TICK / 6 & 1) ? 255 : 47, full && (TICK / 6 & 1) ? 255 : 123, 255); else rect(fb, SW, mx + mw - fw, SH - 14, fw, 6, full && (TICK / 6 & 1) ? 255 : 47, full && (TICK / 6 & 1) ? 255 : 123, 255);
    }
    char t[16]; snprintf(t, sizeof t, "%02d", TIMER / 60 < 0 ? 0 : TIMER / 60); text_c(fb, SW, 8, t, 3, 255, 255, 255);
}
static void dim(u8 *fb, int w) { rect_a(fb, w, 0, 0, w, SH, 0, 0, 20, 150); }

static void draw_bottom(void) {
    u8 *fb = FB_BOT; rect(fb, BW, 0, 0, BW, SH, 24, 22, 52);
    if (G == G_SELECT) {
        const Def *d = &DEFS[SEL]; text_c(fb, BW, 8, d->name, 2, 255, 220, 80); text_c(fb, BW, 30, d->title, 1, 200, 200, 255);
        char b[64]; text(fb, BW, 14, 56, "ESPECIAL 1 (X):", 1, 255, 255, 255); text(fb, BW, 24, 70, d->sp1n, 1, 160, 255, 160);
        text(fb, BW, 14, 90, "ESPECIAL 2 (Y):", 1, 255, 255, 255); text(fb, BW, 24, 104, d->sp2n, 1, 160, 255, 160);
        text(fb, BW, 14, 124, "SUPER (R) CON BARRA LLENA:", 1, 255, 255, 255); text(fb, BW, 24, 138, d->supn, 1, 255, 160, 120);
        snprintf(b, sizeof b, "VIDA %d  VELOCIDAD %.1f  FUERZA %.2f", d->hp, d->speed, d->power); text(fb, BW, 14, 164, b, 1, 200, 200, 200);
        text_c(fb, BW, 200, "IZQ/DER: ELEGIR   A: LUCHAR", 1, 255, 255, 255);
    } else {
        text_c(fb, BW, 10, "CONTROLES", 2, 255, 220, 80);
        const char *l[] = { "PAD / STICK : MOVER", "ARRIBA : SALTAR   ABAJO : AGACHARSE", "A : PUNO     B : PATADA", "ABAJO + B : PATADA BAJA", "X : ESPECIAL 1 (PROYECTIL)", "Y : ESPECIAL 2", "R : SUPER (BARRA LLENA)", "L (o atras) : BLOQUEAR", "START : PAUSA" };
        for (int i = 0; i < 9; i++) text(fb, BW, 16, 40 + i * 16, l[i], 1, 230, 230, 255);
    }
}

static void draw_title(u8 *fb) {
    draw_bg(fb); dim(fb, SW);
    int xs[NCH] = { 35, 101, 167, 233, 299, 365 };
    for (int i = 0; i < NCH; i++) { const Spr *s = &SPR[i][((TICK / 40 + i) & 1) ? P_IDLE2 : P_IDLE]; if (!s->px) s = &SPR[i][P_IDLE]; blit(fb, s, xs[i] - s->ax, GROUND + 6 - s->h, 0, 0); }
    text_c(fb, SW, 22, "MONCLOA'S", 4, 255, 200, 40); text_c(fb, SW, 66, "FIGHTER", 4, 255, 90, 40); text_c(fb, SW, 112, "3DS", 2, 255, 255, 255);
    if ((TICK / 30) & 1) text_c(fb, SW, 150, "PULSA START", 2, 255, 255, 255);
    text_c(fb, SW, SH - 14, "PARODIA SATIRICA", 1, 200, 200, 200);
}
static void draw_select(u8 *fb) {
    draw_bg(fb); dim(fb, SW); text_c(fb, SW, 6, "ELIGE LUCHADOR", 2, 255, 255, 255);
    for (int i = 0; i < NCH; i++) {
        int cx = 4 + i * 66, cy = 38;
        rect(fb, SW, cx - 2, cy - 2, 62, 62, i == SEL ? 255 : 90, i == SEL ? 210 : 90, i == SEL ? 40 : 110);
        rect(fb, SW, cx, cy, 58, 58, DEFS[i].col[0] / 3, DEFS[i].col[1] / 3, DEFS[i].col[2] / 3);
        const Spr *f = &SPR[i][P_FACE];
        if (f->px) {
            int ox = cx + (58 - f->w) / 2, oy = cy + (58 - f->h) / 2;
            for (int xx = 0; xx < f->w; xx++) for (int yy = 0; yy < f->h; yy++) {
                u32 q = f->px[xx * f->h + yy];
                if (q >> 24) rect(fb, SW, ox + xx, oy + (f->h - 1 - yy), 1, 1,
                                  (q >> 16) & 255, (q >> 8) & 255, q & 255);
            }
        }
    }
    const Spr *s = &SPR[SEL][((TICK / 40) & 1) && SPR[SEL][P_IDLE2].px ? P_IDLE2 : P_IDLE]; blit(fb, s, 70 - s->ax, SH - 6 - s->h, 0, 0);
    int tx = 142 + (SW - 142 - tw(DEFS[SEL].name, 2)) / 2; text_o(fb, SW, tx, 126, DEFS[SEL].name, 2, 255, 220, 80);
    text_o(fb, SW, 142 + (SW - 142 - tw(DEFS[SEL].title, 1)) / 2, 150, DEFS[SEL].title, 1, 200, 200, 255);
    { char b[48]; snprintf(b, sizeof b, "VIDA %d  VEL %.1f  FUERZA %.2f", DEFS[SEL].hp, DEFS[SEL].speed, DEFS[SEL].power); text_o(fb, SW, 142 + (SW - 142 - tw(b, 1)) / 2, 170, b, 1, 230, 230, 230); }
    text_c(fb, SW, SH - 18, "A: LUCHAR      B: ATRAS", 1, 255, 255, 255);
}
static void draw_fight(u8 *fb) {
    draw_bg(fb);
    F *o0 = &P[0], *o1 = &P[1]; if (o0->y > o1->y) { F *t = o0; o0 = o1; o1 = t; }
    draw_fighter(fb, o0); draw_fighter(fb, o1); draw_proj(fb); draw_superfx(fb); draw_hud(fb);
    if (FREEZE.t) { rect_a(fb, SW, 0, 70, SW, 60, 0, 0, 0, 170); text_c(fb, SW, 80, FREEZE.f->d->supn, 2, FREEZE.f->d->fx[0], FREEZE.f->d->fx[1], FREEZE.f->d->fx[2]); text_c(fb, SW, 106, FREEZE.f->d->name, 1, 255, 255, 255); }
    char b[32];
    if (G == G_INTRO) { if (GT < 60) { snprintf(b, sizeof b, "ROUND %d", ROUND); text_c(fb, SW, 90, b, 4, 255, 255, 255); } else text_c(fb, SW, 90, "LUCHA!", 4, 255, 90, 40); }
    if (G == G_ROUNDEND) { if (P[0].hp <= 0 || P[1].hp <= 0) text_c(fb, SW, 90, "K.O.", 5, 255, 60, 40); else text_c(fb, SW, 90, "TIEMPO", 4, 255, 255, 255); }
    if (G == G_MATCHEND) { snprintf(b, sizeof b, "GANA %s", P[WINNER].d->name); dim(fb, SW); text_c(fb, SW, 80, b, 2, 255, 220, 80); text_c(fb, SW, 120, WINNER == 0 ? (LEVEL_IDX + 1 >= OPP_N ? "CAMPEON!" : "A: SIGUIENTE") : "A: REINTENTAR", 2, 255, 255, 255); }
    if (G == G_PAUSE) { dim(fb, SW); text_c(fb, SW, 90, "PAUSA", 4, 255, 255, 255); text_c(fb, SW, 140, "START: SEGUIR   SELECT: SALIR", 1, 255, 255, 255); }
}

/* Renderiza cada ojo con una pequeña separación horizontal. El deslizador físico
   controla la profundidad; la simulación y la pantalla inferior se actualizan una sola vez. */
#ifdef __3DS__
static void draw_top_stereo(void) {
    float base_cam = CAM;
    float slider = osGet3DSliderState();
    float eye_sep = slider * 5.0f; /* hasta 5 px: profundidad moderada y cómoda */
    CAM = base_cam - eye_sep;
    if (G == G_TITLE) draw_title(FB_TOP);
    else if (G == G_SELECT) draw_select(FB_TOP);
    else draw_fight(FB_TOP);
    /* draw_hud actualiza la barra de vida; restauramos su estado para el segundo ojo. */
    float left_lag0 = LAG[0], left_lag1 = LAG[1];
    CAM = base_cam + eye_sep;
    if (G == G_TITLE) draw_title(FB_TOP_RIGHT);
    else if (G == G_SELECT) draw_select(FB_TOP_RIGHT);
    else draw_fight(FB_TOP_RIGHT);
    LAG[0] = left_lag0; LAG[1] = left_lag1;
    CAM = base_cam;
}
#endif

/* ------------------------------------------------------------------ principal */
static int g_main_loop(void) {
    if (G == G_TITLE) { if (K_DOWN & B_START || K_DOWN & B_A) { G = G_SELECT; GT = 0; audio_voice("elige"); load_stage(SEL); } }
    else if (G == G_SELECT) {
        if (K_DOWN & B_RIGHT) SEL = (SEL + 1) % NCH; if (K_DOWN & B_LEFT) SEL = (SEL + NCH - 1) % NCH;
        if (K_DOWN & B_A) { start_match(SEL); start_fight(); }
        if (K_DOWN & B_B) G = G_TITLE;
    } else if (G == G_PAUSE) {
        if (K_DOWN & B_START) G = G_FIGHT; if (K_DOWN & B_SELECT) { G = G_SELECT; load_stage(SEL); }
    } else if (G == G_MATCHEND) {
        if (K_DOWN & B_A) { if (WINNER == 0) { LEVEL_IDX++; if (LEVEL_IDX >= OPP_N) { G = G_SELECT; load_stage(SEL); return 1; } } start_fight(); }
        if (K_DOWN & B_SELECT) { G = G_SELECT; load_stage(SEL); }
    } else {
        if (G == G_FIGHT && (K_DOWN & B_START) && !P[0].cpu) { G = G_PAUSE; return 1; }
        update_fight(); next_step();
    }
    return 1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    plat_init();
#ifdef __3DS__
    audio_init(); audio_music("musica_menu"); audio_voice("titulo");
#endif
    { long n; u8 *f = load_file("font.bin", &n); if (!f || n < (long)sizeof FONT) { plat_quit(); return 1; } memcpy(FONT, f, sizeof FONT); free(f); }
    if (!load_sprites()) { plat_quit(); return 1; }
    load_stage(0);
#ifdef HOST
    extern int host_main_hook(void); return host_main_hook();
#else
    { int last_music_state = -1;
    while (plat_running()) {
        plat_begin(); TICK++;
        { int music_state = (G == G_TITLE || G == G_SELECT) ? 0 : 1;
          if (music_state != last_music_state) {
              audio_music(music_state == 0 ? "musica_menu" : "musica_cibeles");
              last_music_state = music_state;
          }
        }
        if ((K_HELD & (B_START | B_SELECT | B_L | B_R)) == (B_START | B_SELECT | B_L | B_R)) break;
        g_main_loop();
        if (G == G_TITLE || G == G_SELECT) CAM = 0;
#ifdef __3DS__
        draw_top_stereo();
#else
        if (G == G_TITLE) draw_title(FB_TOP);
        else if (G == G_SELECT) draw_select(FB_TOP);
        else draw_fight(FB_TOP);
#endif
        { static int lastkey = -1, dirty;                  /* pantalla inferior: solo se redibuja al cambiar (2 veces: doble buffer) */
          int key = G == G_SELECT ? 10 + SEL : 1; if (key != lastkey) { lastkey = key; dirty = 2; }
          if (dirty > 0) { draw_bottom(); dirty--; } }
        plat_end();
    }
    }
    plat_quit(); return 0;
#endif
}
