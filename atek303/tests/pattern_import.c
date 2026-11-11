/* render -> import -> render da la misma rejilla (en los pasos que suenan), para muchas semillas. */
#include <stdio.h>
#include "acid_pattern.h"
int main(void) {
    int bad = 0, s, k;
    for (k = 0; k < 2000; k++) {
        acid_params_t p; acid_pattern_t a, b; acid_grid_t g1, g2;
        acid_params_default(&p); p.scale = k % ACID_SCALES_LEN; p.density = (float)(k % 10) / 10.0f + 0.05f;
        acid_generate(&a, &p, (uint32_t)k * 2654435761u + 7u);
        acid_render(&a, &g1); acid_import(&b, &g1, a.time_length, a.seed, 3); acid_sanitize(&b, p.scale); acid_render(&b, &g2);
        for (s = 0; s < 16; s++)
            if (g1.gate[s] != g2.gate[s] || g1.tie[s] != g2.tie[s] || (g1.gate[s] && (g1.deg[s] != g2.deg[s] || g1.oct[s] != g2.oct[s] || g1.accent[s] != g2.accent[s] || g1.slide[s] != g2.slide[s]))) { bad++; break; }
    }
    printf("%d de 2000 patrones cambian al ir y volver\n", bad);
    return bad != 0;
}
