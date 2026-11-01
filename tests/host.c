/* Arnes de pruebas para PC: usa el MISMO main.c (juego + render) con framebuffers en memoria */
#define HOST 1
#include "../source/main.c"
#include "../source/audio.c"
static unsigned script(int f) {
    if (f >= 3 && f < 6) return B_START;                 /* titulo -> seleccion */
    if (f >= 20 && f < 22) return B_RIGHT;               /* elegir Feijoo */
    if (f >= 30 && f < 33) return B_A;                   /* luchar */
    return 0;
}
extern int host_main_hook(void);
int host_main_hook(void) {
    host_script = script; AUTOPLAY = 1;
    int shots[] = { 4, 25, 100, 200, 500, 900, 1300, 1700, 2100, -1 }; int si = 0; char nm[64];
    for (int fr = 0; fr < 4000 && plat_running(); fr++) {
        plat_begin(); TICK++; g_main_loop();
        if (fr == 800) P[0].meter = 100;                                           /* forzar un super para probar */
        if (G == G_TITLE) { CAM = 0; draw_title(FB_TOP); } else if (G == G_SELECT) { CAM = 0; draw_select(FB_TOP); } else draw_fight(FB_TOP);
        draw_bottom();
        if (shots[si] >= 0 && fr == shots[si]) { snprintf(nm, sizeof nm, "shots/t%04d.ppm", fr); dump_ppm(FB_TOP, SW, nm); if (fr == 25 || fr == 500) { snprintf(nm, sizeof nm, "shots/b%04d.ppm", fr); dump_ppm(FB_BOT, BW, nm); } si++; }
        { static int d1, d2, d3; if (FREEZE.t == 30 && !d1) { dump_ppm(FB_TOP, SW, "shots/s_freeze.ppm"); d1 = 1; }
          if (SFX.on && SFX.t == 14 && !d2) { dump_ppm(FB_TOP, SW, "shots/s_fx.ppm"); d2 = 1; }
          if (G == G_MATCHEND && GT == 20 && !d3) { dump_ppm(FB_TOP, SW, "shots/s_end.ppm"); d3 = 1; } }
        plat_end();
        if (fr % 300 == 0) printf("frame %d G=%d hp %.0f/%.0f round %d wins %d-%d\n", fr, G, P[0].hp, P[1].hp, ROUND, P[0].wins, P[1].wins);
    }
    return 0;
}
