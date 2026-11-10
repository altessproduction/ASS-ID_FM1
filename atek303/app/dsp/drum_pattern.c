/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * The drum pattern's generator and mutations. See drum_pattern.h. The chance streams are a
 * xorshift32 like the acid generator's ownRandom: a seed grows the same pattern every time, on
 * the FM-1 as in the tests. */
#include "drum_pattern.h"
#include "fastmath.h"

static inline uint32_t rs_next(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 1u;
    return *s;
}
static inline float rs_unit(uint32_t *s)     /* uniform 0..1 */
{
    return (float)(rs_next(s) >> 8) * (1.0f / 16777216.0f);
}
static inline float clamp01(float x)
{
    return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}

int drum_gate(const drum_pattern_t *p, int part, int step)
{
    if (part < 0 || part >= DR_NPARTS || step < 0 || step >= DR_MAX_STEPS)
        return 0;
    return p->gate[part * DR_STEP_BYTES + (step >> 3)] >> (step & 7) & 1;
}
int drum_acc(const drum_pattern_t *p, int part, int step)
{
    if (part < 0 || part >= DR_NPARTS || step < 0 || step >= DR_MAX_STEPS)
        return 0;
    return p->acc[part * DR_STEP_BYTES + (step >> 3)] >> (step & 7) & 1;
}
void drum_set_gate(drum_pattern_t *p, int part, int step, int on)
{
    if (part < 0 || part >= DR_NPARTS || step < 0 || step >= DR_MAX_STEPS)
        return;
    if (on)
        p->gate[part * DR_STEP_BYTES + (step >> 3)] |= (uint8_t)(1u << (step & 7));
    else
        p->gate[part * DR_STEP_BYTES + (step >> 3)] &= (uint8_t)~(1u << (step & 7));
}
void drum_set_acc(drum_pattern_t *p, int part, int step, int on)
{
    if (part < 0 || part >= DR_NPARTS || step < 0 || step >= DR_MAX_STEPS)
        return;
    if (on)
        p->acc[part * DR_STEP_BYTES + (step >> 3)] |= (uint8_t)(1u << (step & 7));
    else
        p->acc[part * DR_STEP_BYTES + (step >> 3)] &= (uint8_t)~(1u << (step & 7));
}

void drum_clear(drum_pattern_t *p)
{
    int i;
    for (i = 0; i < DR_NPARTS * DR_STEP_BYTES; i++) {
        p->gate[i] = 0;
        p->acc[i] = 0;
    }
}

void drum_sanitize(drum_pattern_t *p, int steps)
{
    int part, s;
    if (steps < 1)
        steps = 1;
    if (steps > DR_MAX_STEPS)
        steps = DR_MAX_STEPS;
    for (part = 0; part < DR_NPARTS; part++)
        for (s = steps; s < DR_MAX_STEPS; s++) {
            drum_set_gate(p, part, s, 0);
            drum_set_acc(p, part, s, 0);
        }
}

/* ------------------------------------------------------------- generation --- */
void drum_generate(drum_pattern_t *p, const drum_params_t *prm, uint32_t seed)
{
    uint32_t rng = seed ? seed : 1u;
    int steps = prm->steps < 1 ? 1 : prm->steps > DR_MAX_STEPS ? DR_MAX_STEPS : prm->steps;
    float dens = clamp01(prm->density), acc = clamp01(prm->accent), fill = clamp01(prm->fill);
    int s;
    drum_clear(p);
    p->seed = rng;
    for (s = 0; s < steps; s++) {
        int b = s & 15;                       /* the step inside the bar */
        int kick, snare, clap, chh, ohh, toml, tomh, cowb;

        /* the kick: the first step when there is any kick at all, the quarters with the density,
         * and a stray one where the fill asks */
        if (b == 0)
            kick = dens > 0.04f;
        else if ((b & 3) == 0)
            kick = rs_unit(&rng) < clamp01(dens * 1.9f - 0.45f);
        else
            kick = rs_unit(&rng) < dens * 0.10f + fill * 0.14f * dens;
        if (kick)
            drum_set_gate(p, DR_KICK, s, 1);

        /* the snare: the backbeat with the density, ghosts with the fill */
        if (b == 4 || b == 12)
            snare = rs_unit(&rng) < clamp01(dens * 1.7f - 0.25f);
        else if ((b & 1) == 0)
            snare = rs_unit(&rng) < dens * fill * 0.30f;
        else
            snare = 0;
        if (snare)
            drum_set_gate(p, DR_SNARE, s, 1);

        /* the clap: on the backbeat beside the snare, or answering it two sixteenths later */
        if (b == 4 || b == 12)
            clap = rs_unit(&rng) < clamp01(dens * 1.4f - 0.55f);
        else if ((b & 3) == 2)
            clap = rs_unit(&rng) < fill * 0.18f;
        else
            clap = 0;
        if (clap)
            drum_set_gate(p, DR_CLAP, s, 1);

        /* the closed hats: the eighths, the sixteenths when dense */
        if ((b & 1) == 0)
            chh = rs_unit(&rng) < clamp01(dens * 2.2f - 0.30f);
        else
            chh = rs_unit(&rng) < clamp01((dens - 0.55f) * 1.8f);
        /* the open hat on the offbeat eighths, taking the step from the closed one */
        if ((b & 3) == 2)
            ohh = rs_unit(&rng) < clamp01(dens * 1.3f - 0.40f) * 0.9f;
        else
            ohh = 0;
        if (ohh)
            chh = 0;
        if (chh)
            drum_set_gate(p, DR_CHH, s, 1);
        if (ohh)
            drum_set_gate(p, DR_OHH, s, 1);

        /* the toms: sparse talk, and the fill in the last four steps */
        toml = ((b & 1) == 0) && rs_unit(&rng) < 0.04f + fill * 0.08f;
        tomh = ((b & 1) == 1) && rs_unit(&rng) < 0.03f + fill * 0.06f;
        if (s >= steps - 4 && steps >= 12) {
            if (rs_unit(&rng) < fill * 0.55f)
                tomh = 1;
            if (rs_unit(&rng) < fill * 0.30f)
                toml = 1;
        }
        if (toml)
            drum_set_gate(p, DR_TOML, s, 1);
        if (tomh)
            drum_set_gate(p, DR_TOMH, s, 1);

        /* the cowbell: sparse syncopation */
        cowb = (b == 3 || b == 7 || b == 11 || b == 14) && rs_unit(&rng) < 0.08f + fill * 0.30f;
        if (cowb)
            drum_set_gate(p, DR_COWB, s, 1);

        /* the accents: the kick's first step always, the rest with the knob (the hats a little
         * less, they cut through anyway) */
        {
            int part;
            for (part = 0; part < DR_NPARTS; part++) {
                if (!drum_gate(p, part, s))
                    continue;
                if (part == DR_KICK && b == 0) {
                    drum_set_acc(p, part, s, 1);
                    continue;
                }
                if (rs_unit(&rng) < (part == DR_CHH || part == DR_OHH ? acc * 0.7f : acc))
                    drum_set_acc(p, part, s, 1);
            }
        }
    }
}

/* ------------------------------------------------------------- mutations --- */
int drum_mutate(drum_pattern_t *p, int layer, uint32_t first_index, int operations, int steps,
                uint32_t *last, int lo, int hi)
{
    uint32_t rng = first_index * 2654435761u + 1u;
    int op, changed = 0;
    if (rng == 0u)
        rng = 1u;
    if (steps < 1)
        steps = 1;
    if (steps > DR_MAX_STEPS)
        steps = DR_MAX_STEPS;
    if (lo < 0)
        lo = 0;
    if (hi > steps)
        hi = steps;
    if (hi <= lo)
        return 0;
    for (op = 0; op < operations; op++) {
        int part = -1, step = lo + (int)(rs_next(&rng) % (uint32_t)(hi - lo));
        if (layer == DRUM_MUT_ACCENTS) {
            int tries;
            for (tries = 0; tries < 8 && part < 0; tries++) {
                int cand = (int)(rs_next(&rng) % (uint32_t)DR_NPARTS);
                if (drum_gate(p, cand, step))
                    part = cand;
            }
            if (part >= 0) {
                drum_set_acc(p, part, step, !drum_acc(p, part, step));
                changed = 1;
            }
        } else {
            static const uint8_t BACK[3] = {DR_KICK, DR_SNARE, DR_CLAP};
            static const uint8_t TOP[5] = {DR_CHH, DR_OHH, DR_TOML, DR_TOMH, DR_COWB};
            if (layer == DRUM_MUT_BACKBONE)
                part = BACK[rs_next(&rng) % 3u];
            else if (layer == DRUM_MUT_TOPS)
                part = TOP[rs_next(&rng) % 5u];
            else
                break;
            if (drum_gate(p, part, step) && rs_unit(&rng) < 0.55f)
                drum_set_gate(p, part, step, 0);   /* half the moves take a hit away */
            else
                drum_set_gate(p, part, step, 1);
            changed = 1;
        }
    }
    *last = first_index + (uint32_t)operations - 1u;   /* one index an operation, as the acid's burst does */
    return changed;
}
