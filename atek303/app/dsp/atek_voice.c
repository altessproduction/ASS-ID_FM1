/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 * Open303 parts: Copyright (c) Robin Schmidt (www.rs-met.com), MIT licence (see LICENSE-open303.txt).
 *
 * The ATEK303 voice in C. See atek_voice.h for what each part is and where it comes from. The
 * names follow the C++ originals so the two can be read side by side. */
#include "atek_voice.h"
#include "fastmath.h"

#define OS 4                                   /* Open303's oversampling */
#define DECAY_T_OVER_TAU 2.302585093f
#define FIXED_VOLUME_DB (-6.0f)
#define OUTPUT_LEVEL 5.0f                      /* the module's volts */
#define SAT 7.0f                               /* the module's default output saturation (soft) */
#define OTA_HEADROOM 0.6f                      /* the OTA BA662A (otaIdx 1) */
#define ACCENT_TAU_FAST 0.10f
#define ACCENT_TAU_SLOW 0.45f
#define ACCENT_STEP 0.55f
#define ACCENT_DEPTH 0.8f
#define DRIFT_TAU 25.0f
#define SLIDE_TAU_MS 120.0f                    /* slideTauIdx 1 */
#define FILTER_DRIVE 2.0f                      /* filterDriveIdx 1: +6 dB */
/* the ATEK oscillator's shape (the module's defaults: pulseWidthIdx 0, droopIdx 4, sawResetIdx 3,
 * sawDroopIdx 3) */
#define PULSE_WIDTH 0.44f
#define SQUARE_DROOP_HZ 60.0f
#define SAW_RESET_US 20.0f
#define SAW_DROOP_HZ 15.0f
#define SQUARE_SLEW_US 5.0f
#define OSC_GAIN 0.484f
/* the ATEK filter */
#define CAP_RATIO (33.0f / 18.0f)
#define VT 1.2f
#define FB_HIGHPASS_HZ 150.0f
#define MAX_CUTOFF_HZ 14000.0f
#define K_MAX 40.0f

/* Open303's 12th-order elliptic quarter-band filter as six biquads (scipy tf2sos of its direct
 * form II coefficients): b0, b1, b2, a1, a2 */
static const float AA_SOS[6][5] = {
    {1.3671732100e-04f, 1.8695892579e-04f, 1.3671732100e-04f, -1.5863116559e+00f, 6.4282028140e-01f},
    {1.0000000000e+00f, -3.7140354711e-01f, 1.0000000000e+00f, -1.5620957226e+00f, 7.1615708784e-01f},
    {1.0000000000e+00f, -1.0298242816e+00f, 1.0000000000e+00f, -1.5310700890e+00f, 8.1309516633e-01f},
    {1.0000000000e+00f, -1.2676403937e+00f, 1.0000000000e+00f, -1.5080939976e+00f, 8.9315821803e-01f},
    {1.0000000000e+00f, -1.3628794779e+00f, 1.0000000000e+00f, -1.4983265470e+00f, 9.4752881661e-01f},
    {1.0000000000e+00f, -1.3980247164e+00f, 1.0000000000e+00f, -1.5032624531e+00f, 9.8437474069e-01f},
};

/* --------------------------------------------------------------- helpers --- */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float pitch_to_freq(float pitch, float a4)   /* Open303's pitchToFreq */
{
    return a4 * 0.018581361171917517f * fm_expf(0.057762265046662109f * pitch);
}
static inline float db2amp(float db) { return fm_expf(db * 0.11512925464970228f); }

/* rosic::BiquadFilter (direct form I, its sign convention: y = b x ... + a1 y1 + a2 y2) */
static inline float bq_rosic(at_biquad_t *f, float in)
{
    float y = f->b0 * in + f->b1 * f->x1 + f->b2 * f->x2 + f->a1 * f->y1 + f->a2 * f->y2;
    f->x2 = f->x1;
    f->x1 = in;
    f->y2 = f->y1;
    f->y1 = y;
    return y;
}
static void bq_reset(at_biquad_t *f) { f->x1 = f->x2 = f->y1 = f->y2 = 0.0f; }
static void bq_lowpass12(at_biquad_t *f, float freq, float q, float sr)
{
    float w = FM_TWO_PI * freq / sr, s = fm_sinf(w), c = fm_cosf(w);
    float alpha = s / (2.0f * q), scale = 1.0f / (1.0f + alpha);
    f->a1 = 2.0f * c * scale;
    f->a2 = (alpha - 1.0f) * scale;
    f->b1 = (1.0f - c) * scale;
    f->b0 = f->b2 = 0.5f * f->b1;
}
static void bq_bandreject(at_biquad_t *f, float freq, float bw, float sr)
{
    float w = FM_TWO_PI * freq / sr, s = fm_sinf(w), c = fm_cosf(w);
    float x = 0.5f * FM_LN2 * bw * w / s;
    float alpha = s * 0.5f * (fm_expf(x) - fm_expf(-x));   /* s * sinh(x) */
    float scale = 1.0f / (1.0f + alpha);
    f->a1 = 2.0f * c * scale;
    f->a2 = (alpha - 1.0f) * scale;
    f->b0 = scale;
    f->b1 = -2.0f * c * scale;
    f->b2 = scale;
}
/* the anti-aliasing biquads: transposed direct form II, scipy's signs (y = b x ... - a1 y1 - a2 y2);
 * x1, x2 hold the two states */
static inline float aa_run(at_biquad_t *s, float v)
{
    int i;
    for (i = 0; i < 6; i++) {
        at_biquad_t *f = &s[i];
        float o = f->b0 * v + f->x1;
        f->x1 = f->b1 * v - f->a1 * o + f->x2;
        f->x2 = f->b2 * v - f->a2 * o;
        v = o;
    }
    return v;
}

/* rosic::OnePoleFilter */
static inline float op_run(at_onepole_t *f, float in)
{
    float y = f->b0 * in + f->b1 * f->x1 + f->a1 * f->y1;
    f->x1 = in;
    f->y1 = y;
    return y;
}
static void op_highpass(at_onepole_t *f, float fc, float sr)
{
    float x = fm_expf(-FM_TWO_PI * fc / sr);
    f->b0 = 0.5f * (1.0f + x);
    f->b1 = -f->b0;
    f->a1 = x;
}
static void op_allpass(at_onepole_t *f, float fc, float sr)
{
    float t = fm_tanf(FM_PI * fc / sr), x = (t - 1.0f) / (t + 1.0f);
    f->b0 = x;
    f->b1 = 1.0f;
    f->a1 = -x;
}

static uint32_t xorshift(at_voice_t *v)
{
    uint32_t x = v->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return v->rng = x;
}
static float gauss(at_voice_t *v)                  /* three uniforms on [-1, 1): variance 1 */
{
    float s = 0.0f;
    int i;
    for (i = 0; i < 3; i++)
        s += (float)(xorshift(v) >> 8) * (2.0f / 16777216.0f) - 1.0f;
    return s;
}
static float dac_error_cents(const at_voice_t *v, int note)   /* -1 .. +1, fixed per note and unit */
{
    uint32_t h = (uint32_t)note * 2654435761u ^ v->unit_seed * 40503u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xffffu) / 32768.0f - 1.0f;
}

/* ------------------------------------------------------------ ATEK osc --- */
static void osc_coeffs(at_voice_t *v)
{
    float sr = OS * v->sr;
    float fc_saw = 1.0f / (FM_TWO_PI * SAW_RESET_US * 1e-6f), fc_sq = 1.0f / (FM_TWO_PI * SQUARE_SLEW_US * 1e-6f);
    v->a_lp_saw = 1.0f - fm_expf(-FM_TWO_PI * fm_minf(fc_saw, 0.45f * sr) / sr);
    v->a_lp_sq = 1.0f - fm_expf(-FM_TWO_PI * fm_minf(fc_sq, 0.45f * sr) / sr);
    v->a_hp_saw = fm_expf(-FM_TWO_PI * SAW_DROOP_HZ / sr);
    v->a_hp_sq = fm_expf(-FM_TWO_PI * SQUARE_DROOP_HZ / sr);
    v->pw = PULSE_WIDTH;
}
static void osc_reset(at_voice_t *v)
{
    v->ph = 0.0f;
    v->lp_saw = v->lp_sq = 0.0f;
    v->hp_saw_y = v->hp_saw_x = v->hp_sq_y = v->hp_sq_x = 0.0f;
}
static inline float poly_blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
static inline float osc_sample(at_voice_t *v)
{
    float saw, sq, t_fall, inc = v->inc;
    v->ph += inc;
    if (v->ph >= 1.0f)
        v->ph -= 1.0f;
    saw = 2.0f * v->ph - 1.0f - poly_blep(v->ph, inc);
    sq = (v->ph < v->pw) ? 1.0f : -1.0f;
    sq += poly_blep(v->ph, inc);
    t_fall = v->ph + (1.0f - v->pw);
    if (t_fall >= 1.0f)
        t_fall -= 1.0f;
    sq -= poly_blep(t_fall, inc);
    v->lp_saw += v->a_lp_saw * (saw - v->lp_saw);
    v->lp_sq += v->a_lp_sq * (sq - v->lp_sq);
    v->hp_saw_y = v->a_hp_saw * (v->hp_saw_y + v->lp_saw - v->hp_saw_x);
    v->hp_saw_x = v->lp_saw;
    v->hp_sq_y = v->a_hp_sq * (v->hp_sq_y + v->lp_sq - v->hp_sq_x);
    v->hp_sq_x = v->lp_sq;
    return OSC_GAIN * (v->hp_saw_y + v->blend * (v->hp_sq_y - v->hp_saw_y));
}

/* --------------------------------------------------------- ATEK filter --- */
static inline float diode(float x) { return VT * fm_tanhf(x * (1.0f / VT)); }
static void filt_coeffs(at_voice_t *v, float cutoff)
{
    float sr = OS * v->sr;
    float fc = fm_maxf(10.0f, fm_minf(fm_minf(cutoff, MAX_CUTOFF_HZ), 0.45f * sr));
    float fx = fc / (sr * 1.4142135623730951f), k_scale, g;
    v->f_b0 = (0.00045522346f + 6.1922189f * fx) / (1.0f + 12.358354f * fx + 4.4156345f * (fx * fx));
    k_scale = fx * (fx * (fx * (fx * (fx * (fx + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f)
              + 16.998792f;
    g = k_scale * (1.0f / 17.0f);
    g = (g - 1.0f) * v->f_res + 1.0f;
    v->f_gcomp = g * (1.0f + v->f_res);
    v->f_k = fm_minf(k_scale * v->f_res, K_MAX);
}
static void filt_reset(at_voice_t *v) { v->y1 = v->y2 = v->y3 = v->y4 = v->fb_hp = v->fb_prev = 0.0f; }
static inline float filt_sample(at_voice_t *v, float in)
{
    float fb = v->f_k * diode(v->y4), x, b0 = v->f_b0;
    v->fb_hp = v->fb_hp_c * (v->fb_hp + fb - v->fb_prev);
    v->fb_prev = fb;
    x = v->f_drive * in - v->fb_hp;
    v->y1 += CAP_RATIO * b0 * diode(x - v->y1 + v->y2);
    v->y2 += b0 * diode(v->y1 - 2.0f * v->y2 + v->y3);
    v->y3 += b0 * diode(v->y2 - 2.0f * v->y3 + v->y4);
    v->y4 += b0 * diode(v->y3 - 2.0f * v->y4);
    if (!(fm_fabsf(v->y1) + fm_fabsf(v->y2) + fm_fabsf(v->y3) + fm_fabsf(v->y4) < 1e6f))   /* NaN or runaway */
        filt_reset(v);
    return 2.0f * v->f_gcomp * v->y4;
}

/* ------------------------------------------------------------- Open303 --- */
static void env_mod_scaler(at_voice_t *v)          /* calculateEnvModScalerAndOffset, measured mapping */
{
    const float c0 = 313.8152786059267f, c1 = 2394.411986817546f;
    float e = v->env_mod * 0.01f;
    float c = fm_logf(v->cutoff / c0) / fm_logf(c1 / c0);
    float s_lo = 3.773996325111173f * e + 0.736965594166206f;
    float s_hi = 4.194548788411135f * e + 0.864344900642434f;
    v->env_scaler = (1.0f - c) * s_lo + c * s_hi;
    v->env_offset = 0.048292930943553f * c + 0.294391201442418f;
}
static void main_env_decay(at_voice_t *v, float tau_ms)
{
    if (tau_ms > 0.001f) {
        v->main_c = fm_expf(-1.0f / (0.001f * tau_ms * v->sr));
        v->main_init = 1.0f / v->main_c;
    }
}
static float env_coeff(const at_voice_t *v, float ms)   /* AnalogEnvelope: 1 - e^(-1/tau) */
{
    return ms > 0.0f ? 1.0f - fm_expf(-1.0f / (v->sr * 0.001f * ms)) : 1.0f;
}
static void amp_release(at_voice_t *v, float ms)
{
    v->amp_release_ms = ms;
    v->amp_release_c = env_coeff(v, ms);
}
static void set_accent_and_decay(at_voice_t *v, int has_accent)
{
    if (has_accent) {
        v->accent_gain = v->accent;
        main_env_decay(v, v->accent_decay);
        amp_release(v, 50.0f);
    } else {
        v->accent_gain = 0.0f;
        main_env_decay(v, v->normal_decay);
        amp_release(v, 1.0f);
    }
}
static void trigger_note(at_voice_t *v, int note, int has_accent)
{
    if (v->idle) {
        int i;
        osc_reset(v);
        filt_reset(v);
        v->hp1.x1 = v->hp1.y1 = v->hp2.x1 = v->hp2.y1 = v->allpass.x1 = v->allpass.y1 = 0.0f;
        bq_reset(&v->notch);
        for (i = 0; i < 6; i++)
            bq_reset(&v->aa[i]);
        bq_reset(&v->declick);
    }
    set_accent_and_decay(v, has_accent);
    v->osc_freq = pitch_to_freq((float)note, v->tuning);
    v->slew_y = fm_logf(v->osc_freq);
    v->main_y = v->main_init;                      /* mainEnv.trigger() */
    v->amp_time = 0.0f;                            /* ampEnv.noteOn(true): from the current level */
    v->amp_on = 1;
    v->idle = 0;
}
static void slide_to_note(at_voice_t *v, int note, int has_accent)
{
    v->osc_freq = pitch_to_freq((float)note, v->tuning);
    set_accent_and_decay(v, has_accent);
    v->idle = 0;
}
static void amp_note_off(at_voice_t *v)
{
    v->amp_on = 0;
    v->amp_time = v->amp_decay_ms + v->amp_inc;    /* attack 0 + hold 0 + decay + increment */
}
static inline float amp_sample(at_voice_t *v)
{
    float y = v->amp_y;
    if (v->amp_time <= 0.0f) {                     /* attack 0: straight to the peak */
        y = 1.0f;
        v->amp_time += v->amp_inc;
    } else if (v->amp_time <= v->amp_decay_ms) {
        y -= v->amp_decay_c * y;
        v->amp_time += v->amp_inc;
    } else if (v->amp_on)
        y -= v->amp_decay_c * y;
    else {
        y -= v->amp_release_c * y;
        v->amp_time += v->amp_inc;
    }
    return v->amp_y = y;
}

/* ---------------------------------------------------- the module's part --- */
static float accent_amount(const at_voice_t *v) { return 0.5f * v->acc_fast + 0.5f * v->acc_slow; }
static float effective_accent(const at_voice_t *v)   /* percent */
{
    float base = 100.0f * v->param[AT_ACCENT];
    return fm_minf(100.0f, base * (1.0f + ACCENT_DEPTH * accent_amount(v)));
}
static float decay_tau(float knob)                 /* the schematic's range: T 200 ms .. 2.5 s, as tau */
{
    float lo = 200.0f / DECAY_T_OVER_TAU, hi = 2500.0f / DECAY_T_OVER_TAU;
    return lo * fm_powf(hi / lo, knob);
}
static void update_params(at_voice_t *v)           /* Atek303::updateParams, every 16 samples */
{
    float cut_hz, x, e, dt, bend;
    v->tuning = 400.0f + 80.0f * v->param[AT_TUNING];
    cut_hz = 314.0f * fm_powf(2394.0f / 314.0f, v->param[AT_CUTOFF]);
    if (v->drift_on)
        cut_hz *= 1.0f + v->drift_cut * 0.015f;
    v->cutoff = cut_hz;
    /* resonance: the module pre-warps the linear pot by the inverse of the filter's curve, so the
     * ATEK filter's 0..1 resonance is the knob itself */
    v->f_res = clampf(v->param[AT_RESONANCE], 0.0f, 1.0f);
    x = v->param[AT_ENVMOD];
    v->env_mod = 100.0f * x * x * x;               /* the log pot VR5 */
    env_mod_scaler(v);
    v->normal_decay = decay_tau(v->param[AT_DECAY]);
    v->accent_decay = 200.0f / DECAY_T_OVER_TAU;
    v->accent = 0.01f * effective_accent(v);
    v->blend = v->param[AT_WAVE];
    /* the drift's random walk, and the bend it gives (integer notes: no quantisation offset) */
    dt = 16.0f / v->sr;
    e = fm_sqrtf(dt) * 0.25f;
    v->drift_pitch = clampf(v->drift_pitch - v->drift_pitch * (dt / DRIFT_TAU) + gauss(v) * e, -3.0f, 3.0f);
    v->drift_cut = clampf(v->drift_cut - v->drift_cut * (dt / DRIFT_TAU) + gauss(v) * e, -3.0f, 3.0f);
    bend = 0.0f;
    if (v->drift_on)
        bend = (dac_error_cents(v, v->held_note < 0 ? 60 : v->held_note) * 1.5f + v->drift_pitch * 1.5f) / 100.0f;
    v->pitch_wheel = fm_exp2f(bend / 12.0f);
}

/* ------------------------------------------------------------- the API --- */
void at_init(at_voice_t *v, float sr)
{
    int i;
    uint8_t *p = (uint8_t *)v;
    for (i = 0; i < (int)sizeof *v; i++)
        p[i] = 0;
    v->sr = sr;
    v->param[AT_TUNING] = 0.5f;
    v->param[AT_CUTOFF] = 0.35f;
    v->param[AT_RESONANCE] = 0.5f;
    v->param[AT_ENVMOD] = 0.5f;
    v->param[AT_DECAY] = 0.4f;
    v->param[AT_ACCENT] = 0.5f;
    v->param[AT_WAVE] = 1.0f;
    v->drift_on = 1;
    v->unit_seed = 0;
    v->rng = 0x9E3779B9u;
    v->held_note = -1;
    v->idle = 1;
    v->pitch_wheel = 1.0f;
    v->amp_scaler = db2amp(FIXED_VOLUME_DB);
    v->osc_freq = 440.0f;
    v->tuning = 440.0f;
    v->f_drive = FILTER_DRIVE;
    v->fb_hp_c = fm_expf(-FM_TWO_PI * FB_HIGHPASS_HZ / (OS * sr));
    v->slew_coeff = fm_expf(-1.0f / (sr * 0.001f * SLIDE_TAU_MS));
    v->rc2_coeff = fm_expf(-1.0f / (sr * 0.001f * 15.0f));
    v->acc_fast_c = 1.0f - (1.0f / sr) / ACCENT_TAU_FAST;
    v->acc_slow_c = 1.0f - (1.0f / sr) / ACCENT_TAU_SLOW;
    /* the amp envelope: attack 0, decay 1230 ms to 0, release 0.5 ms until a note sets it */
    v->amp_inc = 1000.0f / sr;
    v->amp_decay_ms = 1230.0f;
    v->amp_decay_c = env_coeff(v, 1230.0f);
    amp_release(v, 0.5f);
    v->main_c = 1.0f;
    v->main_y = v->main_init = 1.0f;
    main_env_decay(v, 200.0f);
    bq_lowpass12(&v->declick, 200.0f, 0.70710678f, sr);
    bq_bandreject(&v->notch, 7.5164f, 4.7f, sr);
    for (i = 0; i < 6; i++) {
        v->aa[i].b0 = AA_SOS[i][0];
        v->aa[i].b1 = AA_SOS[i][1];
        v->aa[i].b2 = AA_SOS[i][2];
        v->aa[i].a1 = AA_SOS[i][3];
        v->aa[i].a2 = AA_SOS[i][4];
    }
    op_highpass(&v->hp1, 44.486f, OS * sr);
    op_highpass(&v->hp2, 24.167f, sr);
    op_allpass(&v->allpass, 14.008f, sr);
    osc_coeffs(v);
    update_params(v);
    v->div = 0;
}

void at_set(at_voice_t *v, int param, float value)
{
    if (param >= 0 && param < AT_NPARAMS)
        v->param[param] = clampf(value, 0.0f, 1.0f);
}

void at_gate(at_voice_t *v, int gate, int note, int accent, int slide)
{
    v->slide = slide;
    if (gate && !v->gate) {
        if (accent) {
            v->accent = 0.01f * effective_accent(v);
            v->acc_fast = fm_minf(1.0f, v->acc_fast + ACCENT_STEP);
            v->acc_slow = fm_minf(1.0f, v->acc_slow + ACCENT_STEP);
        }
        if (!v->note_list)                         /* core.noteOn(note, accent ? 127 : 64) */
            trigger_note(v, note, accent);
        else
            slide_to_note(v, note, accent);
        v->note_list = 1;
        v->held_note = note;
        v->note_held = 1;
    } else if (v->auto_legato && gate && v->gate && note != v->held_note) {   /* the module's auto legato: a new note while held */
        slide_to_note(v, note, accent);
        v->held_note = note;
    } else if (!gate && v->gate) {
        if (!slide) {                              /* core.allNotesOff() */
            v->note_list = 0;
            amp_note_off(v);
            v->note_held = 0;
            v->held_note = -1;
        }
    }
    v->gate = gate;
}

void at_panic(at_voice_t *v)
{
    v->gate = 0;
    v->note_list = 0;
    v->note_held = 0;
    v->held_note = -1;
    v->amp_on = 0;
    v->amp_y = 0.0f;
    v->amp_time = v->amp_decay_ms + v->amp_inc;
}

void at_render(at_voice_t *v, float *out, uint32_t n)
{
    uint32_t i;
    int j;
    for (i = 0; i < n; i++) {
        float inst_freq, hz, main_out, tmp1, tmp2, amp_out, s = 0.0f, sr4 = OS * v->sr;
        if ((v->div++ & 15u) == 0u)
            update_params(v);
        v->acc_fast *= v->acc_fast_c;
        v->acc_slow *= v->acc_slow_c;
        if (v->idle) {
            out[i] = 0.0f;
            continue;
        }
        /* the slide, on log Hz */
        v->slew_y = fm_logf(v->osc_freq) + v->slew_coeff * (v->slew_y - fm_logf(v->osc_freq));
        inst_freq = fm_expf(v->slew_y);
        hz = clampf(inst_freq * v->pitch_wheel, 0.01f, 0.45f * sr4);
        v->inc = hz / sr4;
        /* the cutoff, from the nominal one, the filter envelope and the accent */
        main_out = (v->main_y *= v->main_c);
        tmp2 = v->accent_gain > 0.0f ? main_out : 0.0f;
        v->rc2_y = tmp2 + v->rc2_coeff * (v->rc2_y - tmp2);
        tmp1 = v->env_scaler * (main_out - v->env_offset);
        tmp2 = v->accent_gain * v->rc2_y;
        filt_coeffs(v, v->cutoff * fm_exp2f(tmp1 + tmp2));
        /* the amplitude */
        amp_out = amp_sample(v);
        if (v->amp_on)
            amp_out += (0.45f + 4.0f * v->accent_gain) * main_out;
        amp_out = bq_rosic(&v->declick, amp_out);
        /* oversampled: oscillator, pre-filter highpass, the ladder, anti-aliasing */
        for (j = 0; j < OS; j++) {
            s = -osc_sample(v);
            s = op_run(&v->hp1, s);
            s = filt_sample(v, s);
            s = aa_run(v->aa, s);
        }
        s = op_run(&v->allpass, s);
        s = op_run(&v->hp2, s);
        s = bq_rosic(&v->notch, s);
        s = OTA_HEADROOM * fm_tanhf(s * (1.0f / OTA_HEADROOM));
        s *= amp_out * v->amp_scaler;
        /* the module: volts, the soft output saturation, back to about -1..1 */
        s *= OUTPUT_LEVEL;
        s = SAT * fm_tanhf(s * (1.0f / SAT));
        out[i] = clampf(s, -12.0f, 12.0f) * (1.0f / OUTPUT_LEVEL);
    }
}
