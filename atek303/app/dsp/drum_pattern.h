/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * The drum track's pattern: eight parts of sixty-four steps, a gate and an accent bit each,
 * bit-packed (a part is eight bytes a layer). What the 303's acid_pattern is to its voice, this
 * is to the drum engine: a seed grows a pattern with the same flows of chance every time, the
 * character knobs regrow it from the same seed, and deterministic mutation bursts move it.
 *
 * The generator plants a groove, part by part, from the three knobs:
 *
 *   DENSITY  how much of the grid plays: the kick's four-on-the-floor, the snare's backbeat,
 *            the closed hats' eighths, and everything else, denser as it rises.
 *   ACCENTS  the chance that a hit lands accented (the kick's first step always is).
 *   FILL     ghost notes, offbeat claps, toms: the seasoning, and the last four steps' fill.
 *
 * The mutation layers follow the buttons the 303 mutates with: ENV moves the backbone (kick,
 * snare, clap), LFO the tops (hats, toms, cowbell), ARP the accents. */
#pragma once
#include <stdint.h>
#include "drum_voice.h"                       /* DR_NPARTS and the parts' order */

#define DR_MAX_STEPS 64                       /* as the 303: four pages of sixteen */
#define DR_STEP_BYTES 8                       /* sixty-four steps, a bit each */

typedef struct {
    uint32_t seed;
    uint8_t gate[DR_NPARTS * DR_STEP_BYTES];
    uint8_t acc[DR_NPARTS * DR_STEP_BYTES];
} drum_pattern_t;

typedef struct {
    int steps;                                /* 1..64 */
    float density, accent, fill;              /* 0..1 */
} drum_params_t;

enum { DRUM_MUT_BACKBONE = 1, DRUM_MUT_TOPS = 2, DRUM_MUT_ACCENTS = 3 };

int drum_gate(const drum_pattern_t *p, int part, int step);
int drum_acc(const drum_pattern_t *p, int part, int step);
void drum_set_gate(drum_pattern_t *p, int part, int step, int on);
void drum_set_acc(drum_pattern_t *p, int part, int step, int on);
void drum_clear(drum_pattern_t *p);
/* the gates beyond the length out (the length shrunk) */
void drum_sanitize(drum_pattern_t *p, int steps);
/* the same seed, the same pattern, every time */
void drum_generate(drum_pattern_t *p, const drum_params_t *prm, uint32_t seed);
/* a burst of deterministic mutations on one layer within the steps [lo, hi); *last takes the
 * mutation counter to continue from. 0 = nothing changed. */
int drum_mutate(drum_pattern_t *p, int layer, uint32_t first_index, int operations, int steps,
                uint32_t *last, int lo, int hi);
