/* Los efectos sobre la línea acid de prueba: un WAV estéreo por configuración, con nivel, pico y
 * comprobación de estabilidad (nada de NaN, la cola muere). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "atek_voice.h"
#include "fx.h"
#include "secuencia.h"
static void wav2(const char *path, const float *l, const float *r, long n) {
    FILE *f = fopen(path, "wb"); unsigned b = n * 8, rr = 36 + b, sr = SR, br = SR * 8, fs = 16;
    unsigned short fmt = 3, ch = 2, ba = 8, bits = 32; long i;
    fwrite("RIFF",1,4,f); fwrite(&rr,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fs,4,1,f); fwrite(&fmt,2,1,f);
    fwrite(&ch,2,1,f); fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&b,4,1,f);
    for (i = 0; i < n; i++) { fwrite(&l[i],4,1,f); fwrite(&r[i],4,1,f); } fclose(f);
}
int main(int argc, char **argv) {
    static at_voice_t v; long i, notes = (long)STEP_SAMPLES * 16 * 2, total = notes + SR * 6;
    float *m = calloc(total, 4), *l = calloc(total, 4), *r = calloc(total, 4), peak = 0, tail = 0, rms = 0;
    const char *name = argc > 1 ? argv[1] : "fx.wav"; int k, bad = 0;
    at_init(&v, SR);
    for (i = 0; i < total; i++) { int g, n, a, sl; cables(i, &g, &n, &a, &sl); if (i >= notes) g = 0; at_gate(&v, g, n, a, sl); at_render(&v, &m[i], 1); }
    fx_init(SR); fx_tempo(60.0f / 125.0f / 4.0f);
    for (k = 2; k + 1 < argc; k += 2) fx_set(atoi(argv[k]), (float)atof(argv[k + 1]));
    for (i = 0; i < total; i += 256) fx_process(&m[i], &l[i], &r[i], (unsigned)(total - i < 256 ? total - i : 256));
    for (i = 0; i < total; i++) {
        if (!(fabsf(l[i]) < 1e6f) || !(fabsf(r[i]) < 1e6f)) bad = 1;
        if (fabsf(l[i]) > peak) peak = fabsf(l[i]);
        rms += l[i] * l[i];
        if (i > total - SR / 2) tail = fmaxf(tail, fabsf(l[i]));
    }
    printf("%-14s pico %.2f  rms %.3f  cola(ultimo 0,5 s) %.5f  %s\n", name, peak, sqrtf(rms / total), tail, bad ? "NaN/EXPLOTA" : "ok");
    wav2(name, l, r, total);
    return bad;
}
