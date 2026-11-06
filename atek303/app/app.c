/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek)
 * Built on fm1-base's template (MPL-2.0, Charles Vestal).
 *
 * ATEK303 on the FM-1: the ATEK303 voice and the ATEK303 SEQ's acid pattern generator.
 *
 *   PLAY            the sequencer runs / stops (internal clock, or MIDI clock when it comes in)
 *   SEQ             GENERATE: a new pattern (a new seed). With LOCK on: mutate all three layers
 *   ENV / LFO / ARP mutate TIME / NOTES (octaves) / SLIDES and ACCENTS
 *   EDIT            the step editor (a piano roll with UP, DOWN, GATE, ACC and SLIDE rows):
 *                     SELECT or K1 moves the cursor over the steps; a key sets the step's note and
 *                     moves on; K2 its octave (UP / DOWN), K3 its gate (REST, NOTE, TIE), K4 its note
 *                     a semitone; ENV toggles its ACCENT, LFO its SLIDE; REC undoes; EDIT or HOME leave.
 *                     A note outside the scale turns the scale CHROMATIC (nothing is lost)
 *   HOME            straight to page 1 (CUTOFF, RESO, ENV MOD, DECAY), out of the editor
 *   REC             undo the last mutation or edit
 *   GLO             LOCK: GENERATE mutates instead of making a new seed
 *   keys            stopped: play the voice (mono; legato slides; a hard press is an accent).
 *                   Running: transpose the pattern (the key is the new root, as on a 303)
 *   OCT- / OCT+     the octave
 *   SELECT          the page; K1..K4 its four knobs:
 *                     1 VOICE   CUTOFF, RESO, ENV MOD, DECAY
 *                     2 VOICE   ACCENT, TUNE, WAVE, GATE (the sequencer's gate length)
 *                     3 PATTERN DENSITY, RANGE, ACCENTS, SLIDES (they regrow the pattern from its seed)
 *                     4 PATTERN STEPS, SCALE, TEMPO, ROOT
 *                     5 DELAY   TIME (synced: 1/16 .. 1/4.), FEEDBACK, TONE, MIX (ping-pong)
 *                     6 CHORUS  TYPE (CHORUS, FLANGER), RATE, DEPTH, MIX
 *                     7 DRIVE   TYPE (OFF, OVERDRIVE, RAT, FUZZ), DRIVE, TONE, LEVEL
 *                     (the pages' order; the sound goes drive -> chorus -> delay -> reverb)
 *                     8 REVERB  DECAY, DAMP, PRE-DELAY, MIX (Dattorro's plate)
 *   FX              jumps to the effect pages (again: the next one)
 *   SAVE            a new PRESET: everything (pattern, voice, effects, tempo, scale, root) in the next
 *                   free slot of 32, with a random acid name. Held a second: over the preset loaded
 *   PRESETS         the encoder opens the PRESET LIST (the 32 slots) and moves through it; a saved preset
 *                   loads as the cursor reaches it. In the list: ENV twice deletes the one selected, SAVE on an
 *                   empty slot saves there, HOME closes it (it closes by itself after 6 s untouched)
 *   MIDI in         stopped: notes play the voice; running: notes transpose. Clock, start, stop and
 *                   continue drive the sequencer. CC 74 cutoff, 71 reso, 75 env mod, 76 decay,
 *                   77 accent, 78 tune, 79 wave
 *
 * Threads: the voice and the sequencer live in the audio interrupt. The main loop posts the keys,
 * the knobs and the buttons to a queue the render drains at the start of each block; MIDI in is
 * read there too. The screen reads the render's state as it is (a step drawn one frame late is
 * harmless). Part of fm1-base's unity build, after the platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "atek_voice.h"
#include "acid_pattern.h"
#include "fx.h"

/* the Animatek palette, as on the VCV Rack modules (UZZ-VCV-RACK src/ui/CommonWidgets.hpp, AcidEditor.hpp):
 * the screens are slate, the values in the displays' blue, the selection in the logo's blue */
#undef C_BLACK
#define C_BLACK RGB(0x16, 0x18, 0x1C)          /* the panels' slate */
#define C_LOGO RGB(0x2C, 0x7F, 0xFF)           /* the logo's blue: bars, the page selected, the cursor */
#define AK_NOTE RGB(0x2C, 0x7F, 0xFF)          /* the acid editor's colours */
#define AK_TIE RGB(0x1E, 0x4E, 0x9C)
#define AK_ACCENT RGB(0xCF, 0xE6, 0xFF)
#define AK_SLIDE RGB(0x00, 0xC2, 0xFF)
#define AK_OCTAVE RGB(0x7F, 0x9C, 0xC4)
static void animatek_palette(void)              /* over fm1-base's (gfx.c pal[]: C_LINE, C_DIM, C_GRAY, C_AMB, C_HI) */
{
    pal[0] = RGB(0x2C, 0x2C, 0x2C);            /* rules, empty bars */
    pal[1] = RGB(0x5A, 0x5A, 0x5A);            /* inactive */
    pal[2] = RGB(0x9A, 0xA2, 0xB5);            /* labels */
    pal[3] = RGB(0x7F, 0x9C, 0xC4);            /* secondary */
    pal[4] = RGB(0x5D, 0xB7, 0xFF);            /* values: the displays' blue */
}

#define SET_MAGIC 0x41543333u                  /* "AT33": another magic loads the defaults */
#define ACCENT_VEL 100u
#define SR 44100.0f
#define CHUNK 32u                              /* the sequencer's resolution: 0.7 ms */

/* the sequencer's knobs, beside the voice's (0..1 like them, or a count) */
enum { SQ_DENSITY, SQ_RANGE, SQ_ACCENT, SQ_SLIDE, SQ_GATE, SQ_NKNOBS };

typedef struct {
    uint32_t magic;
    float param[AT_NPARAMS];
    float seqk[SQ_NKNOBS];
    int8_t octave;                             /* -2 .. +2 */
    uint8_t page;
    uint8_t steps;                             /* 1..16 */
    uint8_t scale;
    uint8_t root;                              /* 0..11 */
    uint8_t locked;
    uint16_t tempo;                            /* BPM */
    uint32_t mutation_counter;
    acid_pattern_t pattern;
    float fx[FX_NPARAMS];                      /* the effects: knobs 0..1, types and TIME as indices */
} settings_t;
static settings_t set;
static uint8_t set_dirty;

/* ------------------------------------------------------------------ presets --- */
/* 32 presets, 16 to a store object (232 bytes each: 3712 of the 3840 an object takes). A preset is the
 * whole settings record and a name; the page it was saved on is not restored. */
#define NPRESETS 32
#define PRESETS_PER_OBJ 16
typedef struct {
    char name[15];
    uint8_t used;
    settings_t s;
} preset_t;
static preset_t presets[NPRESETS];
static int8_t cur_preset = -1;                 /* the one loaded (or saved) last, -1 none */

/* random acid names: two words, at most 14 characters */
static const char *const NAME_A[] = {"ACID", "RUBBER", "TOXIC", "HYPER", "SQUELCH", "LASER", "MELTED", "ELECTRIC",
                                     "BUBBLE", "WOBBLY", "SILVER", "CHROME", "PLASTIC", "LIQUID", "NEON", "RAW",
                                     "TWISTED", "FUZZY", "BENT", "WARPED", "SNAKE", "JACKIN", "DIRTY", "COSMIC"};
static const char *const NAME_B[] = {"WORM", "LINE", "BASS", "JACK", "RAVE", "PULSE", "MELT", "DROP", "TRIP", "BLOB",
                                     "SNAKE", "SQUID", "LOOP", "TRAX", "BOX", "WAVE", "ZAP", "FROG", "GOO", "BUZZ",
                                     "SLIDE", "SMILE", "FIZZ", "CRAWL"};
static uint32_t name_rng = 0x303A303Au;
static void acid_name(char *out)
{
    uint32_t a, b, i, n = 0;
    const char *p;
    name_rng ^= name_rng << 13;
    name_rng ^= name_rng >> 17;
    name_rng ^= name_rng << 5;
    a = (name_rng >> 4) % (sizeof NAME_A / sizeof NAME_A[0]);
    b = (name_rng >> 12) % (sizeof NAME_B / sizeof NAME_B[0]);
    if (NAME_A[a][0] == NAME_B[b][0] && NAME_A[a][1] == NAME_B[b][1])
        b = (b + 1u) % (sizeof NAME_B / sizeof NAME_B[0]);   /* no "SNAKE SNAKE" */
    for (p = NAME_A[a]; *p && n < 14u; p++)
        out[n++] = *p;
    if (n < 13u)
        out[n++] = ' ';
    for (p = NAME_B[b]; *p && n < 14u; p++)
        out[n++] = *p;
    for (i = n; i < 15u; i++)
        out[i] = 0;
}

static at_voice_t voice;

/* ---------------------------------------------- main loop -> render queue --- */
enum { EV_NOTE_ON, EV_NOTE_OFF, EV_PARAM, EV_PANIC, EV_PLAY, EV_GENERATE, EV_MUTATE, EV_UNDO, EV_REGROW,
       EV_ROOT, EV_FX, EV_EDIT, EV_LOAD };
/* the editor's operations (EV_EDIT: a = step, b = op | value << 8) */
enum { ED_NOTE, ED_GATE, ED_ACC, ED_SLIDE, ED_OCT, ED_SEMI };
#define QN 64u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t kind, uint32_t a, uint32_t b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;
    q[w % QN] = kind | (a & 0xFFu) << 8 | (b & 0xFFFFu) << 16;
    q_w = w + 1u;
}

/* ------------------------------------------------- the held notes (render) --- */
#define NHELD 16
static int8_t held[NHELD];
static uint8_t nheld;
static volatile int32_t ui_note = -1;
static volatile uint8_t ui_accent, ui_slide;

static void voice_follow(int accent)
{
    if (nheld) {
        int note = held[nheld - 1];
        ui_slide = (uint8_t)voice.gate;
        at_gate(&voice, 1, note, accent, 0);
        ui_note = note;
        ui_accent = (uint8_t)accent;
    } else {
        at_gate(&voice, 0, voice.held_note < 0 ? 36 : voice.held_note, 0, 0);
        ui_note = -1;
        ui_accent = ui_slide = 0;
    }
}
static void held_on(int note, uint32_t vel)
{
    uint32_t i, j;
    for (i = j = 0; i < nheld; i++)
        if (held[i] != note)
            held[j++] = held[i];
    nheld = (uint8_t)j;
    if (nheld == NHELD) {
        for (i = 1; i < NHELD; i++)
            held[i - 1] = held[i];
        nheld--;
    }
    held[nheld++] = (int8_t)note;
    voice_follow(vel >= ACCENT_VEL);
}
static void held_off(int note)
{
    uint32_t i, j, top = nheld && held[nheld - 1] == note;
    for (i = j = 0; i < nheld; i++)
        if (held[i] != note)
            held[j++] = held[i];
    nheld = (uint8_t)j;
    if (top || !nheld)
        voice_follow(0);
}

/* ----------------------------------------------------- the sequencer (render) --- */
/* Atek303Seq::process, step by step: the clock, then gate, tie and slide from the grid. */
static struct {
    acid_grid_t grid;
    acid_pattern_t undo;
    uint32_t undo_counter;
    uint8_t has_undo;
    volatile uint8_t running;
    volatile uint8_t step;
    uint8_t started;                           /* the first step is under way */
    uint8_t ext;                               /* following MIDI clock */
    uint8_t ticks;                             /* MIDI clock ticks into this step (6 a step) */
    uint32_t ext_last;                         /* render samples at the last step from MIDI clock */
    uint32_t now;                              /* samples since boot (render) */
    float step_time, period;                   /* seconds */
    uint8_t was_gate;
} sq;
static volatile uint32_t midi_count;

static acid_params_t gen_params(void)
{
    acid_params_t p;
    acid_params_default(&p);
    p.steps = ACID_MAX_STEPS;
    p.scale = set.scale;
    p.density = fm_maxf(0.05f, set.seqk[SQ_DENSITY]);
    p.accent = set.seqk[SQ_ACCENT];
    p.slide = set.seqk[SQ_SLIDE];
    p.range = set.seqk[SQ_RANGE];
    p.tie = 0.4f;
    return p;
}
static volatile uint32_t grid_version;
static void seq_render_grid(void)
{
    acid_render(&set.pattern, &sq.grid);
    grid_version++;
}
static uint32_t seed_entropy;
static void seq_generate(int new_seed)
{
    acid_params_t p = gen_params();
    uint32_t seed = set.pattern.seed;
    if (new_seed) {
        seed_entropy = seed_entropy * 1664525u + 1013904223u + sq.now;
        seed = seed_entropy ? seed_entropy : 1u;
    }
    acid_generate(&set.pattern, &p, seed);
    set.mutation_counter = 0;
    sq.has_undo = 0;
    seq_render_grid();
}
static int seq_mutate(int layer)
{
    acid_pattern_t before = set.pattern;
    uint32_t last = set.mutation_counter;
    if (acid_mutate_burst(&set.pattern, layer, set.mutation_counter + 1u, layer == ACID_MUT_ARTICULATION ? 3 : 2,
                          set.scale, &last)) {
        sq.undo = before;
        sq.undo_counter = set.mutation_counter;
        set.mutation_counter = last;
        sq.has_undo = 1;
        seq_render_grid();
        return 1;
    }
    return 0;
}
static int seq_mutate_all(void)
{
    acid_pattern_t before = set.pattern;
    uint32_t before_counter = set.mutation_counter, cursor = before_counter;
    int layer;
    for (layer = ACID_MUT_TIME; layer <= ACID_MUT_ARTICULATION; layer++) {
        uint32_t last = cursor;
        if (!acid_mutate_burst(&set.pattern, layer, cursor + 1u, layer == ACID_MUT_ARTICULATION ? 3 : 2, set.scale,
                               &last)) {
            set.pattern = before;
            return 0;
        }
        cursor = last;
    }
    sq.undo = before;
    sq.undo_counter = before_counter;
    set.mutation_counter = cursor;
    sq.has_undo = 1;
    seq_render_grid();
    return 1;
}
static int seq_undo(void)
{
    if (!sq.has_undo)
        return 0;
    set.pattern = sq.undo;
    set.mutation_counter = sq.undo_counter;
    sq.has_undo = 0;
    seq_render_grid();
    return 1;
}
static void seq_play(int on)
{
    if (on && !sq.running) {
        nheld = 0;
        sq.step = 0;
        sq.step_time = 0.0f;
        sq.started = 1;
        sq.ticks = 0;
    }
    sq.running = (uint8_t)on;
    if (!on) {
        sq.started = 0;
        at_gate(&voice, 0, voice.held_note < 0 ? 36 : voice.held_note, 0, 0);
        ui_note = -1;
        ui_accent = ui_slide = 0;
        sq.was_gate = 0;
    }
}
static int seq_len(void) { return set.steps < 1 ? 1 : set.steps > ACID_MAX_STEPS ? ACID_MAX_STEPS : set.steps; }
static void seq_next_step(void)
{
    int len = seq_len();
    sq.step = (uint8_t)((sq.step + 1) % len);
    sq.step_time = 0.0f;
}
/* the gate, tie and slide of the step under way (Atek303Seq::process), into the voice */
static void seq_apply(void)
{
    int len = seq_len(), step = sq.step >= len ? 0 : sq.step, next = (step + 1) % len, si = set.scale;
    const acid_grid_t *g = &sq.grid;
    int active = sq.started && g->gate[step], next_active = sq.started && g->gate[next];
    int tie_in = active && g->tie[step], tie_out = active && next_active && g->tie[next];
    int step_semi = acid_semi_of(g->deg[step], g->oct[step], si);
    /* a slide into another note glides; into the same note it is legato (held, no new attack), as on a 303;
     * into a rest or a tie it does nothing. (Generated patterns never slide to the same note.) */
    int slide = active && g->slide[step] && next_active && !tie_in && !g->tie[next];
    float period = sq.period;
    float gap = fm_maxf(0.0015f, 0.03f * period);
    float normal_hold = fm_minf(set.seqk[SQ_GATE] * period, period - gap);
    float tied_hold = period - gap;
    int gate = active && (tie_out || sq.step_time < (tie_in ? tied_hold : normal_hold));
    int note = 36 + 12 * (set.octave + 1) + set.root + step_semi;
    int accent = active && g->accent[step];
    if (note < 0)
        note = 0;
    if (note > 127)
        note = 127;
    at_gate(&voice, gate, note, accent, slide);
    if (gate) {
        ui_note = note;
        ui_accent = (uint8_t)accent;
        ui_slide = (uint8_t)slide;
    } else if (sq.was_gate)
        ui_note = -1;
    sq.was_gate = (uint8_t)gate;
}
static void seq_chunk(uint32_t n)              /* the internal clock, n samples on */
{
    float dt = (float)n / SR;
    if (!sq.running)
        return;
    if (!sq.ext) {
        sq.period = 15.0f / (float)(set.tempo < 30u ? 30u : set.tempo);   /* a sixteenth */
        if (sq.step_time >= sq.period)
            seq_next_step();
    }
    seq_apply();
    sq.step_time += dt;
}
static void seq_clock(uint32_t st)             /* render: MIDI realtime messages */
{
    if (st == 0xFAu) {                         /* start */
        sq.ext = 1;
        sq.running = 0;
        seq_play(1);
        sq.ticks = 0;
        sq.ext_last = sq.now;
    } else if (st == 0xFBu) {                  /* continue */
        sq.ext = 1;
        sq.running = 1;
        sq.started = 1;
    } else if (st == 0xFCu) {                  /* stop */
        seq_play(0);
    } else if (st == 0xF8u) {
        sq.ext = 1;
        if (!sq.running)
            return;
        if (++sq.ticks >= 6u) {
            uint32_t span = sq.now - sq.ext_last;
            sq.ticks = 0;
            if (span > 0u)
                sq.period = fm_clampf((float)span / SR, 0.002f, 4.0f);
            sq.ext_last = sq.now;
            seq_next_step();
        }
    }
}

/* the editor (render): change one step of the rendered grid and bring it back into the pattern */
static int degree_of(int semi, int *d, int *o)      /* semitones from the root -> degree, octave; 0: not in the scale */
{
    const acid_scale_t *sc = &ACID_SCALES[set.scale];
    int pc = ((semi % 12) + 12) % 12, i;
    for (i = 0; i < sc->n; i++)
        if (sc->s[i] == pc) {
            *d = i;
            *o = (semi - pc) / 12;
            return 1;
        }
    return 0;
}
static void grid_to_chromatic(acid_grid_t *g)       /* every step's degree as a semitone: CROM keeps every pitch */
{
    int s2;
    for (s2 = 0; s2 < ACID_MAX_STEPS; s2++) {
        int semi = acid_semi_of(g->deg[s2], g->oct[s2], set.scale), pc = ((semi % 12) + 12) % 12;
        g->deg[s2] = (int8_t)pc;
        g->oct[s2] = (int8_t)((semi - pc) / 12);
    }
    set.scale = ACID_SCALES_LEN - 1;
}
static void seq_edit(int step, int op, int val)
{
    acid_grid_t g = sq.grid;
    int d = 0, o = 0, semi;
    if (step < 0 || step >= ACID_MAX_STEPS)
        return;
    sq.undo = set.pattern;
    sq.undo_counter = set.mutation_counter;
    sq.has_undo = 1;
    if (op == ED_NOTE || op == ED_SEMI) {
        semi = op == ED_NOTE ? val - 64 : acid_semi_of(g.deg[step], g.oct[step], set.scale) + (val ? 1 : -1);
        if (semi < -24)
            semi = -24;
        if (semi > 35)
            semi = 35;
        if (!degree_of(semi, &d, &o)) {
            grid_to_chromatic(&g);
            degree_of(semi, &d, &o);
        }
        g.deg[step] = (int8_t)d;
        g.oct[step] = (int8_t)(o < -2 ? -2 : o > 2 ? 2 : o);
        if (op == ED_NOTE) {
            g.gate[step] = 1;
            g.tie[step] = 0;
        }
    } else if (op == ED_GATE) {                    /* REST -> NOTE -> TIE -> REST (a tie needs a note before) */
        int state = !g.gate[step] ? 0 : g.tie[step] ? 2 : 1, prev = step > 0 && g.gate[step - 1];
        state = (state + (val ? 1 : 2)) % 3;
        if (state == 2 && !prev)
            state = val ? 0 : 1;
        g.gate[step] = (uint8_t)(state != 0);
        g.tie[step] = (uint8_t)(state == 2);
        if (state == 2) {
            g.deg[step] = g.deg[step - 1];
            g.oct[step] = g.oct[step - 1];
        }
    } else if (op == ED_ACC) {
        if (g.gate[step] && !g.tie[step])
            g.accent[step] = (uint8_t)!g.accent[step];
    } else if (op == ED_SLIDE) {
        if (g.gate[step] && !g.tie[step])
            g.slide[step] = (uint8_t)!g.slide[step];
    } else if (op == ED_OCT) {
        o = g.oct[step] + (val ? 1 : -1);
        g.oct[step] = (int8_t)(o < -2 ? -2 : o > 2 ? 2 : o);
    }
    acid_import(&set.pattern, &g, set.pattern.time_length, set.pattern.seed, 3);
    acid_sanitize_ex(&set.pattern, set.scale, 1);   /* the player's slides stay, valid or not */
    seq_render_grid();
}

static void midi_in(void)                      /* render: the packets that came in */
{
    static const uint8_t CC_PARAM[7][2] = {{74, AT_CUTOFF}, {71, AT_RESONANCE}, {75, AT_ENVMOD}, {76, AT_DECAY},
                                           {77, AT_ACCENT}, {78, AT_TUNING}, {79, AT_WAVE}};
    uint32_t pkt, i;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st >= 0xF8u) {
            if (st != 0xFEu)
                seq_clock(st);
            continue;
        }
        midi_count++;
        if ((st & 0xF0u) == 0x90u && d2) {
            if (sq.running)
                set.root = (uint8_t)(d1 % 12u);
            else
                held_on((int)d1, d2);
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            if (!sq.running)
                held_off((int)d1);
        } else if ((st & 0xF0u) == 0xB0u) {
            if (d1 == 123u && !sq.running) {
                nheld = 0;
                voice_follow(0);
            }
            for (i = 0; i < 7u; i++)
                if (d1 == CC_PARAM[i][0]) {
                    at_set(&voice, CC_PARAM[i][1], (float)d2 / 127.0f);
                    set.param[CC_PARAM[i][1]] = (float)d2 / 127.0f;
                }
        }
    }
}

/* the outcome of a button, for the screen (the render sets it, the main loop says it) */
static volatile uint8_t ui_event, ui_event_ok;

static float mono[APP_BLOCK], fxl[APP_BLOCK], fxr[APP_BLOCK];
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, off;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    static float gain_now;
    while (q_r != q_w) {
        uint32_t e = q[q_r % QN], kind = e & 0xFFu, a = (e >> 8) & 0xFFu, b = e >> 16;
        if (kind == EV_NOTE_ON)
            held_on((int)a, b);
        else if (kind == EV_NOTE_OFF)
            held_off((int)a);
        else if (kind == EV_PARAM)
            at_set(&voice, (int)a, (float)b / 1000.0f);
        else if (kind == EV_PANIC) {
            nheld = 0;
            if (!sq.running) {
                at_panic(&voice);
                ui_note = -1;
            }
        } else if (kind == EV_PLAY) {
            sq.ext = 0;
            seq_play(!sq.running);
        } else if (kind == EV_GENERATE) {
            ui_event_ok = (uint8_t)(set.locked ? seq_mutate_all() : (seq_generate(1), 1));
            ui_event = (uint8_t)(EV_GENERATE + 1);
        } else if (kind == EV_MUTATE) {
            ui_event_ok = (uint8_t)seq_mutate((int)a);
            ui_event = (uint8_t)(EV_MUTATE + 1);
        } else if (kind == EV_UNDO) {
            ui_event_ok = (uint8_t)seq_undo();
            ui_event = (uint8_t)(EV_UNDO + 1);
        } else if (kind == EV_REGROW)
            seq_generate(0);
        else if (kind == EV_FX)
            fx_set((int)a, (float)b / 1000.0f);
        else if (kind == EV_EDIT)
            seq_edit((int)a, (int)(b & 0xFFu), (int)(b >> 8));
        else if (kind == EV_LOAD && a < NPRESETS && presets[a].used) {   /* a preset, whole, but the page */
            uint8_t page = set.page;
            int k;
            set = presets[a].s;
            set.page = page;
            for (k = 0; k < AT_NPARAMS; k++)
                at_set(&voice, k, set.param[k]);
            for (k = 0; k < FX_NPARAMS; k++)
                fx_set(k, set.fx[k]);
            acid_sanitize_ex(&set.pattern, set.scale, 1);
            sq.has_undo = 0;
            seq_render_grid();
        }
        q_r++;
    }
    midi_in();
    if (n > APP_BLOCK)
        n = APP_BLOCK;
    for (off = 0; off < n; off += CHUNK) {     /* the sequencer at 0.7 ms, the voice between */
        uint32_t c = n - off < CHUNK ? n - off : CHUNK;
        seq_chunk(c);
        at_render(&voice, mono + off, c);
        sq.now += c;
    }
    fx_tempo(sq.running && sq.ext ? sq.period : 15.0f / (float)(set.tempo < 30u ? 30u : set.tempo));
    fx_process(mono, fxl, fxr, n);
    for (i = 0; i < n; i++) {
        gain_now += (gain - gain_now) * 0.004f;
        lr[2 * i] = (int32_t)fm_clampf(fxl[i] * gain_now, -8388000.0f, 8388000.0f);
        lr[2 * i + 1] = (int32_t)fm_clampf(fxr[i] * gain_now, -8388000.0f, 8388000.0f);
    }
}

void app_panic(void)
{
    nheld = 0;
    sq.running = 0;
    sq.started = 0;
    at_panic(&voice);
    ui_note = -1;
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    int i;
    static at_voice_t d;
    acid_params_t p;
    at_init(&d, SR);
    set.magic = SET_MAGIC;
    for (i = 0; i < AT_NPARAMS; i++)
        set.param[i] = d.param[i];
    acid_params_default(&p);                   /* the SEQ module's defaults */
    set.seqk[SQ_DENSITY] = p.density;
    set.seqk[SQ_RANGE] = p.range;
    set.seqk[SQ_ACCENT] = p.accent;
    set.seqk[SQ_SLIDE] = p.slide;
    set.seqk[SQ_GATE] = 0.85f;
    set.octave = -1;
    set.page = 0;
    set.steps = 16;
    set.scale = 0;
    set.root = 0;
    set.locked = 0;
    set.tempo = 125;
    set.mutation_counter = 0;
    p.scale = 0;
    acid_generate(&set.pattern, &p, 0x41544B33u);
    set.fx[FX_DRIVE_TYPE] = 0.0f;              /* the effects: every one off (its mix at 0) */
    set.fx[FX_DRIVE] = 0.5f;
    set.fx[FX_TONE] = 0.6f;
    set.fx[FX_LEVEL] = 0.6f;
    set.fx[FX_MOD_TYPE] = 0.0f;
    set.fx[FX_MOD_RATE] = 0.3f;
    set.fx[FX_MOD_DEPTH] = 0.5f;
    set.fx[FX_MOD_MIX] = 0.0f;
    set.fx[FX_DLY_TIME] = 3.0f;                /* 1/8 dotted, the acid classic */
    set.fx[FX_DLY_FEEDBACK] = 0.45f;
    set.fx[FX_DLY_TONE] = 0.5f;
    set.fx[FX_DLY_MIX] = 0.0f;
    set.fx[FX_REV_DECAY] = 0.6f;
    set.fx[FX_REV_DAMP] = 0.4f;
    set.fx[FX_REV_PRE] = 0.2f;
    set.fx[FX_REV_MIX] = 0.0f;   /* "ATK3": the same first pattern on every FM-1 */
}

void app_init(void)
{
    int i, ok;
    at_init(&voice, SR);
    voice.auto_legato = 1;
    ok = plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
         && set.octave >= -2 && set.octave <= 2 && set.page < 8u && set.steps >= 1u && set.steps <= 16u
         && set.scale < ACID_SCALES_LEN && set.root < 12u && set.tempo >= 30u && set.tempo <= 300u;
    for (i = 0; ok && i < AT_NPARAMS; i++)
        ok = set.param[i] >= 0.0f && set.param[i] <= 1.0f;
    for (i = 0; ok && i < SQ_NKNOBS; i++)
        ok = set.seqk[i] >= 0.0f && set.seqk[i] <= 1.0f;
    for (i = 0; ok && i < FX_NPARAMS; i++)
        ok = set.fx[i] >= 0.0f && set.fx[i] <= 6.0f;
    if (!ok)
        settings_defaults();
    acid_sanitize(&set.pattern, set.scale);
    for (i = 0; i < AT_NPARAMS; i++)
        at_set(&voice, i, set.param[i]);
    fx_init(SR);
    for (i = 0; i < FX_NPARAMS; i++)
        fx_set(i, set.fx[i]);
    seq_render_grid();
    sq.period = 15.0f / (float)set.tempo;
    {                                          /* the presets: a bad or missing bank is an empty one */
        int b;
        for (b = 0; b < NPRESETS / PRESETS_PER_OBJ; b++) {
            preset_t *bank = &presets[b * PRESETS_PER_OBJ];
            if (plat_store_load((uint32_t)(OBJ_PRESETS_A + b), bank, sizeof(preset_t) * PRESETS_PER_OBJ)
                != (int)(sizeof(preset_t) * PRESETS_PER_OBJ)) {
                int k;
                for (k = 0; k < PRESETS_PER_OBJ; k++)
                    bank[k].used = 0;
            }
        }
        for (i = 0; i < NPRESETS; i++)          /* a preset from another layout is not loaded */
            if (presets[i].used && (presets[i].used != 1u || presets[i].s.magic != SET_MAGIC))
                presets[i].used = 0;
        name_rng ^= set.pattern.seed;
    }
    seed_entropy = set.pattern.seed;
}

/* --------------------------------------------------------------------- UI --- */
/* a knob of a page: a voice parameter (0..), or a sequencer control */
enum { K_NONE = -1, K_SQ = 100, K_STEPS = 200, K_SCALE, K_TEMPO, K_ROOT, K_FX = 300 };
#define NPAGES 8
static const char *const PAGE_TITLE[NPAGES] = {"VOICE", "VOICE", "PATTERN", "PATTERN", "DELAY", "CHORUS", "DRIVE", "REVERB"};
static const char *const PAGE_NAME[NPAGES][4] = {{"CUTOFF", "RESO", "ENV MOD", "DECAY"},
                                            {"ACCENT", "TUNE", "WAVE", "GATE"},
                                            {"DENSITY", "RANGE", "ACCENTS", "SLIDES"},
                                            {"STEPS", "SCALE", "TEMPO", "ROOT"},
                                            {"TIME", "FEEDBACK", "TONE", "MIX"},
                                            {"TYPE", "RATE", "DEPTH", "MIX"},
                                            {"TYPE", "DRIVE", "TONE", "LEVEL"},
                                            {"DECAY", "DAMP", "PRE-DELAY", "MIX"}};
static const int PAGE_KNOB[NPAGES][4] = {{AT_CUTOFF, AT_RESONANCE, AT_ENVMOD, AT_DECAY},
                                    {AT_ACCENT, AT_TUNING, AT_WAVE, K_SQ + SQ_GATE},
                                    {K_SQ + SQ_DENSITY, K_SQ + SQ_RANGE, K_SQ + SQ_ACCENT, K_SQ + SQ_SLIDE},
                                    {K_STEPS, K_SCALE, K_TEMPO, K_ROOT},
                                    {K_FX + FX_DLY_TIME, K_FX + FX_DLY_FEEDBACK, K_FX + FX_DLY_TONE, K_FX + FX_DLY_MIX},
                                    {K_FX + FX_MOD_TYPE, K_FX + FX_MOD_RATE, K_FX + FX_MOD_DEPTH, K_FX + FX_MOD_MIX},
                                    {K_FX + FX_DRIVE_TYPE, K_FX + FX_DRIVE, K_FX + FX_TONE, K_FX + FX_LEVEL},
                                    {K_FX + FX_REV_DECAY, K_FX + FX_REV_DAMP, K_FX + FX_REV_PRE, K_FX + FX_REV_MIX}};

static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint32_t regrow_at;                        /* a pattern knob turned: regrow when it rests */
    uint8_t edit, cursor;                      /* the step editor, and the step it points at */
    uint32_t save_t;                           /* SAVE went down (0: not held, or its hold already acted) */
    uint8_t list;                              /* the preset list is up */
    int8_t list_cur;                           /* its cursor: a slot, 0..31 */
    uint32_t list_until;                       /* it closes by itself then */
    uint32_t del_t;                            /* ENV pressed once in the list: a second press deletes */
    uint8_t redraw;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void pct_text(float x, char *b)
{
    int_text((int32_t)(100.0f * x + 0.5f), b);
    text_cat(b, " %");
}

static float knob_value(int k)                 /* 0..1, for the bar */
{
    if (k >= 0 && k < AT_NPARAMS)
        return set.param[k];
    if (k >= K_SQ && k < K_SQ + SQ_NKNOBS)
        return set.seqk[k - K_SQ];
    if (k == K_STEPS)
        return (float)set.steps / 16.0f;
    if (k == K_SCALE)
        return (float)set.scale / (float)(ACID_SCALES_LEN - 1);
    if (k == K_TEMPO)
        return (float)(set.tempo - 30) / 270.0f;
    if (k == K_ROOT)
        return (float)set.root / 11.0f;
    if (k == K_FX + FX_DRIVE_TYPE)
        return set.fx[FX_DRIVE_TYPE] / (float)(FX_DRIVE_TYPES - 1);
    if (k == K_FX + FX_MOD_TYPE)
        return set.fx[FX_MOD_TYPE];
    if (k == K_FX + FX_DLY_TIME)
        return set.fx[FX_DLY_TIME] / (float)(FX_DLY_TIMES - 1);
    if (k >= K_FX && k < K_FX + FX_NPARAMS)
        return set.fx[k - K_FX];
    return 0.0f;
}
static void knob_text(int k, char *b)
{
    char t[12];
    float x = knob_value(k);
    b[0] = 0;
    if (k == AT_CUTOFF) {
        int_text((int32_t)(314.0f * fm_powf(2394.0f / 314.0f, x) + 0.5f), t);
        text_cat(b, t);
        text_cat(b, " HZ");
    } else if (k == AT_DECAY) {
        int_text((int32_t)(200.0f * fm_powf(12.5f, x) + 0.5f), t);
        text_cat(b, t);
        text_cat(b, " MS");
    } else if (k == AT_TUNING) {
        int_text((int32_t)(400.0f + 80.0f * x + 0.5f), t);
        text_cat(b, t);
        text_cat(b, " HZ");
    } else if (k == AT_WAVE)
        text_cat(b, x >= 0.5f ? "SQUARE" : "SAW");
    else if (k == AT_ENVMOD)
        pct_text(x * x * x, b);
    else if (k == K_STEPS)
        int_text(set.steps, b);
    else if (k == K_SCALE)
        text_cat(b, ACID_SCALES[set.scale].short_name);
    else if (k == K_TEMPO) {
        int_text(set.tempo, t);
        text_cat(b, t);
        text_cat(b, sq.ext ? " EXT" : " BPM");
    } else if (k == K_ROOT)
        text_cat(b, ACID_NOTE_NAMES[set.root]);
    else if (k == K_FX + FX_DRIVE_TYPE)
        text_cat(b, FX_DRIVE_NAME[(int)set.fx[FX_DRIVE_TYPE]]);
    else if (k == K_FX + FX_MOD_TYPE)
        text_cat(b, FX_MOD_NAME[(int)set.fx[FX_MOD_TYPE]]);
    else if (k == K_FX + FX_DLY_TIME)
        text_cat(b, FX_DLY_NAME[(int)set.fx[FX_DLY_TIME]]);
    else if (k == K_FX + FX_MOD_RATE) {        /* tenths of a hertz: "0.3 HZ", "6.4 HZ" */
        int32_t d = (int32_t)(0.5f * fm_powf(2.0f, 7.0f * x) + 0.5f);
        int_text(d / 10, t);
        text_cat(b, t);
        text_cat(b, ".");
        int_text(d % 10, t);
        text_cat(b, t);
        text_cat(b, " HZ");
    } else if (k == K_FX + FX_REV_PRE) {
        int_text((int32_t)(50.0f * x + 0.5f), t);
        text_cat(b, t);
        text_cat(b, " MS");
    } else
        pct_text(x, b);
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void knob_turn(int k, int32_t det)
{
    int32_t v;
    float step = det > 1 || det < -1 ? 0.02f : 0.01f;
    if (k >= 0 && k < AT_NPARAMS) {
        float x = set.param[k];
        x = k == AT_WAVE ? (det > 0 ? 1.0f : 0.0f) : fm_clampf(x + (float)det * step, 0.0f, 1.0f);
        if (x == set.param[k])
            return;
        set.param[k] = x;
        post(EV_PARAM, (uint32_t)k, (uint32_t)(x * 1000.0f + 0.5f));
    } else if (k >= K_SQ && k < K_SQ + SQ_NKNOBS) {
        float *x = &set.seqk[k - K_SQ];
        float y = fm_clampf(*x + (float)det * step, k == K_SQ + SQ_GATE ? 0.05f : 0.0f, 1.0f);
        if (y == *x)
            return;
        *x = y;
        if (k != K_SQ + SQ_GATE)
            ui.regrow_at = plat_ms() + 120u;   /* regrow from the same seed once the knob rests */
    } else if (k == K_STEPS) {
        v = set.steps + det;
        set.steps = (uint8_t)(v < 1 ? 1 : v > 16 ? 16 : v);
    } else if (k == K_SCALE) {
        v = set.scale + (det > 0 ? 1 : -1);
        set.scale = (uint8_t)(v < 0 ? 0 : v >= ACID_SCALES_LEN ? ACID_SCALES_LEN - 1 : v);
    } else if (k == K_TEMPO) {
        v = set.tempo + det;
        set.tempo = (uint16_t)(v < 30 ? 30 : v > 300 ? 300 : v);
    } else if (k == K_ROOT) {
        v = (set.root + det % 12 + 12) % 12;
        set.root = (uint8_t)v;
    } else if (k == K_FX + FX_DRIVE_TYPE || k == K_FX + FX_MOD_TYPE || k == K_FX + FX_DLY_TIME) {
        int p = k - K_FX;
        int top = p == FX_DRIVE_TYPE ? FX_DRIVE_TYPES - 1 : p == FX_MOD_TYPE ? FX_MOD_TYPES - 1 : FX_DLY_TIMES - 1;
        v = (int32_t)set.fx[p] + (det > 0 ? 1 : -1);
        v = v < 0 ? 0 : v > top ? top : v;
        if ((float)v == set.fx[p])
            return;
        set.fx[p] = (float)v;
        post(EV_FX, (uint32_t)p, (uint32_t)(v * 1000));
    } else if (k >= K_FX && k < K_FX + FX_NPARAMS) {
        int p = k - K_FX;
        float y = fm_clampf(set.fx[p] + (float)det * step, 0.0f, 1.0f);
        if (y == set.fx[p])
            return;
        set.fx[p] = y;
        post(EV_FX, (uint32_t)p, (uint32_t)(y * 1000.0f + 0.5f));
    }
    set_dirty = 1;
    ui.redraw = 1;
}

static void octave_step(int32_t d)
{
    int32_t o = set.octave + d;
    char t[4];
    if (o < -2 || o > 2)
        return;
    post(EV_PANIC, 0, 0);
    set.octave = (int8_t)o;
    set_dirty = 1;
    t[0] = o < 0 ? '-' : '+';
    t[1] = (char)('0' + (o < 0 ? -o : o));
    t[2] = 0;
    app_say("OCTAVE", t);
}

static void preset_say(int i, const char *what)
{
    char t[24];
    t[0] = (char)('0' + (i + 1) / 10);
    t[1] = (char)('0' + (i + 1) % 10);
    t[2] = ' ';
    t[3] = 0;
    text_cat(t, presets[i].name);
    app_say(what, t);
}
/* slot -1: a new preset in the next free slot (all full: the one after the last saved, over it) */
static void preset_save(int slot)
{
    int i, bank, ok;
    if (slot < 0 || !presets[slot].used) {
        if (slot < 0) {
            for (i = 0; i < NPRESETS && presets[i].used; i++)
                ;
            slot = i < NPRESETS ? i : (cur_preset + 1) % NPRESETS;
        }
        name_rng ^= plat_ms() * 2654435761u;   /* when SAVE was pressed: a different name each time */
        if (!name_rng)
            name_rng = 1u;
        acid_name(presets[slot].name);
    }
    presets[slot].s = set;
    presets[slot].used = 1;
    bank = slot / PRESETS_PER_OBJ;
    ok = plat_store_save((uint32_t)(OBJ_PRESETS_A + bank), &presets[bank * PRESETS_PER_OBJ],
                         sizeof(preset_t) * PRESETS_PER_OBJ) == 0;
    ok = ok && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0;   /* and the state at power-on */
    if (!ok) {
        app_say("SAVE", "FAILED");
        return;
    }
    cur_preset = (int8_t)slot;
    set_dirty = 0;
    preset_say(slot, "SAVED");
}
/* the PRESETS encoder: the first turn opens the list on the preset loaded; then each detent moves the cursor
 * over the 32 slots, and a saved preset under it loads at once */
static void preset_step(int32_t d)
{
    if (!ui.list) {
        ui.list = 1;
        ui.list_cur = (int8_t)(cur_preset >= 0 ? cur_preset : 0);
    } else {
        int c = ((ui.list_cur + d) % NPRESETS + NPRESETS) % NPRESETS;
        ui.list_cur = (int8_t)c;
        if (presets[c].used) {
            cur_preset = (int8_t)c;
            post(EV_LOAD, (uint32_t)c, 0);
            set_dirty = 0;
        }
    }
    ui.del_t = 0;
    ui.list_until = plat_ms() + 6000u;
    ui.redraw = 1;
}
static void preset_delete(int slot)
{
    int bank = slot / PRESETS_PER_OBJ;
    uint32_t i;
    presets[slot].used = 0;
    for (i = 0; i < sizeof presets[slot].name; i++)
        presets[slot].name[i] = 0;
    if (plat_store_save((uint32_t)(OBJ_PRESETS_A + bank), &presets[bank * PRESETS_PER_OBJ],
                        sizeof(preset_t) * PRESETS_PER_OBJ) != 0) {
        app_say("DELETE", "FAILED");
        return;
    }
    if (cur_preset == slot)
        cur_preset = -1;
    app_say("DELETED", "");
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            uint32_t v = touch_vel(k);
            if (ui.edit) {                     /* the editor: the key is the step's note; on to the next step */
                int32_t base = 36 + 12 * (set.octave + 1) + set.root;
                post(EV_EDIT, ui.cursor, (uint32_t)ED_NOTE | (uint32_t)(note - base + 64) << 8);
                if (!sq.running)
                    post(EV_NOTE_ON, (uint32_t)note, v);   /* and hear it */
                ui.cursor = (uint8_t)((ui.cursor + 1u) % (uint32_t)seq_len());
                set_dirty = 1;
                ui.redraw = 1;
            } else if (sq.running) {           /* transpose: the key is the new root */
                set.root = (uint8_t)(note % 12);
                set_dirty = 1;
                app_say("ROOT", ACID_NOTE_NAMES[set.root]);
            } else
                post(EV_NOTE_ON, (uint32_t)note, v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            if (!sq.running)
                post(EV_NOTE_OFF, (uint32_t)note, 0);   /* (in the editor too: the note heard ends) */
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB);
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (ui.list && k == B_ENV) {           /* delete: twice, within two seconds */
            int c = ui.list_cur;
            ui.list_until = plat_ms() + 6000u;
            if (!presets[c].used)
                app_say("EMPTY", "");
            else if (ui.del_t && plat_ms() - ui.del_t < 2000u) {
                ui.del_t = 0;
                preset_delete(c);
            } else {
                ui.del_t = plat_ms() | 1u;
                app_say("ENV AGAIN", "TO DELETE");
            }
            ui.redraw = 1;
        } else if (ui.list && k == B_SAVE && !presets[ui.list_cur].used) {
            preset_save(ui.list_cur);          /* an empty slot of the list: saved right there */
            ui.list_until = plat_ms() + 6000u;
            ui.redraw = 1;
        } else if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_PLAY)
            post(EV_PLAY, 0, 0);
        else if (k == B_FX) {                  /* the effect pages: in, then the next one */
            set.page = (uint8_t)(set.page < 4u || set.page == NPAGES - 1u ? 4u : set.page + 1u);
            set_dirty = 1;
            ui.redraw = 1;
        }
        else if (k == B_SEQ) {
            post(EV_GENERATE, 0, 0);
            set_dirty = 1;
        } else if (k == B_EDIT) {
            ui.edit = (uint8_t)!ui.edit;
            if (ui.cursor >= seq_len())
                ui.cursor = 0;
            ui.redraw = 1;
        } else if (ui.edit && (k == B_ENV || k == B_LFO)) {
            post(EV_EDIT, ui.cursor, k == B_ENV ? ED_ACC : ED_SLIDE);
            set_dirty = 1;
        } else if (k == B_ENV || k == B_LFO || k == B_ARP) {
            post(EV_MUTATE, k == B_ENV ? ACID_MUT_TIME : k == B_LFO ? ACID_MUT_PITCH : ACID_MUT_ARTICULATION, 0);
            set_dirty = 1;
        } else if (k == B_HOME) {              /* straight to page 1 (CUTOFF), out of the editor and the list */
            set.page = 0;
            ui.edit = 0;
            ui.list = 0;
            set_dirty = 1;
            ui.redraw = 1;
        } else if (k == B_REC)
            post(EV_UNDO, 0, 0);
        else if (k == B_GLO) {
            set.locked = (uint8_t)!set.locked;
            set_dirty = 1;
            app_say("LOCK", set.locked ? "ON: MUTATE" : "OFF: NEW SEED");
        } else if (k == B_SAVE)
            ui.save_t = plat_ms() | 1u;        /* acts on release (a new preset) or after a second held (over) */
    }
    if (ui.save_t) {                           /* SAVE: a tap saves a new preset, a second held saves over the loaded one */
        if (btn >> B_SAVE & 1u) {
            if (plat_ms() - ui.save_t >= 1000u) {
                ui.save_t = 0;
                preset_save(cur_preset >= 0 ? cur_preset : -1);
            }
        } else {
            ui.save_t = 0;
            preset_save(-1);
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (k == EN_PRESET) {                  /* through the saved presets, loading each */
            preset_step(e);
            continue;
        }
        if (ui.edit && (k == EN_SELECT || k == EN_K1)) {
            int32_t len = seq_len(), c = ((int32_t)ui.cursor + e) % len;
            ui.cursor = (uint8_t)(c < 0 ? c + len : c);
            ui.redraw = 1;
        } else if (ui.edit && k >= EN_K2 && k <= EN_K4) {
            post(EV_EDIT, ui.cursor, (uint32_t)(k == EN_K2 ? ED_OCT : k == EN_K3 ? ED_GATE : ED_SEMI) | (e > 0 ? 1u : 0u) << 8);
            set_dirty = 1;
        } else if (k == EN_SELECT) {
            set.page = (uint8_t)((set.page + (e > 0 ? 1u : NPAGES - 1u)) % NPAGES);
            set_dirty = 1;
            ui.redraw = 1;
        } else if (k >= EN_K1 && k <= EN_K4) {
            int kn = PAGE_KNOB[set.page][k - EN_K1];
            if (kn != K_NONE)
                knob_turn(kn, e);
        }
    }
    if (ui.regrow_at && plat_ms() >= ui.regrow_at) {
        ui.regrow_at = 0;
        post(EV_REGROW, 0, 0);
    }
    if (ui_event) {                            /* what a pattern button did */
        static const char *const NAME[] = {"", "", "", "", "", "GENERATE", "MUTATE", "UNDO"};
        uint8_t ev = ui_event - 1u;
        ui_event = 0;
        if (ev < 8u)
            app_say(NAME[ev], ui_event_ok ? "OK" : "NOTHING");
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u) | (sq.running ? 1u << B_PLAY : 0u)
                  | (set.locked ? 1u << B_GLO : 0u) | (ui.edit ? 1u << B_EDIT : 0u),
              keys);
}

void app_input_only(void) { input(); }

static void note_name(int32_t note, char *b)
{
    char o[12];
    b[0] = 0;
    text_cat(b, ACID_NOTE_NAMES[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

/* the pattern strip: a cell a step, as the SEQ module's LEDs (green note, blue tie, amber slide,
 * now in the acid editor's colours of the VCV modules: blue note, dark blue tie, pale accent, cyan slide), the step
 * under way framed white */
static void draw_pattern(int32_t y)
{
    int32_t i, len = seq_len(), cur = sq.step;
    const acid_grid_t *g = &sq.grid;
    for (i = 0; i < 16; i++) {
        int32_t x = 1 + i * 15;
        uint16_t c = C_LINE;
        if (i < len && g->gate[i]) {
            if (g->tie[i])
                c = AK_TIE;
            else if (g->accent[i])
                c = AK_ACCENT;
            else if (g->slide[i])
                c = AK_SLIDE;
            else
                c = AK_NOTE;
        } else if (i >= len)
            c = C_BLACK;
        if (sq.running && i == cur)
            cv_rect(x - 1, y - 1, 15, 22, C_WHITE);
        cv_rect(x, y, 13, 20, c);
    }
}

/* the page navigator at the foot: five tabs at a time around the page, the page boxed, arrows where there are more */
static const char *const PAGE_TAB[NPAGES] = {"VCF", "VOICE", "PATT", "SEQ", "DELAY", "CHOR", "DRIVE", "REVRB"};
static void draw_nav(int32_t y)
{
    int32_t first = (int32_t)set.page - 2, k;
    const int32_t TABS = 5, W = 44, X0 = 10, H = 20;
    if (first > NPAGES - TABS)
        first = NPAGES - TABS;
    if (first < 0)
        first = 0;
    cv_rect(0, y, 240, 1, C_LOGO);               /* the frame */
    cv_rect(0, y + H + 1, 240, 1, C_LOGO);
    cv_rect(0, y, 1, H + 2, C_LOGO);
    cv_rect(239, y, 1, H + 2, C_LOGO);
    for (k = 0; k < TABS; k++) {
        int32_t i = first + k, x = X0 + k * W, tw = text_w(&FONT_XS, PAGE_TAB[i]);
        if (i == set.page) {
            cv_rect(x, y + 3, W - 4, H - 4, C_LOGO);
            cv_text(x + (W - 4 - tw) / 2, y + 4, &FONT_XS, PAGE_TAB[i], C_WHITE);
        } else
            cv_text(x + (W - 4 - tw) / 2, y + 4, &FONT_XS, PAGE_TAB[i], C_HI);
    }
    for (k = 0; k < 4; k++) {                    /* the arrows: small triangles */
        if (first > 0)
            cv_rect(6 - k, y + 7 + k, 1, 8 - 2 * k, C_LOGO);
        if (first + TABS < NPAGES)
            cv_rect(233 + k, y + 7 + k, 1, 8 - 2 * k, C_LOGO);
    }
}

/* the step editor: a piano roll of one octave (the pitch class, root included), the octave in the UP and
 * DOWN rows below, then GATE, ACC and SLIDE, as on a 303's own pattern sheet */
#define ED_X 30                                /* the grid's left edge; 16 columns of 13 */
#define ED_W 13
static int32_t step_pc(const acid_grid_t *g, int st)
{
    int semi = acid_semi_of(g->deg[st], g->oct[st], set.scale) + set.root;
    return ((semi % 12) + 12) % 12;
}
static int32_t step_oct(const acid_grid_t *g, int st)
{
    int semi = acid_semi_of(g->deg[st], g->oct[st], set.scale) + set.root;
    return (semi - (((semi % 12) + 12) % 12)) / 12;
}
static void draw_editor(void)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    static const char *const ROW[5] = {"UP", "DOWN", "GATE", "ACC", "SLIDE"};
    const acid_grid_t *g = &sq.grid;
    int32_t i, r, len = seq_len(), cur = ui.cursor, play = sq.running ? sq.step : -1;
    const uint16_t NOTE = AK_NOTE, ACC = AK_ACCENT, TIE = AK_TIE;
    const uint16_t SLIDE = AK_SLIDE, ROWA = RGB(0x16, 0x18, 0x1C), ROWB = RGB(0x22, 0x25, 0x2B);
    /* the roll: 12 rows of 8 px, B at the top */
    cv_begin(240, 100, C_BLACK);
    for (r = 0; r < 12; r++) {
        int32_t y = 2 + (11 - r) * 8;
        cv_rect(2, y, 26, 7, BLACK[r] ? RGB(20, 20, 24) : RGB(200, 200, 206));
        if (r == 0)
            cv_text(17, y - 3, &FONT_T, "C", C_BLACK);
        cv_rect(ED_X, y, 16 * ED_W, 7, BLACK[r] ? ROWA : ROWB);
    }
    for (i = 4; i < 16; i += 4)
        cv_rect(ED_X + i * ED_W - 1, 2, 1, 96, C_DIM);
    for (i = 0; i < 16; i++) {
        int32_t x = ED_X + i * ED_W, y;
        if (i >= len) {
            cv_rect(x, 2, ED_W, 96, C_BLACK);
            continue;
        }
        if (i == cur) {                        /* the cursor's column, lit */
            for (r = 0; r < 12; r++)
                cv_rect(x, 2 + (11 - r) * 8, ED_W, 7, BLACK[r] ? RGB(44, 50, 72) : RGB(58, 66, 92));
            cv_rect(x, 0, ED_W, 1, C_WHITE);
            cv_rect(x, 99, ED_W, 1, C_WHITE);
        }
        if (!g->gate[i])
            continue;
        y = 2 + (11 - step_pc(g, i)) * 8;
        cv_rect(x + 1, y + 1, ED_W - 2, 5, g->tie[i] ? TIE : g->accent[i] ? ACC : NOTE);
        if (g->slide[i] && i + 1 < len && g->gate[i + 1]) {   /* the slide: a line to the next note */
            int32_t y2 = 2 + (11 - step_pc(g, i + 1)) * 8;
            cv_line(x + ED_W - 2, y + 3, x + ED_W + ED_W / 2, y2 + 3, SLIDE);
        }
    }
    if (play >= 0 && play < 16)
        cv_rect(ED_X + play * ED_W, 1, ED_W, 1, C_HI);
    cv_blit(0, 32);
    /* the five rows, and what the controls do */
    cv_begin(240, 108, C_BLACK);
    for (r = 0; r < 5; r++) {
        int32_t y = 4 + r * 16;
        cv_text(2, y + 1, &FONT_T, ROW[r], C_GRAY);
        for (i = 0; i < 16; i++) {
            int32_t x = ED_X + i * ED_W, oc = step_oct(g, i);
            uint16_t c = RGB(28, 30, 40);
            int on = i < len && g->gate[i];
            if (i >= len)
                c = C_BLACK;
            else if (r == 0 && on && oc > 0)
                c = oc > 1 ? ACC : AK_OCTAVE;
            else if (r == 1 && on && oc < 0)
                c = oc < -1 ? ACC : AK_OCTAVE;
            else if (r == 2 && on)
                c = g->tie[i] ? TIE : NOTE;
            else if (r == 3 && on && g->accent[i])
                c = ACC;
            else if (r == 4 && on && g->slide[i]) {   /* dim: set, but into a rest or a tie it does nothing */
                int nx = (i + 1) % len;
                c = g->gate[nx] && !g->tie[nx] ? SLIDE : RGB(0, 70, 100);
            }
            cv_rect(x + 1, y, ED_W - 2, 13, c);
            if (i == cur)
                cv_rect(x, y + 14, ED_W, 1, C_WHITE);
        }
    }
    cv_text(2, 86, &FONT_T, "K1 STEP  K2 UP/DN  K3 GATE  K4 NOTE", C_DIM);
    cv_text(2, 97, &FONT_T, "KEY NOTE  ENV ACC  LFO SLIDE  REC UNDO", C_DIM);
    cv_blit(0, 132);
}

/* the preset list: 8 rows of the 32 slots around the cursor; the one loaded in colour, the cursor lit */
static void draw_list(void)
{
    int32_t top = ui.list_cur - 3, r, band;
    char t[24];
    if (top < 0)
        top = 0;
    if (top > NPRESETS - 8)
        top = NPRESETS - 8;
    for (band = 0; band < 2; band++) {
        cv_begin(240, band ? 124 : 84, C_BLACK);   /* the whole area: 32 .. 240 */
        for (r = band ? 4 : 0; r < (band ? 8 : 4); r++) {
            int32_t i = top + r, y = (r - (band ? 4 : 0)) * 20 + (band ? 0 : 4);
            int cur = i == ui.list_cur;
            if (cur)
                cv_rect(4, y, 232, 19, RGB(0x22, 0x25, 0x2B));
            t[0] = (char)('0' + (i + 1) / 10);
            t[1] = (char)('0' + (i + 1) % 10);
            t[2] = 0;
            cv_text(10, y + 2, &FONT_B, t, cur ? C_WHITE : C_DIM);
            if (presets[i].used)
                cv_text(40, y + 2, &FONT_B, presets[i].name, i == cur_preset ? C_HI : cur ? C_WHITE : C_GRAY);
            else
                cv_text(40, y + 3, &FONT_XS, "- EMPTY -", C_DIM);
        }
        if (band) {
            cv_text(4, 84, &FONT_T, "PRESETS MOVE  ENV x2 DELETE  SAVE HERE", C_DIM);
            cv_text(4, 94, &FONT_T, "HOME CLOSE", C_DIM);
        }
        cv_blit(0, band ? 116 : 32);
    }
}

static void draw(void)
{
    char t[24];
    int32_t note = ui_note, i;
    uint32_t now = plat_ms();
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(10, 6, &FONT_B, ui.list ? "PRESETS" : ui.edit ? "EDIT" : cur_preset >= 0 ? presets[cur_preset].name : APP_NAME, C_WHITE);
        if (ui.edit) {                         /* the step under the cursor: number, note, accent, slide */
            const acid_grid_t *gg = &sq.grid;
            int st = ui.cursor;
            t[0] = 0;
            text_cat(t, "STEP ");
            {
                char nn[4];
                int_text(st + 1, nn);
                text_cat(t, nn);
            }
            x = cv_text(x + 8, 8, &FONT_XS, t, C_GRAY);
            t[0] = 0;
            if (gg->gate[st]) {
                note_name(36 + 12 * (set.octave + 1) + set.root + acid_semi_of(gg->deg[st], gg->oct[st], set.scale), t);
                if (gg->tie[st])
                    text_cat(t, " TIE");
                if (gg->accent[st])
                    text_cat(t, " ACC");
                if (gg->slide[st])
                    text_cat(t, " SLD");
            } else
                text_cat(t, "REST");
            cv_text(232 - text_w(&FONT_XS, t), 8, &FONT_XS, t, C_HI);
            note = -1;
        } else
            cv_text(x + 8, 8, &FONT_XS, sq.running ? "PLAY" : "", C_HI);
        if (note >= 0) {
            note_name(note, t);
            cv_text(230 - text_w(&FONT_B, t), 6, &FONT_B, t, C_HI);
        }
    }
    cv_blit(0, 0);
    if (ui.list) {
        draw_list();
        return;
    }
    if (ui.edit) {
        draw_editor();
        return;
    }
    cv_begin(240, 138, C_BLACK);               /* 32 .. 170: no gap under the header */
    for (i = 0; i < 4; i++) {
        int kn = PAGE_KNOB[set.page][i];
        int32_t x = (i & 1) * 120, y = (i >> 1) * 68 + 2, w;
        if (kn == K_NONE)
            continue;
        cv_text(x + 8, y + 2, &FONT_XS, PAGE_NAME[set.page][i], C_GRAY);
        knob_text(kn, t);
        cv_text(x + 8, y + 18, &FONT_B, t, C_HI);
        cv_rect(x + 8, y + 46, 104, 6, C_LINE);
        w = (int32_t)(104.0f * knob_value(kn) + 0.5f);
        cv_rect(x + 8, y + 46, w, 6, C_LOGO);
    }
    cv_blit(0, 32);
    cv_begin(240, 70, C_BLACK);
    t[0] = 0;
    text_cat(t, set.locked ? "LOCK  " : "");
    text_cat(t, "OCT ");
    {
        char o[4];
        o[0] = set.octave < 0 ? '-' : '+';
        o[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
        o[2] = 0;
        text_cat(t, o);
    }
    cv_text(232 - text_w(&FONT_T, t), 0, &FONT_T, t, C_GRAY);
    draw_pattern(14);
    draw_nav(46);
    cv_blit(0, 170);
}

/* the first screen: the acid smiley in Animatek blue (art/splash-smiley.png, tools/make_splash.py), and the name */
#include "splash_img.h"
void app_splash(void)
{
    const uint16_t BG = RGB(33, 33, 33);
    uint32_t run = 0, left = 0, idx = 0, y0, rows;
    int32_t w;
    lcd_fill(0, 0, 240, 240, BG);
    for (y0 = 0; y0 < SPLASH_H; y0 += rows) {   /* in bands: the canvas holds 240 x 144 */
        uint32_t y, x;
        rows = SPLASH_H - y0 < 98u ? SPLASH_H - y0 : 98u;
        cv_begin(SPLASH_W, rows, BG);
        for (y = 0; y < rows; y++)
            for (x = 0; x < SPLASH_W; x++) {
                if (!left && run < SPLASH_RUNS) {
                    idx = SPLASH_RLE[2u * run];
                    left = SPLASH_RLE[2u * run + 1u];
                    run++;
                }
                if (left)
                    left--;
                cv_pset((int32_t)x, (int32_t)y, SPLASH_PAL[idx & 15u]);
            }
        cv_blit((240u - SPLASH_W) / 2u, 6u + y0);
    }
    cv_begin(240, 30, BG);
    w = text_w(&FONT_B, APP_NAME);
    cv_text((240 - w) / 2, 6, &FONT_B, APP_NAME, C_WHITE);
    cv_blit(0, 206);
}

void app_ui_init(void)
{
    animatek_palette();
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static int32_t last_note = -2;
    static uint8_t last_acc, last_slide, last_step = 255, last_run;
    static uint32_t last_seed, last_count, last_grid;
    uint32_t now = plat_ms();
    input();
    ann_tick(now);
    if (ui_note != last_note || ui_accent != last_acc || ui_slide != last_slide || sq.step != last_step
        || sq.running != last_run || set.pattern.seed != last_seed || set.mutation_counter != last_count || grid_version != last_grid) {
        last_grid = grid_version;
        last_note = ui_note;
        last_acc = ui_accent;
        last_slide = ui_slide;
        last_step = sq.step;
        last_run = sq.running;
        last_seed = set.pattern.seed;
        last_count = set.mutation_counter;
        ui.redraw = 1;
    }
    if (ui.list && now >= ui.list_until) {
        ui.list = 0;
        ui.redraw = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "note"))
        *got = ui_note;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "accent"))
        *got = ui_accent;
    else if (!strcmp(what, "slide"))
        *got = ui_slide;
    else if (!strcmp(what, "cutoff"))
        *got = (int)(set.param[AT_CUTOFF] * 100.0f + 0.5f);
    else if (!strcmp(what, "running"))
        *got = sq.running;
    else if (!strcmp(what, "step"))
        *got = sq.step;
    else if (!strcmp(what, "seed"))
        *got = (int)(set.pattern.seed & 0x7FFFFFFFu);
    else if (!strcmp(what, "mutations"))
        *got = (int)set.mutation_counter;
    else if (!strcmp(what, "root"))
        *got = set.root;
    else if (!strcmp(what, "tempo"))
        *got = set.tempo;
    else if (!strcmp(what, "drive_type"))
        *got = (int)set.fx[FX_DRIVE_TYPE];
    else if (!strcmp(what, "dly_mix"))
        *got = (int)(set.fx[FX_DLY_MIX] * 100.0f + 0.5f);
    else if (!strcmp(what, "edit"))
        *got = ui.edit;
    else if (!strcmp(what, "cursor"))
        *got = ui.cursor;
    else if (!strncmp(what, "gate", 4) && what[4])   /* gateN: step N rest 0, note 1, tie 2 */
        *got = sq.grid.gate[atoi(what + 4) & 15] ? (sq.grid.tie[atoi(what + 4) & 15] ? 2 : 1) : 0;
    else if (!strncmp(what, "acc", 3) && what[3])
        *got = sq.grid.accent[atoi(what + 3) & 15];
    else if (!strncmp(what, "sld", 3) && what[3])
        *got = sq.grid.slide[atoi(what + 3) & 15];
    else if (!strncmp(what, "semi", 4) && what[4])   /* semiN: step N, semitones from the root */
        *got = acid_semi_of(sq.grid.deg[atoi(what + 4) & 15], sq.grid.oct[atoi(what + 4) & 15], set.scale);
    else if (!strcmp(what, "scale"))
        *got = set.scale;
    else if (!strcmp(what, "list"))
        *got = ui.list;
    else if (!strcmp(what, "listcur"))
        *got = ui.list_cur;
    else if (!strcmp(what, "preset"))
        *got = cur_preset;
    else if (!strcmp(what, "npresets")) {
        int k, n = 0;
        for (k = 0; k < NPRESETS; k++)
            n += presets[k].used;
        *got = n;
    } else if (!strcmp(what, "locked"))
        *got = set.locked;
    else if (!strcmp(what, "notes"))
        *got = acid_semi_of(0, 0, 0) + set.pattern.pitch_length;
    else
        return 0;
    return 1;
}
#endif
