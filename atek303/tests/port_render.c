/* El port en C, con la misma secuencia y la deriva apagada. */
#include <stdio.h>
#include <stdlib.h>
#include "atek_voice.h"
#include "secuencia.h"
static void wav(const char *path, const float *s, long n) {
    FILE *f = fopen(path, "wb"); unsigned b = n * 4, r = 36 + b, sr = SR, br = SR * 4, fs = 16;
    unsigned short fmt = 3, ch = 1, ba = 4, bits = 32;
    fwrite("RIFF",1,4,f); fwrite(&r,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fs,4,1,f); fwrite(&fmt,2,1,f);
    fwrite(&ch,2,1,f); fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&b,4,1,f); fwrite(s,4,n,f); fclose(f);
}
int main(int argc, char **argv) {
    static at_voice_t v; long i, total = (long)STEP_SAMPLES * 16 * BARS;
    float *out = malloc(sizeof(float) * total);
    at_init(&v, SR);
    v.drift_on = argc > 2 ? atoi(argv[2]) : 0;
    for (i = 0; i < total; i++) {   /* de muestra en muestra, como la referencia */
        int g, n, a, sl; cables(i, &g, &n, &a, &sl);
        at_gate(&v, g, n, a, sl);
        at_render(&v, &out[i], 1);
    }
    wav(argc > 1 ? argv[1] : "port.wav", out, total);
    return 0;
}
