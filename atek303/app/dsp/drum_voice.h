/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * ATEK303's drum track for the M-VAVE FM-1: eight analogue-modelled percussion voices in the
 * TR-808 / TR-909 / TR-707 spirit, in single-precision C with no libm (fastmath.h, as the 303's
 * atek_voice.c). All synthesis, no samples: the firmware stays small and every voice follows its
 * LEVEL knob live; TUNE and DECAY apply at the next hit.
 *
 * The voices, and the circuit each one is modelled on:
 *
 *   KICK    a sine with an exponential pitch drop (the bridged-T resonant circuit's ringing) and
 *           an amplitude decay, plus a short noise click on top (the 909's attack transient).
 *           The kit sets how fast the pitch falls and how loud the click: 808 long and round,
 *           909 fast and punchy, 707 in between.
 *   SNARE   two detuned body sines with their own fast decay, plus white noise through a
 *           band-pass and a high-pass (the 808's 180/330 Hz tank + noise; the 909 brighter and
 *           noisier, the 707 cleaner).
 *   CLAP    noise through a band-pass, struck four times: three pre-echoes 11 ms apart and the
 *           main tail (the 808's four envelopes on one noise generator).
 *   HAT C   on the 808 kit, six detuned square oscillators (its metallic ring: 263, 400, 421,
 *           474, 587 and 845 Hz, doubled) through a band-pass and a high-pass; on the 909 and
 *           707, filtered noise, brighter and darker. Short decay.
 *   HAT O   the same generator with a long decay. A closed hat chokes it (the machines' shared
 *           envelope), always: the classic pedal behaviour, no option.
 *   TOM L   a sine with a pitch drop and a soft second partial (the 808's conga line, tuned
 *           low; the 707's tom sweeps more).
 *   TOM H   the same, tuned high.
 *   COWB    two detuned squares (the 808 cowbell's 540/800 Hz) through a band-pass: a fast drop
 *           and a short tail. On the 707 kit this part plays a rimshot instead (a narrow burst
 *           of square and noise, band-passed at 1.7 kHz).
 *
 * Every voice runs only while it rings; an envelope under its silence floor stops it. One
 * trigger() per hit, with the accent as gain. Threads: dr_render() and dr_trigger() run in the
 * audio interrupt; dr_set(), dr_kit(), dr_accamt() between blocks. */
#pragma once
#include <stdint.h>

/* the eight parts, the matrix editor's rows in this order */
enum { DR_KICK, DR_SNARE, DR_CLAP, DR_CHH, DR_OHH, DR_TOML, DR_TOMH, DR_COWB, DR_NPARTS };

/* the kits: the flavour of every voice at once */
enum { DR_KIT_808 = 0, DR_KIT_909 = 1, DR_KIT_707 = 2 };
#define DR_NKITS 3

/* the three knobs per part, 0..1 */
enum { DR_LEVEL, DR_TUNE, DR_DECAY, DR_NPAR };

typedef struct {
    int on;                               /* it rings */
    int kit;                              /* the machine's kit at the last hit */
    float t;                              /* seconds since the hit */
    float env, env_c;                     /* the main amplitude envelope and its per-sample factor */
    float env2, env2_c;                   /* the second envelope (bodies, tails) */
    float hit_acc;                        /* the accent's gain at the hit (level applies live) */
    float ph1, ph2, ph3;                  /* the oscillator phases */
    float w1, w2, w3;                     /* their frequencies, radians a sample */
    float pc, pc_c;                       /* the pitch drop: 1 at the hit, decaying */
    float click, click_c;                 /* the kick's click envelope */
    int bursts;                           /* the clap's pre-echoes left */
    float next_burst;                     /* when the next one lands */
    int tail;                             /* the clap's tail armed */
    float hph[6];                         /* the 808 hat's six square phases */
    float hp_b0, hp_b1, hp_a1, hp_x1, hp_y1;                              /* one-pole high-pass */
    float b0, b1, b2, a1, a2, x1, x2, y1, y2;                             /* band-pass biquad */
    float level, tune, decay;             /* the part's knobs, 0..1 */
    uint32_t seed;                        /* the voice's own noise stream */
} dr_part_t;

typedef struct {
    float sr;
    int kit;
    float acc_amt;                        /* how much an accent opens (0..1) */
    dr_part_t p[DR_NPARTS];
    uint32_t rng;                         /* the shared noise stream (the kick's click, the 909 hats) */
} dr_machine_t;

void dr_init(dr_machine_t *d, float sr);
void dr_set(dr_machine_t *d, int part, int param, float value);   /* 0..1, between blocks */
void dr_kit(dr_machine_t *d, int kit);                            /* 0..2, between blocks */
void dr_accamt(dr_machine_t *d, float value);                     /* 0..1, between blocks */
void dr_trigger(dr_machine_t *d, int part, int accent);           /* a hit; render or between blocks */
void dr_panic(dr_machine_t *d);                                   /* every voice silent */
/* n mono samples, about -1..1: all the voices that ring, each with its LEVEL */
void dr_render(dr_machine_t *d, float *out, uint32_t n);

extern const char *const DR_PART_NAME[DR_NPARTS];      /* KICK, SNARE, CLAP, ... (short, for the screen) */
extern const char *const DR_KIT_NAME[DR_NKITS];        /* 808, 909, 707 */
/* the cowbell plays a rimshot on the 707 kit: the name follows */
const char *dr_part_label(int part, int kit);
/* the swing of an offbeat step, seconds: sw 0..1 (the knob's 50..75 %) over a sixteenth */
float dr_swing_seconds(float sw, float period);
