// El generador original (AcidPattern.hpp del ATEK303 de VCV) imprime patrones para muchas semillas,
// ajustes y mutaciones. pattern_port.c imprime lo mismo con el port: las dos salidas deben ser iguales.
#include "AcidPattern.hpp"
#include <cstdio>
static void dump(const AcidPatternV4& p) {
    printf("L%d P%d T", p.timeLength, p.pitchLength);
    for (int i = 0; i < p.timeLength; i++) printf("%d", (int)p.time[i]);
    printf(" |");
    for (int i = 0; i < p.pitchLength; i++)
        printf(" %d/%d%s%s", p.pitch[i].degree, p.pitch[i].octave, p.pitch[i].accent ? "a" : "", p.pitch[i].slideOut ? "s" : "");
    printf("\n");
}
int main() {
    const float SETS[6][5] = { // density, accent, slide, range, tie
        {0.65f,0.6f,0.5f,0.45f,0.4f}, {0.05f,0.f,0.f,0.f,0.f}, {1.f,1.f,1.f,1.f,1.f},
        {0.3f,0.8f,0.2f,0.9f,0.4f}, {0.9f,0.3f,0.9f,0.2f,0.7f}, {0.5f,0.5f,0.5f,0.5f,0.1f}};
    AcidDualGenerator g;
    for (int set = 0; set < 6; set++)
    for (int scale = 0; scale < ACID_SCALES_LEN; scale++)
    for (int steps : {64, 32, 16, 7})
    for (uint32_t k = 0; k < 40; k++) {
        uint32_t seed = k * 2654435761u + 12345u * (set + 1) + scale;
        AcidGenParams p; p.steps = steps; p.scale = scale;
        p.density = SETS[set][0]; p.accent = SETS[set][1]; p.slide = SETS[set][2]; p.range = SETS[set][3]; p.tie = SETS[set][4];
        AcidPatternV4 pat; int c = g.generate(pat, p, seed);
        printf("s%u set%d sc%d st%d c%d ", seed, set, scale, steps, c); dump(pat);
        uint32_t cursor = 0;
        for (int m = 0; m < 6; m++) {
            AcidPatternMutator::Layer layer = (AcidPatternMutator::Layer)(1 + m % 3);
            uint32_t last = cursor;
            bool ok = AcidPatternMutator::mutateBurst(pat, layer, cursor + 1, layer == AcidPatternMutator::Articulation ? 3 : 2, scale, last);
            if (ok) cursor = last;
            printf("  m%d %d ", m, ok ? 1 : 0); dump(pat);
        }
        AcidGen grid; pat.render(grid);
        printf("  R");
        for (int i = 0; i < 16; i++) printf(" %d:%d:%d%d%d%d", grid.deg[i], grid.oct[i], grid.gate[i], grid.accent[i], grid.slide[i], grid.tie[i]);
        printf("\n");
    }
}
// (importRendered: render -> import -> render must give the same grid; checked in pattern_import.c)
