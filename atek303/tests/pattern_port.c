/* El port en C: misma salida que pattern_golden.cpp. */
#include <stdio.h>
#include "acid_pattern.h"
static void dump(const acid_pattern_t *p) {
    int i;
    printf("L%d P%d T", p->time_length, p->pitch_length);
    for (i = 0; i < p->time_length; i++) printf("%d", (int)p->time[i]);
    printf(" |");
    for (i = 0; i < p->pitch_length; i++)
        printf(" %d/%d%s%s", p->pitch[i].degree, p->pitch[i].octave, p->pitch[i].accent ? "a" : "", p->pitch[i].slide_out ? "s" : "");
    printf("\n");
}
int main(void) {
    static const float SETS[6][5] = {
        {0.65f,0.6f,0.5f,0.45f,0.4f}, {0.05f,0.f,0.f,0.f,0.f}, {1.f,1.f,1.f,1.f,1.f},
        {0.3f,0.8f,0.2f,0.9f,0.4f}, {0.9f,0.3f,0.9f,0.2f,0.7f}, {0.5f,0.5f,0.5f,0.5f,0.1f}};
    int set, scale, steps, m, i; uint32_t k;
    for (set = 0; set < 6; set++)
    for (scale = 0; scale < ACID_SCALES_LEN; scale++)
    for (steps = 16; steps >= 7; steps -= 9)
    for (k = 0; k < 40; k++) {
        uint32_t seed = k * 2654435761u + 12345u * (uint32_t)(set + 1) + (uint32_t)scale, cursor = 0;
        acid_params_t p; acid_pattern_t pat; acid_grid_t g; int c;
        p.steps = steps; p.scale = scale;
        p.density = SETS[set][0]; p.accent = SETS[set][1]; p.slide = SETS[set][2]; p.range = SETS[set][3]; p.tie = SETS[set][4];
        c = acid_generate(&pat, &p, seed);
        printf("s%u set%d sc%d st%d c%d ", seed, set, scale, steps, c); dump(&pat);
        for (m = 0; m < 6; m++) {
            int layer = 1 + m % 3; uint32_t last = cursor;
            int ok = acid_mutate_burst(&pat, layer, cursor + 1, layer == ACID_MUT_ARTICULATION ? 3 : 2, scale, &last);
            if (ok) cursor = last;
            printf("  m%d %d ", m, ok ? 1 : 0); dump(&pat);
        }
        acid_render(&pat, &g);
        printf("  R");
        for (i = 0; i < 16; i++) printf(" %d:%d:%d%d%d%d", g.deg[i], g.oct[i], g.gate[i], g.accent[i], g.slide[i], g.tie[i]);
        printf("\n");
    }
    return 0;
}
