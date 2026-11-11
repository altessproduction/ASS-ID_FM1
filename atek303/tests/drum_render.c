/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * The drum engine on its own, on the computer: it renders every voice of every kit to a WAV and
 * checks the numbers a synth must hold: no NaN, no runaway, every voice ends (a one-shot dies),
 * the accent is louder, the swing seats the offbeat on the triplet, and the same seed gives the
 * same pattern.
 *
 *   gcc -std=c99 -O2 -ffp-contract=off -I../app/dsp -I../base/include -o build/drum_render drum_render.c ../app/dsp/drum_voice.c ../app/dsp/drum_pattern.c -lm
 *   build/drum_render build
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "drum_voice.h"
#include "drum_pattern.h"

#define SR 44100.0f

static void wav(const char *path, const float *s, long n)
{
    FILE *f = fopen(path, "wb");
    unsigned b = n * 4, r = 36 + b, sr = (unsigned)SR, br = (unsigned)SR * 4, fs = 16;
    unsigned short fmt = 3, ch = 1, ba = 4, bits = 32;
    fwrite("RIFF", 1, 4, f); fwrite(&r, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fs, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&b, 4, 1, f); fwrite(s, 4, n, f); fclose(f);
}

static int fails;

static void check(const char *what, int ok)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        fails++;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build";
#define BUF_N (64L * 35280)                    /* 64 steps' worth at 125 BPM, wide */
    static dr_machine_t m;
    static drum_pattern_t pat, pat2;
    static float buf[BUF_N];
    int kit, part;
    char path[256];

    /* every voice of every kit: it rings, it is sane, it dies (with a short decay, in a window
     * four times the longest tail it can have) */
    for (kit = 0; kit < DR_NKITS; kit++) {
        for (part = 0; part < DR_NPARTS; part++) {
            long n = (long)(SR * 4.0f), i, voiced = 0;
            float peak = 0.0f, acc_peak = 0.0f;
            dr_init(&m, SR);
            dr_kit(&m, kit);
            dr_set(&m, part, DR_LEVEL, 0.9f);
            dr_set(&m, part, DR_TUNE, 0.5f);
            dr_set(&m, part, DR_DECAY, 0.2f);
            dr_trigger(&m, part, 0);
            dr_render(&m, buf, (uint32_t)n);
            for (i = 0; i < n; i++) {
                float a = fabsf(buf[i]);
                if (!(buf[i] > -8.0f && buf[i] < 8.0f)) {
                    check("no NaN or runaway", 0);
                    goto next;
                }
                if (a > peak)
                    peak = a;
                if (a > 0.001f)
                    voiced = i;                    /* the last sample above the floor */
            }
            check("it sounds", peak > 0.05f);
            check("it dies (a one-shot)", voiced < n - (long)(SR * 1.0f));
            /* the accent: louder, always */
            dr_init(&m, SR);
            dr_kit(&m, kit);
            dr_set(&m, part, DR_LEVEL, 0.9f);
            dr_trigger(&m, part, 1);
            dr_render(&m, buf, (uint32_t)n);
            for (i = 0; i < n; i++) {
                float a = fabsf(buf[i]);
                if (a > acc_peak)
                    acc_peak = a;
            }
            check("the accent is louder", acc_peak > peak * 1.2f);
        next:;
        }
    }
    if (!fails)
        check("every voice of every kit", 1);

    /* the swing: 75 % seats the offbeat a third of a sixteenth late */
    {
        float period = 0.12f;
        check("swing 50 % is straight", dr_swing_seconds(0.0f, period) == 0.0f);
        check("swing 75 % is the triplet seat", fabsf(dr_swing_seconds(1.0f, period) - period / 3.0f) < 1e-6f);
    }

    /* the pattern: the same seed, the same groove; the kick's first step when there is density */
    {
        drum_params_t p;
        int s, diffs = 0, kick0 = 0, gates = 0;
        p.steps = 16;
        p.density = 0.8f;
        p.accent = 0.5f;
        p.fill = 0.3f;
        drum_generate(&pat, &p, 0xC0FFEEu);
        drum_generate(&pat2, &p, 0xC0FFEEu);
        for (s = 0; s < DR_MAX_STEPS; s++) {
            int part2;
            for (part2 = 0; part2 < DR_NPARTS; part2++) {
                if (drum_gate(&pat, part2, s) != drum_gate(&pat2, part2, s)
                    || drum_acc(&pat, part2, s) != drum_acc(&pat2, part2, s))
                    diffs++;
                gates += drum_gate(&pat, part2, s);
            }
            kick0 += drum_gate(&pat, DR_KICK, s) && (s % 16) == 0;
        }
        check("the same seed, the same pattern", diffs == 0);
        check("the generator plants a groove", gates > 8);
        check("the kick lands on the one", kick0 == 1);   /* a 16-step pattern: one downbeat */
        drum_sanitize(&pat, 8);
        gates = 0;
        for (s = 8; s < DR_MAX_STEPS; s++) {
            int part2;
            for (part2 = 0; part2 < DR_NPARTS; part2++)
                gates += drum_gate(&pat, part2, s) + drum_acc(&pat, part2, s);
        }
        check("sanitize clears past the length", gates == 0);
    }

    /* one WAV per kit, all eight voices playing the pattern, for the ear */
    for (kit = 0; kit < DR_NKITS; kit++) {
        drum_params_t p;
        long i, n = (long)(SR * 4.0f);
        int step = -1;
        float step_time = 0.0f;
        p.steps = 16;
        p.density = 0.8f;
        p.accent = 0.55f;
        p.fill = 0.35f;
        drum_generate(&pat, &p, 0x44523031u);
        dr_init(&m, SR);
        dr_kit(&m, kit);
        for (part = 0; part < DR_NPARTS; part++) {
            dr_set(&m, part, DR_LEVEL, 0.9f);
            dr_set(&m, part, DR_DECAY, 0.5f);
        }
        for (i = 0; i < n; i++) {
            float dt = 1.0f / SR;
            int s;
            step_time += dt;
            if (step_time >= 0.12f) {          /* 125 BPM */
                step_time -= 0.12f;
                step = (step + 1) & 15;
                for (part = 0; part < DR_NPARTS; part++)
                    if (drum_gate(&pat, part, step))
                        dr_trigger(&m, part, drum_acc(&pat, part, step));
            }
            dr_render(&m, &buf[i], 1);
        }
        snprintf(path, sizeof path, "%s/drums_kit%d.wav", out, kit);
        wav(path, buf, n);
    }
    printf(fails ? "drum_render: %d FAIL\n" : "drum_render: all good\n", fails);
    return fails != 0;
}
