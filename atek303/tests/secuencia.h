/* La misma línea acid para las dos pruebas: 16 pasos a 125 BPM, gate al 50 %, con acentos y slides.
 * nota (MIDI, -1 silencio), acento, slide */
static const int SEQ[16][3] = {
    {36,1,0},{36,0,0},{48,0,1},{46,0,0}, {36,0,0},{-1,0,0},{39,1,0},{41,0,1},
    {43,0,0},{36,0,0},{48,1,1},{36,0,0}, {34,0,0},{36,1,0},{-1,0,0},{39,0,1},
};
#define SR 44100
#define STEP_SAMPLES ((int)(SR * 60.0 / 125.0 / 4.0))   /* una semicorchea */
#define BARS 4
/* el estado de los cuatro "cables" en la muestra i (gate, nota, acento, slide), como un secuenciador
 * de 303: el slide de un paso mantiene el gate hasta el siguiente */
static void cables(long i, int *gate, int *note, int *acc, int *slide)
{
    long step = i / STEP_SAMPLES;
    int k = (int)(step % 16), pos = (int)(i % STEP_SAMPLES);
    *note = SEQ[k][0] < 0 ? 36 : SEQ[k][0];
    *acc = SEQ[k][1];
    *slide = SEQ[k][2];
    *gate = SEQ[k][0] >= 0 && (pos < STEP_SAMPLES / 2 || SEQ[k][2]);
}
