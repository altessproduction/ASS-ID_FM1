/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * The drum track's eight voices. See drum_voice.h for the circuit each one models. The numbers
 * come from the classic machines' recipes (the 808 hat's six square ratios, the clap's three
 * pre-echoes, the kick's resonant pitch drop), tuned by ear against them. Envelopes decay
 * geometrically (a per-sample factor), filters are an RBJ band-pass biquad and a one-pole
 * high-pass, their coefficients computed at the hit, never per sample. */
#include "drum_voice.h"
#include "fastmath.h"

#define TAU_FLOOR 0.0004f                    /* an envelope under this is done (-72 dB) */
#define CLICK_TAU 0.0022f                    /* the kick's click, about 2 ms */
#define ACCENT_BOOST 1.20f                   /* what a full accent adds to the gain */
#define CLAP_PRE 0.011f                      /* the clap's pre-echoes, 11 ms apart */
#define CLAP_PRE_TAU 0.0038f                 /* their short decay */
#define CLAP_PRE_N 3                         /* three of them */
#define CHOKED_TAU 0.025f                    /* a choked open hat dies in about 25 ms */

const char *const DR_PART_NAME[DR_NPARTS] = {"KICK", "SNARE", "CLAP", "HAT C", "HAT O", "TOM L", "TOM H", "COWBELL"};
const char *const DR_KIT_NAME[DR_NKITS] = {"808", "909", "707"};

const char *dr_part_label(int part, int kit)
{
    return part == DR_COWB && kit == DR_KIT_707 ? "RIMSHOT" : DR_PART_NAME[part];
}

float dr_swing_seconds(float sw, float period)
{
    if (sw < 0.0f)
        sw = 0.0f;
    if (sw > 1.0f)
        sw = 1.0f;
    return sw * (1.0f / 3.0f) * period;      /* 75 % seats the offbeat on the triplet */
}

/* ----------------------------------------------------------------- helpers --- */
static inline float noise_white(uint32_t *st)
{
    uint32_t x = *st;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
/* the envelope one step on; the voice ends when it falls under the floor */
static inline float env_run(float *env, float c, int *on)
{
    float e = *env * c;
    if (e < TAU_FLOOR) {
        e = 0.0f;
        *on = 0;
    }
    *env = e;
    return e;
}
static inline float env_step(float *env, float c)
{
    return *env *= c;
}
static void env_set(dr_part_t *v, float tau_s, float sr)
{
    if (tau_s < 0.001f)
        tau_s = 0.001f;
    v->env = 1.0f;
    v->env_c = fm_expf(-1.0f / (tau_s * sr));
}
/* a band-pass biquad and a one-pole high-pass, RBJ cookbook; at the hit, not per sample */
static void set_bp(dr_part_t *v, float freq, float q, float sr)
{
    float w = FM_TWO_PI * freq / sr, s = fm_sinf(w), c = fm_cosf(w);
    float alpha = s / (2.0f * q), scale = 1.0f / (1.0f + alpha);
    v->a1 = 2.0f * c * scale;
    v->a2 = (alpha - 1.0f) * scale;
    v->b0 = alpha * scale;
    v->b1 = 0.0f;
    v->b2 = -alpha * scale;
    v->x1 = v->x2 = v->y1 = v->y2 = 0.0f;
}
static inline float bq_run(dr_part_t *v, float x)
{
    float y = v->b0 * x + v->b1 * v->x1 + v->b2 * v->x2 + v->a1 * v->y1 + v->a2 * v->y2;
    v->x2 = v->x1;
    v->x1 = x;
    v->y2 = v->y1;
    v->y1 = y;
    return y;
}
static void set_hp(dr_part_t *v, float fc, float sr)
{
    float x = fm_expf(-FM_TWO_PI * fc / sr);
    v->hp_b0 = 0.5f * (1.0f + x);
    v->hp_b1 = -v->hp_b0;
    v->hp_a1 = x;
    v->hp_x1 = v->hp_y1 = 0.0f;
}
static inline float hp_run(dr_part_t *v, float x)
{
    float y = v->hp_b0 * x + v->hp_b1 * v->hp_x1 + v->hp_a1 * v->hp_y1;
    v->hp_x1 = x;
    v->hp_y1 = y;
    return y;
}
static inline float semitones(float x)       /* TUNE's centre detent: -12..+12 semitones */
{
    return fm_powf(2.0f, (x - 0.5f) * 2.0f);
}
static inline float sq(float ph)             /* a square's sign, the phase already wrapped */
{
    return ph < 3.14159265f ? 1.0f : -1.0f;
}

/* --------------------------------------------------------------- the voices --- */
static void trig_kick(dr_part_t *v, float acc, float sr)
{
    float fend = 47.0f * semitones(v->tune), fstart, ptau, atau, click;
    if (v->kit == DR_KIT_909) {
        fstart = fend * 5.2f;
        ptau = 0.008f;
        atau = 0.055f + 0.50f * v->decay;
        click = 0.85f;
    } else if (v->kit == DR_KIT_707) {
        fstart = fend * 3.4f;
        ptau = 0.014f;
        atau = 0.065f + 0.45f * v->decay;
        click = 0.45f;
    } else {                                  /* 808 */
        fstart = fend * 2.3f;
        ptau = 0.026f;
        atau = 0.10f + 0.60f * v->decay;
        click = 0.12f;
    }
    v->w1 = FM_TWO_PI * fstart / sr;          /* the pitch's start */
    v->w2 = FM_TWO_PI * fend / sr;            /* its seat */
    v->pc = 1.0f;
    v->pc_c = fm_expf(-1.0f / (ptau * sr));
    v->ph1 = v->ph2 = 0.0f;
    env_set(v, atau, sr);
    v->click = 1.0f;
    v->click_c = fm_expf(-1.0f / (CLICK_TAU * sr));
    v->hit_acc = acc;
    v->env2 = 0.0f;
}
static float rend_kick(dr_part_t *v)
{
    float s, click;
    v->ph1 += v->w2 + (v->w1 - v->w2) * v->pc;
    v->ph2 += v->w2;
    v->pc *= v->pc_c;
    s = fm_tanhf(fm_sinf(v->ph1) * 1.9f) * 0.85f + 0.15f * fm_sinf(v->ph2);
    click = noise_white(&v->seed) * v->click;
    click *= env_step(&v->click, v->click_c);
    return (s * env_run(&v->env, v->env_c, &v->on) + click) * v->hit_acc * v->level;
}

static void trig_snare(dr_part_t *v, float acc, float sr)
{
    float f1, f2, nbp, hpf, tgain, ntau, bt;
    if (v->kit == DR_KIT_909) {
        f1 = 168.0f;
        f2 = 331.0f;
        nbp = 3400.0f;
        hpf = 430.0f;
        tgain = 0.60f;
        ntau = 0.10f + 0.30f * v->decay;
        bt = 0.034f;
    } else if (v->kit == DR_KIT_707) {
        f1 = 190.0f;
        f2 = 376.0f;
        nbp = 2500.0f;
        hpf = 520.0f;
        tgain = 0.40f;
        ntau = 0.08f + 0.26f * v->decay;
        bt = 0.030f;
    } else {                                  /* 808 */
        f1 = 185.0f;
        f2 = 330.0f;
        nbp = 1800.0f;
        hpf = 340.0f;
        tgain = 0.85f;
        ntau = 0.09f + 0.34f * v->decay;
        bt = 0.040f;
    }
    f1 *= semitones(v->tune);
    f2 *= semitones(v->tune);
    v->w1 = FM_TWO_PI * f1 / sr;
    v->w2 = FM_TWO_PI * f2 / sr;
    v->ph1 = v->ph2 = 0.0f;
    set_bp(v, nbp, 0.8f, sr);
    set_hp(v, hpf, sr);
    env_set(v, ntau, sr);
    v->env2 = 1.0f;
    v->env2_c = fm_expf(-1.0f / (bt * sr));
    v->hit_acc = acc;
    v->pc = tgain;                            /* the tone's gain rides in the pitch-drop spare */
}
static float rend_snare(dr_part_t *v)
{
    float n = noise_white(&v->seed);
    float body = fm_sinf(v->ph1) + 0.6f * fm_sinf(v->ph2);
    v->ph1 += v->w1;
    v->ph2 += v->w2;
    n = hp_run(v, bq_run(v, n));
    body *= env_step(&v->env2, v->env2_c);
    return (body * v->pc + n * env_run(&v->env, v->env_c, &v->on) * 1.5f) * v->hit_acc * v->level * 0.70f;
}

static void trig_clap(dr_part_t *v, float acc, float sr)
{
    set_bp(v, 1080.0f * semitones(v->tune), 1.7f, sr);
    set_hp(v, 380.0f, sr);
    v->t = 0.0f;
    v->bursts = CLAP_PRE_N;
    v->next_burst = CLAP_PRE;
    v->tail = 0;
    v->env = 1.0f;
    v->env_c = fm_expf(-1.0f / (CLAP_PRE_TAU * sr));
    v->env2_c = fm_expf(-1.0f / ((0.09f + 0.30f * v->decay) * sr));
    v->hit_acc = acc;
}
static float rend_clap(dr_part_t *v, float sr)
{
    float n = hp_run(v, bq_run(v, noise_white(&v->seed)));
    v->t += 1.0f / sr;
    env_step(&v->env, v->env_c);
    if (v->bursts && v->t >= v->next_burst) { /* a pre-echo lands: the envelope jumps back up */
        v->bursts--;
        v->next_burst += CLAP_PRE;
        v->env = 1.0f;
    }
    if (!v->tail && v->t >= CLAP_PRE * (float)(CLAP_PRE_N + 1)) {
        v->tail = 1;                          /* the main tail takes over from the last echo */
        v->env_c = v->env2_c;
        if (v->env < 0.8f)
            v->env = 0.8f;
    }
    return n * env_run(&v->env, v->env_c, &v->on) * v->hit_acc * v->level * 2.6f;
}

/* the hats: the 808's six squares, or filtered noise */
static void trig_hat(dr_part_t *v, float acc, float sr, int open)
{
    float tau = open ? 0.16f + 0.55f * v->decay : 0.028f + 0.09f * v->decay;
    int i;
    if (v->kit == DR_KIT_909)
        tau *= 0.72f;
    else if (v->kit == DR_KIT_707)
        tau *= 0.85f;
    for (i = 0; i < 6; i++)
        v->hph[i] = 0.0f;
    set_bp(v, 10000.0f, 0.55f, sr);
    set_hp(v, v->kit == DR_KIT_808 ? 7800.0f : v->kit == DR_KIT_909 ? 8600.0f : 6900.0f, sr);
    env_set(v, tau, sr);
    v->hit_acc = acc;
    v->pc = open ? 1.05f : 1.0f;              /* the part's raw gain rides in the pitch-drop spare */
}
static float rend_hat(dr_part_t *v, float sr)
{
    float s;
    if (v->kit == DR_KIT_808) {
        static const float FR[6] = {263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f};
        float tune = semitones(v->tune);
        int i;
        s = 0.0f;
        for (i = 0; i < 6; i++) {
            v->hph[i] += FM_TWO_PI * FR[i] * 2.0f * tune / sr;
            if (v->hph[i] >= FM_TWO_PI)
                v->hph[i] -= FM_TWO_PI;
            s += sq(v->hph[i]);
        }
        s *= 0.30f;
    } else {
        s = noise_white(&v->seed) * 0.9f;
        if (v->kit == DR_KIT_707)
            s = s * 0.7f + 0.3f * noise_white(&v->seed);
    }
    return hp_run(v, bq_run(v, s)) * env_run(&v->env, v->env_c, &v->on) * v->hit_acc * v->level * v->pc;
}

static void trig_tom(dr_part_t *v, float acc, float sr, float base)
{
    float f = base * semitones(v->tune);
    float sweep = v->kit == DR_KIT_707 ? 2.1f : v->kit == DR_KIT_909 ? 1.55f : 1.35f;
    v->w1 = FM_TWO_PI * f * sweep / sr;
    v->w2 = FM_TWO_PI * f / sr;
    v->pc = 1.0f;
    v->pc_c = fm_expf(-1.0f / ((v->kit == DR_KIT_707 ? 0.05f : 0.028f) * sr));
    v->ph1 = v->ph2 = 0.0f;
    env_set(v, 0.10f + 0.45f * v->decay, sr);
    v->hit_acc = acc;
}
static float rend_tom(dr_part_t *v)
{
    float s;
    v->ph1 += v->w2 + (v->w1 - v->w2) * v->pc;
    v->ph2 += v->w2 * 2.0f;
    v->pc *= v->pc_c;
    s = fm_sinf(v->ph1) + 0.22f * fm_sinf(v->ph2);
    return fm_tanhf(s * 1.3f) * env_run(&v->env, v->env_c, &v->on) * v->hit_acc * v->level;
}

static void trig_cowb(dr_part_t *v, float acc, float sr)
{
    float f1;
    if (v->kit == DR_KIT_707) {               /* the rimshot: a square and a noise burst, band-passed */
        f1 = 485.0f * semitones(v->tune);
        set_bp(v, 1750.0f, 1.1f, sr);
        env_set(v, 0.030f + 0.10f * v->decay, sr);
        v->env2 = 1.0f;
        v->env2_c = fm_expf(-1.0f / (0.006f * sr));
    } else {                                  /* the 808's cowbell: two squares into a band-pass */
        float f2;
        f1 = 540.0f * semitones(v->tune);
        f2 = 800.0f * semitones(v->tune);
        set_bp(v, 2640.0f, 1.1f, sr);
        env_set(v, 0.10f + 0.42f * v->decay, sr);
        v->env2 = 1.0f;
        v->env2_c = fm_expf(-1.0f / (0.028f * sr));
        v->w2 = FM_TWO_PI * f2 / sr;
        v->ph2 = 0.0f;
    }
    v->w1 = FM_TWO_PI * f1 / sr;
    v->ph1 = 0.0f;
    v->hit_acc = acc;
}
static float rend_cowb(dr_part_t *v)
{
    float s;
    v->ph1 += v->w1;
    if (v->ph1 >= FM_TWO_PI)
        v->ph1 -= FM_TWO_PI;
    if (v->kit == DR_KIT_707) {
        s = (sq(v->ph1) + 0.8f * noise_white(&v->seed)) * 0.6f * env_step(&v->env2, v->env2_c);
        return bq_run(v, s) * env_run(&v->env, v->env_c, &v->on) * v->hit_acc * v->level;
    }
    v->ph2 += v->w2;
    if (v->ph2 >= FM_TWO_PI)
        v->ph2 -= FM_TWO_PI;
    s = (sq(v->ph1) + sq(v->ph2)) * 0.55f * (0.55f + 0.45f * env_step(&v->env2, v->env2_c));
    return bq_run(v, s) * env_run(&v->env, v->env_c, &v->on) * v->hit_acc * v->level * 0.78f;
}

/* ------------------------------------------------------------------ machine --- */
void dr_init(dr_machine_t *d, float sr)
{
    int i, j;
    d->sr = sr;
    d->kit = DR_KIT_808;
    d->acc_amt = 1.0f;
    for (i = 0; i < DR_NPARTS; i++) {
        dr_part_t *v = &d->p[i];
        v->on = 0;
        v->kit = DR_KIT_808;
        v->env = 0.0f;
        v->env2 = 0.0f;
        v->level = 0.8f;
        v->tune = 0.5f;
        v->decay = 0.5f;
        v->hit_acc = 1.0f;
        v->seed = 0x9E3779B9u * (uint32_t)(i + 7);
        for (j = 0; j < 6; j++)
            v->hph[j] = 0.0f;
        set_bp(v, 1000.0f, 1.0f, sr);
        set_hp(v, 500.0f, sr);
    }
}

void dr_set(dr_machine_t *d, int part, int param, float value)
{
    dr_part_t *v;
    if (part < 0 || part >= DR_NPARTS || param < 0 || param >= DR_NPAR)
        return;
    v = &d->p[part];
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    if (param == DR_LEVEL)
        v->level = value;
    else if (param == DR_TUNE)
        v->tune = value;
    else
        v->decay = value;
}

void dr_kit(dr_machine_t *d, int kit)
{
    int i;
    if (kit < 0 || kit >= DR_NKITS)
        return;
    d->kit = kit;
    for (i = 0; i < DR_NPARTS; i++)
        d->p[i].kit = kit;
}

void dr_accamt(dr_machine_t *d, float value)
{
    d->acc_amt = fm_clampf(value, 0.0f, 1.0f);
}

void dr_trigger(dr_machine_t *d, int part, int accent)
{
    dr_part_t *v;
    float acc;
    if (part < 0 || part >= DR_NPARTS)
        return;
    v = &d->p[part];
    acc = accent ? 1.0f + d->acc_amt * ACCENT_BOOST : 1.0f;
    v->on = 1;
    switch (part) {
    case DR_KICK: trig_kick(v, acc, d->sr); break;
    case DR_SNARE: trig_snare(v, acc, d->sr); break;
    case DR_CLAP: trig_clap(v, acc, d->sr); break;
    case DR_CHH:
        trig_hat(v, acc, d->sr, 0);
        {                                     /* the pedal: a closed hat chokes the open one */
            dr_part_t *o = &d->p[DR_OHH];
            if (o->on)
                o->env_c = fm_expf(-1.0f / (CHOKED_TAU * d->sr));
        }
        break;
    case DR_OHH: trig_hat(v, acc, d->sr, 1); break;
    case DR_TOML: trig_tom(v, acc, d->sr, 96.0f); break;
    case DR_TOMH: trig_tom(v, acc, d->sr, 176.0f); break;
    default: trig_cowb(v, acc, d->sr); break;
    }
}

void dr_panic(dr_machine_t *d)
{
    int i;
    for (i = 0; i < DR_NPARTS; i++)
        d->p[i].on = 0;
}

void dr_render(dr_machine_t *d, float *out, uint32_t n)
{
    uint32_t i;
    int p;
    for (i = 0; i < n; i++)
        out[i] = 0.0f;
    for (p = 0; p < DR_NPARTS; p++) {
        dr_part_t *v = &d->p[p];
        if (!v->on)
            continue;
        for (i = 0; i < n; i++) {
            float s;
            switch (p) {
            case DR_KICK: s = rend_kick(v); break;
            case DR_SNARE: s = rend_snare(v); break;
            case DR_CLAP: s = rend_clap(v, d->sr); break;
            case DR_CHH:
            case DR_OHH: s = rend_hat(v, d->sr); break;
            case DR_TOML:
            case DR_TOMH: s = rend_tom(v); break;
            default: s = rend_cowb(v); break;
            }
            out[i] += s;
            if (!v->on)
                break;                        /* it died inside the block */
        }
    }
}
