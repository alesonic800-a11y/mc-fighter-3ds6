#define HOST 1
#include "../source/main.c"
#include "../source/audio.c"
#include <time.h>
static unsigned script(int f) { (void)f; return (G == G_MATCHEND && (host_frame & 8)) ? B_A : 0; }
extern int host_main_hook(void);
int host_main_hook(void) {
    host_script = script; AUTOPLAY = 1; long frames = 0; struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int a = 0; a < NCH; a++) for (int b = 0; b < NCH; b++) {
        if (a == b) continue; PLAYER_CI = a; OPP_N = 1; OPP_LIST[0] = b; LEVEL_IDX = 0; start_fight(); int kos = 0, supers = 0;
        for (int fr = 0; fr < 2500; fr++, frames++) {
            plat_begin(); TICK++; g_main_loop();
            if (G == G_FIGHT && rnd(400) == 0) P[rnd(2)].meter = 100;
            if (FREEZE.t == 20) supers++;
            if (G == G_ROUNDEND && GT == 1) kos++;
            draw_fight(FB_TOP); draw_bottom(); plat_end();
            if (G == G_MATCHEND && GT == 0) GT = 1;
            if (G == G_MATCHEND) { start_fight(); }
        }
        printf("%s vs %s: rondas %d, supers %d\n", DEFS[a].id, DEFS[b].id, kos, supers);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1); double s = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("TOTAL %ld frames en %.2fs = %.3f ms/frame (con ASAN si se compilo asi)\n", frames, s, s * 1000 / frames); return 0;
}
