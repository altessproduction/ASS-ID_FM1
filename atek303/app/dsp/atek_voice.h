/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 *
 * ATEK303 voice for the M-VAVE FM-1: the TB-303 voice of the ATEK303 VCV Rack module
 * (../../../VCV-Rack/ATEK303), in single-precision C with no libm.
 *
 * What it is, piece by piece, and where each piece comes from:
 *   - the Open303 core by Robin Schmidt (MIT, thirdparty/open303 of ATEK303): the envelopes, the
 *     cutoff modulation from the measured mapping, the slide, the 4x oversampling with its
 *     elliptic anti-aliasing filter and the fixed post-filters (allpass, highpass, notch);
 *   - the ATEK oscillator (src/AtekOsc.hpp): polyBLEP ramp and comparator square with the
 *     rounded reset, the slewed edges and the AC-coupling droop;
 *   - the ATEK filter (src/AtekFilter.hpp): the diode ladder with a soft diode in every cell,
 *     the real capacitor ratio and the drive;
 *   - the ATEK module (src/Atek303.cpp) in its "circuit" sound model: decay range of the
 *     schematic, log env mod pot, linear resonance pot, accent accumulation, OTA headroom,
 *     output saturation, slide in the pitch domain, analog drift.
 *
 * Departures from the C++ original, on purpose:
 *   - float instead of double (the FM-1's FPU is single precision); the 12th-order elliptic
 *     filter runs as six biquads (computed from Open303's direct form II coefficients with
 *     scipy's tf2sos, same response within 2e-7), which float can run, where the direct form
 *     can't;
 *   - the drift's random walk uses a xorshift generator and a sum of uniforms instead of
 *     Rack's random::normal();
 *   - no Open303 sequencer and no note list: the voice follows gate, accent and slide as the
 *     VCV module does with its inputs.
 *
 * Threads: at_render() runs in the audio interrupt; the rest is called between blocks (the app
 * posts to it from the main loop through its own queue). */
#pragma once
#include <stdint.h>

/* the seven controls of the 303, 0..1 each (wave: 0 saw, 1 square) */
enum { AT_TUNING, AT_CUTOFF, AT_RESONANCE, AT_ENVMOD, AT_DECAY, AT_ACCENT, AT_WAVE, AT_NPARAMS };

typedef struct {
    float b0, b1, b2, a1, a2, x1, x2, y1, y2;
} at_biquad_t;

typedef struct {
    float b0, b1, a1, x1, y1;
} at_onepole_t;

typedef struct {
    /* settings */
    float sr;                         /* the voice's sample rate (44100 on the FM-1) */
    float param[AT_NPARAMS];
    int drift_on;                     /* analog drift (the circuit model's "subtle") */
    int auto_legato;                  /* the module's option: a new note while the gate is high slides to it */
    uint32_t unit_seed;               /* which 303: the DAC's per-note error */

    /* gate logic (as the VCV module's inputs) */
    int gate, slide, note_held, held_note;

    /* Open303 state */
    float tuning, cutoff, env_mod, accent, normal_decay, accent_decay;
    float env_scaler, env_offset, accent_gain, osc_freq, pitch_wheel, amp_scaler;
    int note_list;                    /* Open303's note list, as the module uses it: empty or not */
    int idle;
    float slew_coeff, slew_y;         /* the pitch slew limiter (on log Hz) */
    float rc2_coeff, rc2_y;           /* the accent's RC */
    float main_c, main_y, main_init;  /* the filter envelope (pure decay) */
    /* the amp envelope (AnalogEnvelope: attack 0, decay 1230 ms, sustain 0, release by accent) */
    float amp_y, amp_time, amp_inc, amp_decay_c, amp_release_c, amp_decay_ms, amp_release_ms;
    int amp_on;
    at_biquad_t declick, notch, aa[6];
    at_onepole_t hp1, hp2, allpass;

    /* ATEK oscillator */
    float ph, inc, blend, pw, a_lp_saw, a_lp_sq, a_hp_saw, a_hp_sq;
    float lp_saw, lp_sq, hp_saw_y, hp_saw_x, hp_sq_y, hp_sq_x;

    /* ATEK filter */
    float f_cut, f_res, f_drive, f_b0, f_k, f_gcomp, y1, y2, y3, y4, fb_hp, fb_prev, fb_hp_c;

    /* the module's extras */
    float acc_fast, acc_slow, drift_pitch, drift_cut;
    float acc_fast_c, acc_slow_c;
    uint32_t rng;
    uint32_t div;                     /* the module updates its parameters every 16 samples */
} at_voice_t;

void at_init(at_voice_t *v, float sr);
void at_set(at_voice_t *v, int param, float value);   /* 0..1 */

/* the gate, the note (MIDI), accent and slide, as the module's GATE, V/OCT, ACC and SLIDE inputs:
 * a gate going up plays the note (sliding if the last gate went down with slide held); a gate going
 * down releases it unless slide is held. */
void at_gate(at_voice_t *v, int gate, int note, int accent, int slide);
void at_panic(at_voice_t *v);

/* n mono samples, about -1..1 (the module's volts / 5) */
void at_render(at_voice_t *v, float *out, uint32_t n);
