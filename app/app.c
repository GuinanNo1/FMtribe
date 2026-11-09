/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* FMtribe's app: the monotribe's panel on the FM-1. The pages follow its panel left to right (ENV the VCO, then VCF
 * VCA; LFO; ARP the drums; SEQ the step row on the keys, then the step editor; GLO MIDI, Bluetooth and lights), K1-K4
 * edit the page, SELECT is the VCO's wave (the step cursor on SEQ), ALGO the LFO's wave on LFO, OCT- / OCT+ the
 * octave, MASTER the volume, SAVE stores the settings and the pattern. The keys are played with velocity and pressure
 * (lib/touch.h). Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c
 * are in scope. */
#include "fastmath.h"
#include "tribe.h"
#include "seq.h"

/* ------------------------------------------------------------- the settings --- */
/* every setting the panel edits, by number: a knob's 0..127, or a switch's position (its OPT names) */
enum { P_CUTOFF, P_PEAK, P_WAVE, P_NOISE, P_EG, P_LEVEL, P_RHYTHM, P_LRATE, P_LINT, P_LTARGET, P_LMODE, P_LWAVE,
       P_SYNTH_CH, P_DRUM_CH, P_BLE, P_BD_DEC, P_SN_DEC, P_HH_DEC, P_KEYLIGHT, P_GLOW,
       P_TEMPO, P_PART, P_GATE, P_SWING, P_SYNC, P_CLKOUT, P_RANGE, P_MOTION, P_PREVIEW,
       NSAVED,                                   /* the settings saved; after them, what the knobs edit elsewhere: */
       P_STEPS16 = NSAVED,                       /* the pattern's 16-step synth */
       P_EDSTEP, P_EDNOTE, P_EDGATE, P_EDON,     /* the step editor: its slot, and that slot's note, gate, on */
       P_BLEDEV,                                 /* the Bluetooth MIDI device picked (lib/btdev) */
       NP };
enum { KL_OFF, KL_HELD, KL_GUIDE, NKL };   /* KEY LIGHT: none, the keys held, those and the page's key functions */
typedef struct {
    const char *name;
    uint8_t nopt;                        /* 0: a knob, 0..127 (or to hi); else a switch with this many positions */
    const char *const *opt;              /* a switch's positions, as the screen shows them */
    uint16_t def, lo, hi;                /* (lo and hi: a knob's range when not 0..127) */
} param_t;
static const char *const WAVE_OPT[TRIBE_NWAVE] = {"SAW", "TRI", "SQUARE"};
static const char *const EG_OPT[TRIBE_NEG] = {"DECAY", "GATE", "ATTACK"};
static const char *const TARGET_OPT[TRIBE_NTARGET] = {"VCO", "VCO+VCF", "VCF"};
static const char *const MODE_OPT[TRIBE_NMODE] = {"FAST", "SLOW", "1 SHOT"};
static const char *const LWAVE_OPT[TRIBE_NLWAVE] = {"SAW", "TRI", "SQUARE", "S&H"};
static const char *const CH_OPT[16] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
                                       "15", "16"};
static const char *const BLE_OPT[2] = {"ON", "OFF"};   /* (stored as "off", so 0, the default, is on) */
static const char *const KL_OPT[NKL] = {"OFF", "HELD", "GUIDE"};
static const char *const GLOW_OPT[3] = {"OFF", "LOW", "HIGH"};
static const char *const PART_OPT[SEQ_NPART] = {"SYNTH", "BD", "SN", "HH"};
static const char *const STEPS_OPT[2] = {"8", "16"};
static const char *const SYNC_OPT[2] = {"INT", "MIDI"};
static const char *const OFFON_OPT[2] = {"OFF", "ON"};
static const char *const RANGE_OPT[3] = {"WIDE", "NARROW", "KEY"};   /* the ribbon's: KEY (chromatic) for now */
static const param_t PAR[NP] = {
    {"CUTOFF", 0, 0, 80, 0, 0},
    {"PEAK", 0, 0, 30, 0, 0},
    {"WAVE", TRIBE_NWAVE, WAVE_OPT, TRIBE_SAW, 0, 0},
    {"NOISE", 0, 0, 0, 0, 0},
    {"EG", TRIBE_NEG, EG_OPT, TRIBE_DECAY, 0, 0},
    {"LEVEL", 0, 0, 100, 0, 0},
    {"RHYTHM", 0, 0, 90, 0, 0},
    {"RATE", 0, 0, 64, 0, 0},
    {"INT", 0, 0, 0, 0, 0},
    {"TARGET", TRIBE_NTARGET, TARGET_OPT, TRIBE_LFO_VCO, 0, 0},
    {"MODE", TRIBE_NMODE, MODE_OPT, TRIBE_LFO_SLOW, 0, 0},
    {"WAVE", TRIBE_NLWAVE, LWAVE_OPT, TRIBE_LFO_TRI, 0, 0},
    {"SYNTH CH", 16, CH_OPT, 0, 0, 0},        /* the MIDI channels: the synth's (in and out), the drums' */
    {"DRUM CH", 16, CH_OPT, 9, 0, 0},
    {"BLE MIDI", 2, BLE_OPT, 0, 0, 0},         /* Bluetooth MIDI on or off */
    {"BD DECAY", 0, 0, 32, 0, 0},              /* the drums' decays: 32 stock (the decay mods' pots, as a setting) */
    {"SN DECAY", 0, 0, 32, 0, 0},
    {"HH DECAY", 0, 0, 32, 0, 0},
    {"KEY LIGHT", NKL, KL_OPT, KL_GUIDE, 0, 0},
    {"GLOW", 3, GLOW_OPT, 1, 0, 0},            /* the unlit buttons' and the guide keys' glow */
    {"TEMPO", 0, 0, 120, 10, 600},       /* BPM: the monotribe's WIDE range */
    {"PART", SEQ_NPART, PART_OPT, SEQ_SYNTH, 0, 0},   /* the part the keys and the step editor edit */
    {"GATE", 0, 0, 64, 0, 0},                  /* a new note's gate time (127: tied) */
    {"SWING", 0, 0, 0, 0, 0},                  /* 0 straight .. 127 a triplet feel */
    {"SYNC", 2, SYNC_OPT, 0, 0, 0},            /* the internal tempo, or MIDI clock in */
    {"CLOCK OUT", 2, OFFON_OPT, 1, 0, 0},      /* MIDI clock, start and stop out */
    {"RANGE", 3, RANGE_OPT, 2, 0, 0},          /* the ribbon's range: KEY, or NARROW / WIDE as a fast / slow glide */
    {"MOTION", 2, OFFON_OPT, 1, 0, 0},         /* MOTION VOLUME played back */
    {"PREVIEW", 2, OFFON_OPT, 1, 0, 0},        /* DRUM PREVIEW: the part keys play their drum (the monotribe's Global 6) */
    {"STEPS", 2, STEPS_OPT, 0, 0, 0},          /* (the pattern's) */
    {"STEP", 0, 0, 0, 0, 15},
    {"NOTE", 0, 0, 60, 0, 0},
    {"GATE", 0, 0, 64, 0, 0},
    {"ON", 2, OFFON_OPT, 0, 0, 0},
    {"DEVICE", 0, 0, 0, 0, 0},
};

/* the pages: a button shows its page and K1..K4 edit it; pressed again, its next page */
typedef struct {
    const char *name;                    /* in the header and to a screen reader */
    const char *tab;                     /* on the tab (the button's first page's) */
    uint8_t button;                      /* plat.h B_* */
    int8_t p[4];                         /* the settings on K1..K4 (-1: none) */
} page_t;
/* the pages, after the monotribe's panel: ENV the VCO, then the VCF and VCA; LFO; ARP the drums; SEQ the step row (on
 * the keys) and the step editor; GLO the MIDI, Bluetooth and lights. SELECT is the VCO's WAVE (on SEQ, the step
 * cursor), ALGO the LFO's WAVE on LFO, OCT- / OCT+ the octave everywhere */
enum { PG_HOME, PG_HOME2, PG_VCO, PG_VCF, PG_LFO, PG_DRUMS, PG_SEQ, PG_EDIT, PG_GLOBAL, PG_GLOBAL2, PG_DEVICES, NPG };
static const page_t PAGE[NPG] = {
    /* HOME, the start page: the synth's and the drums' levels, and the filter */
    {"HOME", "HOME", B_HOME, {P_LEVEL, P_RHYTHM, P_CUTOFF, P_PEAK}},
    {"DECAY MOTION", "HOME", B_HOME, {P_BD_DEC, P_SN_DEC, P_HH_DEC, P_MOTION}},   /* (RANGE: on VCO) */
    {"VCO", "VCO", B_ENV, {P_NOISE, P_RANGE, -1, -1}},
    {"VCF VCA", "VCO", B_ENV, {P_CUTOFF, P_PEAK, P_LEVEL, P_EG}},
    {"LFO", "LFO", B_LFO, {P_LRATE, P_LINT, P_LTARGET, P_LMODE}},
    {"DRUMS", "DRUM", B_ARP, {P_RHYTHM, P_BD_DEC, P_SN_DEC, P_HH_DEC}},   /* and the black keys play them */
    /* the step row on the keys: the white keys are the steps (their lights the step LEDs), the black ones the part
     * buttons, ACTIVE STEP and GATE TIME; then a synth step's note, gate and on (the keys write its note) */
    {"STEP", "SEQ", B_SEQ, {P_TEMPO, P_SWING, P_STEPS16, P_GATE}},
    {"EDIT", "SEQ", B_SEQ, {P_EDSTEP, P_EDNOTE, P_EDGATE, P_EDON}},
    {"MIDI", "GLO", B_GLO, {P_SYNTH_CH, P_DRUM_CH, P_SYNC, P_CLKOUT}},
    {"GLOBAL", "GLO", B_GLO, {P_BLE, P_KEYLIGHT, P_GLOW, P_PREVIEW}},
    {"DEVICES", "GLO", B_GLO, {P_BLEDEV, -1, -1, -1}},   /* SAVE connects the pick, or lets it go (lib/btdev) */
};

#define SET_MAGIC 0x38544D46u            /* "FMT8": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +2: six positions, as the monotribe's 64' .. 2' */
    uint8_t rsv;
    uint16_t v[NSAVED];                  /* the settings above */
    seq_pattern_t pat;                   /* the sequencer's pattern, saved with them */
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

static const char *const DRUM_NAME[TRIBE_NDRUM] = {"BD", "SN", "HH"};
static const uint8_t DRUM_NOTE[TRIBE_NDRUM] = {36, 40, 42};   /* channel 10, the monotribe's (GM) */
/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const uint8_t WHITE[16] = PLAT_WHITE_KEYS;   /* the white keys, low to high: the sequencer's slots */
static int white_index(uint32_t k)       /* a key's place among the white keys, or -1 for a black one */
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE[i] == k)
            return i;
    return -1;
}

/* the settings' values: the saved ones, and what the knobs edit in the pattern and the device list */
static uint8_t ed_slot;                  /* the step cursor: the step editor's slot (SELECT moves it on SEQ) */
static uint8_t half_tempo;               /* v2's HALF TEMPO: the sequencer at half the tempo shown (not saved) */
static int pmax(int pn)
{
    if (pn == P_BLEDEV)
        return btdev_count() > 0 ? btdev_count() - 1 : 0;
    return PAR[pn].nopt ? PAR[pn].nopt - 1 : PAR[pn].hi ? PAR[pn].hi : 127;
}
static int pget(int pn)
{
    if (pn < NSAVED)
        return set.v[pn];
    switch (pn) {
    case P_STEPS16: return set.pat.steps16;
    case P_EDSTEP: return ed_slot;
    case P_EDNOTE: return set.pat.note[ed_slot];
    case P_EDGATE: return set.pat.gate[ed_slot];
    case P_EDON: return set.pat.on[set.v[P_PART]] >> ed_slot & 1u;   /* (the part picked: PRESET on SEQ) */
    case P_BLEDEV: return btdev_pick();
    }
    return 0;
}
static void pset(int pn, int v)
{
    if (pn < NSAVED) {
        set.v[pn] = (uint16_t)v;
        return;
    }
    switch (pn) {
    case P_STEPS16: set.pat.steps16 = (uint8_t)v; break;
    case P_EDSTEP: ed_slot = (uint8_t)v; break;
    case P_EDNOTE:                       /* (a drum has no note or gate: nothing) */
        if (set.v[P_PART] == SEQ_SYNTH)
            set.pat.note[ed_slot] = (uint8_t)v;
        break;
    case P_EDGATE:
        if (set.v[P_PART] == SEQ_SYNTH)
            set.pat.gate[ed_slot] = (uint8_t)v;
        break;
    case P_EDON:
        if (v)
            set.pat.on[set.v[P_PART]] |= (uint16_t)(1u << ed_slot);
        else
            set.pat.on[set.v[P_PART]] &= (uint16_t)~(1u << ed_slot);
        break;
    case P_BLEDEV: btdev_choose(v); break;
    }
}

static void engine_update(void)          /* the settings to the engine */
{
    tribe_params_t p;
    p.cutoff = set.v[P_CUTOFF] / 127.0f;
    p.peak = set.v[P_PEAK] / 127.0f;
    p.noise = set.v[P_NOISE] / 127.0f;
    p.level = set.v[P_LEVEL] / 127.0f;
    p.rhythm = set.v[P_RHYTHM] / 127.0f;
    p.lfo_rate = set.v[P_LRATE] / 127.0f;
    p.lfo_int = set.v[P_LINT] / 127.0f;
    p.wave = set.v[P_WAVE];
    p.eg = set.v[P_EG];
    p.lfo_target = set.v[P_LTARGET];
    p.lfo_mode = set.v[P_LMODE];
    p.lfo_wave = set.v[P_LWAVE];
    p.decay[0] = fm_exp2f(((float)set.v[P_BD_DEC] - 32.0f) / 24.0f);   /* 32 stock; 0 x0.4 .. 127 x15 */
    p.decay[1] = fm_exp2f(((float)set.v[P_SN_DEC] - 32.0f) / 24.0f);
    p.decay[2] = fm_exp2f(((float)set.v[P_HH_DEC] - 32.0f) / 24.0f);
    p.glide = set.v[P_RANGE] == 0 ? 0.3f : set.v[P_RANGE] == 1 ? 0.06f : 0.0f;   /* WIDE slow, NARROW fast: GUESSES */
    tribe_set(&p);
    seq_tempo((float)set.v[P_TEMPO] * (half_tempo ? 0.5f : 1.0f));
    seq_swing((float)set.v[P_SWING] / 127.0f);
    seq_ext(set.v[P_SYNC]);
    seq_clock_out(set.v[P_CLKOUT]);
}

static void param_text(int p, char *t);  /* (UI) */

/* ------------------------------------------------------------------ voice --- */
/* one voice: the keys keep a short stack (the newest sounds; let go, the one before it, without a new attack); MIDI in
 * plays it too (the audio interrupt). A new key or MIDI note always restarts the EG, as the monotribe's ribbon does. */
static volatile int32_t voice_note = -1;     /* MIDI note sounding, -1 = released */
static volatile int32_t bend_q;              /* channel 1 pitch bend, -8192..8191 (+-2 semitones) */
static int32_t held[8], nheld;               /* the keys held, oldest first (main loop) */
static volatile uint32_t drum_hits;          /* each drum's hit count (the screen lights its pad) */
static float gain_now;                       /* glides to MASTER: no step at a block's edge */
static float render_buf[APP_BLOCK];

static float note_pitch(int32_t note) { return (float)note + (float)bend_q * (2.0f / 8192.0f); }

static void note_on(int32_t note, uint32_t vel)
{
    (void)vel;                               /* the monotribe's VCA has no velocity */
    voice_note = note;
    tribe_note_on(note_pitch(note));
}
static void note_off(int32_t note)
{
    if (voice_note != note)
        return;
    voice_note = -1;
    tribe_note_off();
}
static void drum(int d)
{
    tribe_drum(d);
    drum_hits += 1u << (8 * d);
}

static void key_down(int32_t note, uint32_t vel)
{
    int32_t i, j;
    for (i = j = 0; i < nheld; i++)          /* (a key already listed moves to the top) */
        if (held[i] != note)
            held[j++] = held[i];
    nheld = j;
    if (nheld == 8) {
        for (i = 1; i < 8; i++)
            held[i - 1] = held[i];
        nheld = 7;
    }
    held[nheld++] = note;
    note_on(note, vel);
}
static void key_up(int32_t note)
{
    int32_t i, j, top = nheld ? held[nheld - 1] : -1;
    for (i = j = 0; i < nheld; i++)
        if (held[i] != note)
            held[j++] = held[i];
    nheld = j;
    if (note != top || voice_note != note)
        return;
    if (nheld) {                             /* back to the key before: the pitch alone */
        voice_note = held[nheld - 1];
        tribe_note_glide(note_pitch(voice_note));
    } else {
        note_off(note);
    }
}

/* --------------------------------------------------------------- sequencer --- */
/* what the sequencer plays (seq.h's callbacks, in the audio interrupt): the voice, and the notes out over MIDI */
static volatile int32_t seq_note = -1;       /* the sequencer's note sounding */
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    plat_midi_send((st >> 4) | st << 8 | d1 << 16 | d2 << 24, TO_USB | TO_BLE);
}
void seq_cb_note_on(uint8_t note)
{
    if (seq_note >= 0)
        midi_send(0x80u | set.v[P_SYNTH_CH], (uint32_t)seq_note, 0);
    seq_note = note;
    voice_note = note;
    tribe_note_on(note_pitch(note));
    midi_send(0x90u | set.v[P_SYNTH_CH], note, 100);
}
void seq_cb_note_off(void)
{
    if (seq_note < 0)
        return;
    midi_send(0x80u | set.v[P_SYNTH_CH], (uint32_t)seq_note, 0);
    if (voice_note == seq_note && !nheld)    /* (a key held since plays on) */
        note_off(seq_note);
    seq_note = -1;
}
void seq_cb_drum(int part)
{
    int d = part - SEQ_BD;
    drum(d);
    midi_send(0x90u | set.v[P_DRUM_CH], DRUM_NOTE[d], 100);
    midi_send(0x80u | set.v[P_DRUM_CH], DRUM_NOTE[d], 0);
}
void seq_cb_clock(uint8_t st) { plat_midi_send(0x0Fu | (uint32_t)st << 8, TO_USB | TO_BLE); }
static volatile int16_t motion_live = -1;    /* ACTIVE STEP and a key while it plays: the synth's level (-1 none) */
void seq_cb_motion(uint8_t vol)              /* a synth slot's MOTION VOLUME, with MOTION on (the live one wins) */
{
    if (motion_live >= 0)
        return;
    tribe_motion(set.v[P_MOTION] && vol != SEQ_NO_VOL ? (float)vol / 127.0f : -1.0f);
}

/* recording (main loop): REC armed and playing, a key's note or a pad's hit lands on the nearest slot it can play */
static uint8_t rec_armed;
static int8_t rec_slot = -1;                 /* the synth note being recorded: its slot, its key, when it started */
static int32_t rec_note;
static uint32_t rec_t0;
static uint8_t gate_hold;                /* v2's GATE TIME HOLD: recording pitch keeps the steps' recorded gates */
static uint8_t rec_keep;                 /* (the note being recorded keeps its step's gate) */
static int recording(void) { return rec_armed && seq_playing(); }
static void rec_note_on(int32_t note)
{
    int sl = seq_rec_slot(SEQ_SYNTH);
    if (sl < 0)
        return;
    set.pat.note[sl] = (uint8_t)note;
    rec_keep = (uint8_t)(gate_hold && (set.pat.on[SEQ_SYNTH] >> sl & 1u));   /* GATE TIME HOLD: a step's gate kept */
    if (!rec_keep)
        set.pat.gate[sl] = (uint8_t)set.v[P_GATE];
    set.pat.on[SEQ_SYNTH] |= (uint16_t)(1u << sl);
    rec_slot = (int8_t)sl;
    rec_note = note;
    rec_t0 = seq_now();
    set_dirty = 1;
}
static void rec_note_off(int32_t note)       /* the gate time as held: 1.4 % .. 100 % of the slot, longer is tied */
{
    float f;
    uint32_t len = set.pat.steps16 ? SEQ_TPS / 2u : SEQ_TPS;
    if (rec_slot < 0 || note != rec_note)
        return;
    if (rec_keep) {                          /* GATE TIME HOLD: the step's recorded gate kept */
        rec_slot = -1;
        return;
    }
    f = (float)(seq_now() - rec_t0) / (float)len;
    set.pat.gate[rec_slot] = (uint8_t)(f >= 1.0f ? SEQ_GATE_TIE : clamp((int32_t)((f - 0.014f) / 0.986f * 127.0f), 0, 126));
    rec_slot = -1;
}
static void rec_drum(int d)
{
    int sl = seq_rec_slot(SEQ_BD + d);
    if (sl >= 0) {
        set.pat.on[SEQ_BD + d] |= (uint16_t)(1u << sl);
        set_dirty = 1;
    }
}

static volatile uint32_t midi_last, midi_last_port, midi_count;   /* the last message in (shown), and how many */
static volatile uint32_t midi_knobs;         /* a CC moved a setting: the main loop redraws and marks it changed */
static void midi_in(void)                    /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port, d;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24, ch = st & 0x0Fu, ty = st & 0xF0u;
        if (st != 0xF8u && st != 0xFEu) {    /* (clock and active sensing would flood the screen) */
            midi_last = pkt;
            midi_last_port = (uint32_t)port;
            midi_count++;
        }
        if (st >= 0xF8u) {                   /* real time: the clock, start and stop, with SYNC on MIDI */
            seq_midi_rt((uint8_t)st);
            continue;
        }
        if (ch == set.v[P_DRUM_CH]) {        /* the drums' channel (10) */
            if (ty == 0x90u && d2)
                for (d = 0; d < TRIBE_NDRUM; d++)
                    if (d1 == DRUM_NOTE[d])
                        drum(d);
        } else if (ch == set.v[P_SYNTH_CH]) {   /* the synth's channel (1) */
            if (ty == 0x90u && d2)
                note_on((int32_t)d1, d2);
            else if (ty == 0x80u || ty == 0x90u)
                note_off((int32_t)d1);
            else if (ty == 0xE0u) {
                bend_q = (int32_t)(d1 | (d2 << 7)) - 8192;
                if (voice_note >= 0)
                    tribe_note_glide(note_pitch(voice_note));
            } else if (ty == 0xB0u) {        /* the monotribe's controllers; a switch: 32, 64, 96 (127: S&H) */
                uint8_t sw = (uint8_t)(d2 < 48u ? 0 : d2 < 80u ? 1 : d2 < 112u ? 2 : 3);
                int pn = d1 == 1u ? P_LINT : d1 == 16u ? P_LRATE : d1 == 7u ? P_LEVEL : d1 == 80u ? P_EG :
                         d1 == 81u ? P_LTARGET : d1 == 82u ? P_LMODE : d1 == 83u ? P_LWAVE : -1;
                if (pn >= 0) {
                    set.v[pn] = PAR[pn].nopt ? (sw < PAR[pn].nopt ? sw : (uint8_t)(PAR[pn].nopt - 1)) : (uint8_t)d2;
                    midi_knobs++;
                }
            }
        }
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;   /* MASTER, read once a block */
    midi_in();
    if (n > APP_BLOCK)
        n = APP_BLOCK;
    for (i = 0; i < n;) {                    /* the sound, cut where the sequencer's ticks and note offs fall */
        uint32_t k = seq_frames(n - i);
        tribe_render(render_buf + i, k);
        seq_advance(k);
        i += k;
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        gain_now += (gain - gain_now) * 0.004f;                  /* ~5 ms: turning MASTER does not click */
        s = (int32_t)(render_buf[i] * gain_now);
        lr[2 * i] = s;
        lr[2 * i + 1] = s;
    }
}

void app_panic(void)
{
    seq_play(0);
    voice_note = -1;
    nheld = 0;
    tribe_panic();
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    uint32_t i;
    set.magic = SET_MAGIC;
    set.octave = 0;
    for (i = 0; i < NSAVED; i++)
        set.v[i] = PAR[i].def;
    seq_clear(&set.pat, SEQ_CLEAR_ALL);
}

void app_init(void)
{
    uint32_t i, bad = 0;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.octave < -3 || set.octave > 2)
        bad = 1;
    for (i = 0; i < NSAVED; i++)
        bad |= set.v[i] < PAR[i].lo || (int)set.v[i] > pmax((int)i);
    for (i = 0; i < SEQ_SLOTS; i++)
        bad |= set.pat.note[i] > 127u || set.pat.gate[i] > 127u;
    if (bad)
        settings_defaults();
    seq_init(&set.pat);
    tribe_init();
    engine_update();
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t vel[NKEYS], prs[NKEYS];      /* each key's last velocity, and its pressure while held */
    int32_t last_key;                    /* the key pressed last: its numbers in the header */
    uint32_t key_synth;                  /* the keys held that play the synth (their release lets it go) */
    int32_t last_note;                   /* the last note a key played: a step switched on takes it */
    int32_t turns[NE];                   /* detents since boot, per encoder */
    uint32_t turned_at[NE];              /* when each was last turned: it shows lit for a moment */
    uint32_t drum_seen, drum_at[TRIBE_NDRUM];   /* the drums' hit counts drawn, and when each last hit */
    uint8_t page;                        /* PG_*: what K1..K4 edit */
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

static void int_text(int32_t v, char *b)     /* signed, no padding */
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
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void octave_text(char *t)
{
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
}

static void note_name(int32_t note, char *b);
static void param_text(int p, char *t)   /* a setting's value as the screen shows it */
{
    int v = pget(p);
    t[0] = 0;
    if ((p == P_EDNOTE || p == P_EDGATE) && set.v[P_PART] != SEQ_SYNTH)
        text_cat(t, "-");                    /* (a drum's step: a hit, no note or gate) */
    else if (PAR[p].nopt)
        text_cat(t, PAR[p].opt[v]);
    else if (p == P_EDNOTE)
        note_name(v, t);
    else if ((p == P_GATE || p == P_EDGATE) && v >= SEQ_GATE_TIE)
        text_cat(t, "TIE");
    else if (p == P_EDSTEP) {                /* step 3, or 3+: half-way through it */
        int_text(v / 2 + 1, t);
        if (v & 1)
            text_cat(t, "+");
    } else if (p == P_SWING) {               /* 50 % straight .. 67 % a triplet */
        int_text(50 + (v * 17 + 63) / 127, t);
        text_cat(t, "%");
    } else if (p == P_BLEDEV) {
        char n[21];
        if (btdev_found(v, n, 0)) {
            n[9] = 0;                        /* (a knob's slot is narrow) */
            text_cat(t, n);
        } else {
            text_cat(t, set.v[P_BLE] ? "BLE OFF" : "SCANNING");
        }
    } else
        int_text(v, t);
}

static uint32_t page_of(uint32_t pg, uint32_t *n)   /* a page's place among its button's pages (0-based), and how many */
{
    uint32_t i, k = 0;
    *n = 0;
    for (i = 0; i < NPG; i++)
        if (PAGE[i].button == PAGE[pg].button) {
            if (i < pg)
                k++;
            (*n)++;
        }
    return k;
}

static void page_title(uint32_t pg, char *t)  /* "LFO 1/2", "EG VCA 1/1" */
{
    uint32_t n, k = page_of(pg, &n);
    t[0] = 0;
    text_cat(t, PAGE[pg].name);
    {
        char d[4] = {' ', (char)('1' + k), '/', 0}, e[2] = {(char)('0' + n), 0};
        text_cat(t, d);
        text_cat(t, e);
    }
}

static void page_text(char *t)           /* the page and its settings, for a screen reader: "LFO 1/2. RATE 64. ..." */
{
    uint32_t i;
    char v[12];
    page_title(ui.page, t + strlen(t));
    for (i = 0; i < 4u; i++)
        if (PAGE[ui.page].p[i] >= 0) {
            text_cat(t, ". ");
            text_cat(t, PAR[PAGE[ui.page].p[i]].name);
            text_cat(t, " ");
            param_text(PAGE[ui.page].p[i], v);
            text_cat(t, v);
        }
}

/* a page button: its first page, or, already on it, its next */
static void page_button(uint32_t b)
{
    uint32_t i, first = NPG, next = NPG;
    char t[96];
    for (i = 0; i < NPG; i++)
        if (PAGE[i].button == b) {
            if (first == NPG)
                first = i;
            if (i > ui.page && next == NPG && PAGE[ui.page].button == b)
                next = i;
        }
    if (first == NPG)
        return;
    ui.page = (uint8_t)(next < NPG ? next : first);
    ui.redraw = 1;
    app_say(PAGE[ui.page].name, "");
    if (ann_on()) {                          /* the page's settings too */
        t[0] = 0;
        page_text(t);
        ann_say(t, 0, ANN_NOW);
    }
}

static int is_page_button(uint32_t b)
{
    uint32_t i;
    for (i = 0; i < NPG; i++)
        if (PAGE[i].button == b)
            return 1;
    return 0;
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[128], o[4];
    if (ann_on() && ann_where()) {
        octave_text(o);
        t[0] = 0;
        text_cat(t, APP_NAME ". OCTAVE ");
        text_cat(t, o);
        text_cat(t, ". ");
        page_text(t);
        if (midi_count) {                    /* the last MIDI message in, as the screen shows it */
            static const char *const PORT[NPORTS] = {"USB", "BLE", "TRS"};
            static const char HEX[] = "0123456789ABCDEF";
            uint32_t m = midi_last, b;
            char h[10];
            for (b = 0; b < 3u; b++) {
                uint32_t v = (m >> (8u + 8u * b)) & 0xFFu;
                h[3 * b] = HEX[v >> 4];
                h[3 * b + 1] = HEX[v & 15u];
                h[3 * b + 2] = b < 2u ? ' ' : 0;
            }
            text_cat(t, ". MIDI IN ");
            text_cat(t, midi_last_port < NPORTS ? PORT[midi_last_port] : "");
            text_cat(t, " ");
            text_cat(t, h);
        }
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void note_name(int32_t note, char *b)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[12];
    b[0] = 0;
    text_cat(b, N[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -3, 2);
    char t[4];
    if (o == set.octave)
        return;
    if (voice_note >= 0)                     /* a key held across the change would not find its note off */
        note_off(voice_note);
    nheld = 0;
    set.octave = (int8_t)o;
    set_dirty = 1;
    octave_text(t);
    app_say("OCTAVE", t);
}

int fmtribe_ble_off(void) { return set.v[P_BLE]; }

/* the step row on the keys (STEP): the white keys are the steps, the black ones the monotribe's buttons beside them:
 * F#3 G#3 A#3 C#4 the parts BD SN HH SYNTH, D#4 ACTIVE STEP, F#4 GATE TIME (held, with a white key) */
#define PART_KEYS (1u << 1 | 1u << 3 | 1u << 5 | 1u << 8)
static const uint8_t PART_KEY[SEQ_NPART] = {8, 1, 3, 5};
#define KEY_ACTIVE 10u
#define KEY_GATE 13u
static uint32_t keys_now;                /* the keys held this scan (a white key reads ACTIVE STEP, GATE TIME, a part) */
static int16_t gate_val = -1;            /* GATE TIME played: the gate every synth note gets (-1: the pattern's) */
static uint8_t lwave_prev = TRIBE_LFO_TRI;   /* the LFO wave before v2's SAMPLE & HOLD */
static int n16(int part) { return part == SEQ_SYNTH && set.pat.steps16; }   /* the part on 16 keys (else 8) */
static int slot_key(int part, int sl) { return n16(part) ? sl : sl / 2; }   /* the white key showing a slot */

/* the lights: the held buttons, the page's button, SAVE (something to save) and REC (armed) lit, PLAY green while it
 * plays (red recording), the others glowing (GLOW); the keys by KEY LIGHT: none, the held ones, or those and the
 * page's key functions (GUIDE): the DRUMS page's pads glow; on STEP the white keys are the step LEDs: dark, a step
 * switched off (ACTIVE STEP) or past the 8; glowing, an active step with nothing on it; half-lit, one with a note or
 * hit; lit, the step playing (stopped: the synth's step cursor). ACTIVE STEP held, the active steps lit. The part
 * keys glow, the part's lit; ACTIVE STEP and GATE TIME glow (GATE TIME lit while it holds) */
static uint32_t lit_keys_now, dim_keys_now, mid_keys_now;   /* (the host's checks) */
static void lights(uint32_t btn, uint32_t keys)
{
    uint32_t b = btn | 1u << PAGE[ui.page].button | (set_dirty ? 1u << B_SAVE : 0u) | (rec_armed ? 1u << B_REC : 0u),
             lit = 0, dim = 0, mid = 0, i;
    int kl = set.v[P_KEYLIGHT], glow = set.v[P_GLOW], part = set.v[P_PART];
    if (seq_playing())
        b |= rec_armed ? 1u << B_PLAY : LED_PLAY_GREEN;
    if (kl >= KL_HELD)
        lit = keys;
    if (kl == KL_GUIDE && ui.page == PG_SEQ) {
        int nk = n16(part) ? 16 : 8, sl = seq_slot(part);
        if (keys >> KEY_ACTIVE & 1u) {       /* ACTIVE STEP held: the steps that play */
            for (i = 0; i < SEQ_STEPS; i++)
                if (set.pat.active[part] >> i & 1u)
                    lit |= 1u << WHITE[i];
                else
                    dim |= 1u << WHITE[i];
        } else {
            for (i = 0; i < (uint32_t)nk; i++) {
                uint32_t st = n16(part) ? i / 2u : i, m = n16(part) ? 1u << i : part == SEQ_SYNTH ? 1u << (2u * i)
                                                                                                  : 3u << (2u * i);
                if (!(set.pat.active[part] >> st & 1u))
                    continue;
                if (set.pat.on[part] & m)
                    mid |= 1u << WHITE[i];
                else
                    dim |= 1u << WHITE[i];
            }
            if (sl >= 0 && !(part == SEQ_SYNTH && !set.pat.steps16 && (sl & 1)))
                lit |= 1u << WHITE[slot_key(part, sl)];
            else if (sl < 0 && part == SEQ_SYNTH)
                lit |= 1u << WHITE[slot_key(part, ed_slot)];
        }
        dim |= PART_KEYS | 1u << KEY_ACTIVE | 1u << KEY_GATE;
        lit |= 1u << PART_KEY[part] | (gate_hold ? 1u << KEY_GATE : 0u);
    }
    lit_keys_now = lit;
    dim_keys_now = dim & ~lit & ~mid;
    mid_keys_now = mid & ~lit;
    if (glow == 0 && (dim | mid)) {          /* the guide wants a glow: the keys' then, but not the buttons' */
        plat_lights(b, lit, 0, dim & ~lit & ~mid, 1);
    } else {
        plat_lights(b, lit, glow ? ~b & ((1u << NB) - 1u) : 0, dim & ~lit & ~mid, glow);
    }
    plat_lights_mid(0, mid & ~lit);
}

/* the keys: what a press does depends on the page; the synth's keys are let go as they were pressed */
static void synth_key(uint32_t k, int32_t note, uint32_t v)
{
    ui.vel[k] = (uint8_t)v;
    ui.prs[k] = 0;
    ui.last_key = (int32_t)k;
    ui.last_note = note;
    ui.key_synth |= 1u << k;
    key_down(note, v);
    plat_midi_send(0x09u | (0x90u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);
    if (recording())
        rec_note_on(note);
}

static void pad(int d)                   /* a drum played from its part key while REC is on: heard, sent, recorded */
{
    drum(d);
    plat_midi_send(0x09u | (0x90u | set.v[P_DRUM_CH]) << 8 | (uint32_t)DRUM_NOTE[d] << 16 | 100u << 24, TO_USB | TO_BLE);
    plat_midi_send(0x08u | (0x80u | set.v[P_DRUM_CH]) << 8 | (uint32_t)DRUM_NOTE[d] << 16, TO_USB | TO_BLE);
    if (recording())
        rec_drum(d);
}

static void step_say(const char *what, int slot)   /* "STEP 3+" "ON" */
{
    char t[24] = "STEP ", n[8];
    int_text(slot / 2 + 1, n);
    text_cat(t, n);
    if (slot & 1)
        text_cat(t, "+");
    app_say(t, what);
}

static void gate_text(int g, char *t)    /* a gate time as a percentage of the step: 1.4 % .. 100 % */
{
    t[0] = 0;
    int_text(g >= SEQ_GATE_TIE ? 100 : (14 + 986 * g / 127 + 5) / 10, t);
    text_cat(t, "%");
}

static void motion_rec(int v)             /* MOTION VOLUME into the synth's slot playing now (its step, in 8-step mode) */
{
    int sl = seq_slot(SEQ_SYNTH);
    if (sl >= 0 && !set.pat.steps16)
        sl &= ~1;
    if (sl >= 0 && set.pat.vol[sl] != (uint8_t)v) {
        set.pat.vol[sl] = (uint8_t)v;
        set_dirty = 1;
    }
}

static void gate_off(void)               /* GATE TIME let go (and not held): the pattern's gates again */
{
    gate_val = -1;
    seq_gate_live(-1);
}

/* v2's multi-function key: GATE TIME held, stopped, and a step: INITIALIZE ALL, SYNTH, RHYTHM, ACTIVE STEP (1-4),
 * SAMPLE & HOLD, 16-STEP SYNTH, GATE TIME HOLD, HALF TEMPO (5-8) */
static void multi_function(int w)
{
    static const char *const INIT[4] = {"ALL", "SYNTH", "RHYTHM", "ACTIVE STEP"};
    if (w < 4) {
        seq_clear(&set.pat, w);
        set_dirty = 1;
        app_say("INITIALIZE", INIT[w]);
    } else if (w == 4) {
        if (set.v[P_LWAVE] == TRIBE_NLWAVE - 1) {
            set.v[P_LWAVE] = lwave_prev;
        } else {
            lwave_prev = (uint8_t)set.v[P_LWAVE];
            set.v[P_LWAVE] = TRIBE_NLWAVE - 1;
        }
        set_dirty = 1;
        engine_update();
        app_say("SAMPLE & HOLD", set.v[P_LWAVE] == TRIBE_NLWAVE - 1 ? "ON" : "OFF");
    } else if (w == 5) {
        set.pat.steps16 = (uint8_t)!set.pat.steps16;
        if (!set.pat.steps16)
            ed_slot &= (uint8_t)~1u;
        set_dirty = 1;
        app_say("16-STEP SYNTH", set.pat.steps16 ? "ON" : "OFF");
    } else if (w == 6) {
        gate_hold = (uint8_t)!gate_hold;
        app_say("GATE TIME HOLD", gate_hold ? "ON" : "OFF");
    } else if (w == 7) {
        half_tempo = (uint8_t)!half_tempo;
        engine_update();
        app_say("HALF TEMPO", half_tempo ? "ON" : "OFF");
    }
}

/* a part picked (PRESET on SEQ, or its black key on STEP): a drum's heard, as the monotribe's part buttons */
static void part_pick(int i)
{
    if (i != (int)set.v[P_PART]) {
        set.v[P_PART] = (uint16_t)i;
        if (i == SEQ_SYNTH && !set.pat.steps16)
            ed_slot &= (uint8_t)~1u;         /* (the synth's cursor on a step) */
    }
    if (i != SEQ_SYNTH && set.v[P_PREVIEW])  /* DRUM PREVIEW (the monotribe's Global 6): heard */
        drum(i - SEQ_BD);
    app_say("PART", PART_OPT[i]);
}

/* STEP: a key of the step row */
static void step_key(uint32_t k, int w, int32_t note)
{
    int part = set.v[P_PART], i, sl;
    if (w < 0) {                             /* the black keys: a part (a drum's heard), ACTIVE STEP, GATE TIME */
        for (i = 0; i < SEQ_NPART; i++)
            if (PART_KEY[i] == k) {
                if (rec_armed && i != SEQ_SYNTH) {   /* REC on: the part button plays its drum, recorded */
                    set.v[P_PART] = (uint16_t)i;
                    pad(i - SEQ_BD);
                } else {
                    part_pick(i);
                }
            }
        if (k == KEY_ACTIVE)
            app_say("ACTIVE STEP", "HOLD, PICK STEPS");
        else if (k == KEY_GATE)
            app_say("GATE TIME", seq_playing() ? "TOUCH THE KEYS" : "HOLD, PICK A STEP");
        return;
    }
    if (keys_now >> KEY_GATE & 1u) {         /* GATE TIME: playing, the keys a ribbon (left 1.4 %, right 100 %) */
        if (seq_playing()) {
            char t[8];
            gate_val = (int16_t)((w * SEQ_GATE_TIE + 7) / 15);
            seq_gate_live(gate_val);
            gate_text(gate_val, t);
            app_say("GATE TIME", t);
        } else if (w < SEQ_STEPS) {
            multi_function(w);
        }
        return;
    }
    if ((keys_now >> KEY_ACTIVE & 1u) && seq_playing()) {   /* ACTIVE STEP while it plays: MOTION VOLUME, the keys a
                                                             * ribbon, left silent, right full (Ver.2) */
        char t[8];
        motion_live = (int16_t)((w * 127 + 7) / 15);
        tribe_motion((float)motion_live / 127.0f);
        int_text(motion_live * 100 / 127, t);
        text_cat(t, "%");
        app_say("MOTION", t);
        return;
    }
    if (keys_now >> KEY_ACTIVE & 1u) {       /* ACTIVE STEP: a step on or off for the part (never all off) */
        uint8_t m;
        if (w >= SEQ_STEPS)
            return;
        m = (uint8_t)(set.pat.active[part] ^ (1u << w));
        if (!m) {
            app_say("ONE STEP", "STAYS ON");
            return;
        }
        set.pat.active[part] = m;
        set_dirty = 1;
        {
            char t[16] = "ACTIVE ", n[4];
            int_text(w + 1, n);
            text_cat(t, n);
            app_say(t, m >> w & 1u ? "ON" : "OFF");
        }
        return;
    }
    if (rec_armed) {                         /* REC: the keys play, and record while it plays */
        synth_key(k, note, touch_vel(k));
        return;
    }
    if (w >= (n16(part) ? 16 : SEQ_STEPS))
        return;
    sl = n16(part) ? w : 2 * w;              /* a step on or off; a drum's part key held: half-way through it */
    if (part != SEQ_SYNTH && (keys_now >> PART_KEY[part] & 1u))
        sl++;
    set.pat.on[part] ^= (uint16_t)(1u << sl);
    if (part == SEQ_SYNTH) {
        ed_slot = (uint8_t)sl;
        if (set.pat.on[part] >> sl & 1u) {
            set.pat.note[sl] = (uint8_t)(ui.last_note ? ui.last_note : 60);
            set.pat.gate[sl] = (uint8_t)set.v[P_GATE];
        }
    }
    set_dirty = 1;
    step_say(set.pat.on[part] >> sl & 1u ? "ON" : "OFF", sl);
}

static void key_press(uint32_t k, int32_t note)
{
    int w = white_index(k);
    if (ui.page == PG_SEQ) {
        step_key(k, w, note);
        return;
    }
    synth_key(k, note, touch_vel(k));
    if (ui.page == PG_EDIT && !rec_armed && set.v[P_PART] == SEQ_SYNTH) {   /* EDIT: the note in, on to the next */
        set.pat.note[ed_slot] = (uint8_t)note;
        set.pat.on[SEQ_SYNTH] |= (uint16_t)(1u << ed_slot);
        set_dirty = 1;
        step_say(PAR[P_EDNOTE].name, ed_slot);
        ed_slot = (uint8_t)((ed_slot + (set.pat.steps16 ? 1u : 2u)) % SEQ_SLOTS);
    }
}

/* the step cursor (SELECT on SEQ, K1 on EDIT): a step at a time (a slot in 16-step mode), round the 8 or 16 */
static int cursor_half(void) { return set.v[P_PART] != SEQ_SYNTH || set.pat.steps16; }   /* (half-steps too) */
static void cursor_step(int dir)
{
    int n = cursor_half() ? 1 : 2;
    ed_slot = (uint8_t)(((ed_slot & (cursor_half() ? 15 : 14)) + n * dir + SEQ_SLOTS) % SEQ_SLOTS);
    step_say("", ed_slot);
}

/* SELECT and ALGO turned as a switch: a position a detent, no wrap */
static void switch_turn(int pn, int32_t e)
{
    int v = clamp(set.v[pn] + (e > 0 ? 1 : -1), 0, PAR[pn].nopt - 1);
    char t[16];
    if (v != set.v[pn]) {
        set.v[pn] = (uint16_t)v;
        set_dirty = 1;
        engine_update();
    }
    param_text(pn, t);
    app_say(pn == P_LWAVE ? "LFO WAVE" : "WAVE", t);
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    keys_now = keys;
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            key_press(k, note);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            if (ui.key_synth & bit) {        /* a synth key let go */
                ui.key_synth &= ~bit;
                ui.prs[k] = 0;
                key_up(note);
                rec_note_off(note);
                plat_midi_send(0x08u | (0x80u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
            } else if (k == KEY_GATE && gate_val >= 0) {   /* GATE TIME let go: the pattern's gates again */
                gate_off();
            } else if (k == KEY_ACTIVE && motion_live >= 0) {   /* ACTIVE STEP let go: MOTION's levels again */
                motion_live = -1;
                tribe_motion(-1.0f);
            }
        } else if ((keys & bit) && (ui.key_synth & bit) && touch_pressure(k) != ui.prs[k]) {   /* poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
            plat_midi_send(0x0Au | (0xA0u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24,
                           TO_USB | TO_BLE);
        }
    }
    if (motion_live >= 0 && recording() && (keys >> KEY_ACTIVE & 1u))   /* MOTION VOLUME recorded, as it plays */
        motion_rec(motion_live);
    if (gate_val >= 0 && recording() && (keys >> KEY_GATE & 1u)) {   /* GATE TIME recorded: each synth step as it plays */
        int sl = seq_slot(SEQ_SYNTH);
        if (sl >= 0 && !set.pat.steps16)
            sl &= ~1;
        if (sl >= 0 && (set.pat.on[SEQ_SYNTH] >> sl & 1u) && set.pat.gate[sl] != (uint8_t)gate_val) {
            set.pat.gate[sl] = (uint8_t)gate_val;
            set_dirty = 1;
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed: its name, and what it does here */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (is_page_button(k)) {
            page_button(k);
        } else if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_PLAY) {             /* PLAY: start from step 1, or stop */
            int on = !seq_playing();
            seq_play(on);
            if (!on)                         /* (stopped: the synth at LEVEL again) */
                tribe_motion(-1.0f);
            app_say(on ? "PLAY" : "STOP", "");
        } else if (k == B_REC) {            /* REC: armed or not; armed, the keys and pads record while it plays */
            rec_armed = (uint8_t)!rec_armed;
            rec_slot = -1;
            app_say("REC", rec_armed ? "ON" : "OFF");
        } else if (k == B_SAVE && ui.page == PG_DEVICES) {   /* DEVICES: SAVE connects the pick, or lets it go */
            int ev = btdev_save();
            app_say(ev == BTDEV_CONNECTING ? btdev_word(ev) : "BLE", ev == BTDEV_CONNECTING ? btdev_name() : btdev_word(ev));
        } else if (k == B_SAVE) {
            if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
                set_dirty = 0;
                app_say("SAVED", "");
            } else {
                app_say("SAVE", "FAILED");
            }
        } else {
            app_say("BUTTON", BTN_NAME[k]);
        }
    }
    for (k = 0; k < NE; k++) {               /* an encoder turned: a knob's value, the EG, the wave */
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.turns[k] += e;
        ui.turned_at[k] = plat_ms();
        ui.redraw = 1;
        if (k == EN_SELECT) {                /* SELECT: the step cursor on SEQ, else the VCO's WAVE */
            if (PAGE[ui.page].button == B_SEQ)
                cursor_step(e > 0 ? 1 : -1);
            else
                switch_turn(P_WAVE, e);
        } else if (k == EN_ALGO && ui.page == PG_LFO) {   /* ALGO: the LFO's WAVE on LFO */
            switch_turn(P_LWAVE, e);
        } else if (k == EN_PRESET && PAGE[ui.page].button == B_SEQ) {   /* PRESET: the part, on SEQ */
            part_pick(clamp(set.v[P_PART] + (e > 0 ? 1 : -1), 0, SEQ_NPART - 1));
        } else if (k == EN_K1 && PAGE[ui.page].p[0] == P_EDSTEP) {   /* EDIT: K1 moves the step cursor too */
            cursor_step(e > 0 ? 1 : -1);
        } else if (k >= EN_K1 && PAGE[ui.page].p[k - EN_K1] >= 0) {
            /* K1..K4: the page's settings; a knob a step a detent (three, turned fast), a switch a position */
            int pn = PAGE[ui.page].p[k - EN_K1], one = PAR[pn].nopt || pn == P_EDSTEP || pn == P_BLEDEV;
            int32_t v = pget(pn), was = v;
            if (pn == P_EDSTEP && !set.pat.steps16)   /* the step editor: steps only, in 8-step mode */
                v = (v & ~1) + 2 * (e > 0 ? 1 : -1);
            else
                v += one ? (e > 0 ? 1 : -1) : e > 1 || e < -1 ? 3 * e : e;
            v = clamp(v, PAR[pn].lo, pmax(pn));
            if (v != was) {
                pset(pn, v);
                if (pn != P_EDSTEP && pn != P_BLEDEV)
                    set_dirty = 1;
                engine_update();
                if (pn == P_BLE && FM1B_BLE)
                    plat_ble_enable(!set.v[P_BLE]);
                if (pn == P_LEVEL && recording())   /* LEVEL turned while it records: MOTION VOLUME */
                    motion_rec(set.v[P_LEVEL]);
                if (pn == P_MOTION && !set.v[P_MOTION])
                    tribe_motion(-1.0f);
                if (pn == P_SYNTH_CH && voice_note >= 0)   /* (its note off would go out on the new channel) */
                    note_off(voice_note), nheld = 0;
            }
            {
                char m[24];
                param_text(pn, m);
                app_say(PAR[pn].name, m);
            }
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    lights(btn, keys);
}

void app_input_only(void) { input(); }

static void cell(int32_t x, int32_t y, int32_t w, const char *s, int on)   /* a labelled box, lit when on */
{
    cv_rect(x, y, w, 18, on ? C_HI : C_LINE);
    cv_text(x + (w - text_w(&FONT_XS, s)) / 2, y + 2, &FONT_XS, s, on ? C_BLACK : C_GRAY);
}

static void key_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, uint16_t off)
{
    int32_t v, p;
    cv_rect(x, y, w, h, off);
    if (!(ui.keys >> k & 1u))
        return;
    v = (ui.vel[k] * h + 126) / 127;
    p = (ui.prs[k] * h + 126) / 127;
    cv_rect(x, y + h - v, w, v, C_HI);
    if (p)
        cv_rect(x + 2, y + h - p, w - 4, p, C_WHITE);
}

static void draw(void)
{
    char t[24];
    int32_t note = voice_note;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    /* the header: the name and the note, or a message for a moment */
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(10, 6, &FONT_B, PAGE[ui.page].name, C_WHITE);   /* the page, where the name was */
        if (FM1B_BLE)                        /* Bluetooth: grey while it waits, lit while a computer is connected */
            x = cv_text(x + 8, 10, &FONT_XS, "BLE", plat_ble_connected() ? C_HI : C_DIM);
        if (FM1B_BLE_CENTRAL)                /* a Bluetooth MIDI device: lit while connected */
            cv_text(x + 6, 10, &FONT_XS, "DEV", plat_ble_device() == BLE_DEV_READY ? C_HI : C_DIM);
        if (note >= 0) {                     /* the note, and the last key's velocity and pressure */
            int32_t x;
            note_name(note, t);
            x = 196 - text_w(&FONT_B, t);
            cv_text(x, 6, &FONT_B, t, C_HI);
            if (ui.keys >> ui.last_key & 1u) {
                char n[24] = "V ";
                int_text(ui.vel[ui.last_key], t);
                text_cat(n, t);
                text_cat(n, "  P ");
                int_text(ui.prs[ui.last_key], t);
                text_cat(n, t);
                cv_text(x - 12 - text_w(&FONT_XS, n), 10, &FONT_XS, n, C_GRAY);
            }
        }
    }
    {                                        /* the page's place on its button, top right: 1/2 */
        uint32_t np, k = page_of(ui.page, &np);
        char c[6] = {(char)('1' + k), '/', (char)('0' + np), 0};
        cv_rect(202, 0, 38, 32, C_BLACK);
        cv_text(230 - text_w(&FONT_B, c), 6, &FONT_B, c, C_GRAY);
    }
    cv_blit(0, 0);
    /* the panel: the page buttons' tabs, the page's lit (its button lit too); K1..K4's settings, each lit for a moment
     * after a turn, a knob as a bar and its value, a switch as its position; the drum pads (EDIT, GLO, HOME), lit as
     * they hit */
    cv_begin(240, 144, C_BLACK);
    {
        uint32_t nt = 0, b, ntab = 0;
        int32_t tw;
        for (i = 0; i < NPG; i++)
            ntab += !i || PAGE[i].button != PAGE[i - 1].button;
        tw = 232 / (int32_t)ntab;
        for (i = 0; i < NPG; i++) {
            if (i && PAGE[i].button == PAGE[i - 1].button)
                continue;                    /* one tab a button */
            b = PAGE[i].button;
            t[0] = 0;
            text_cat(t, PAGE[i].tab);
            cell(4 + (int32_t)nt * tw, 0, tw - 2, t, PAGE[ui.page].button == b);
            cv_text(4 + (int32_t)nt * tw + (tw - 2 - text_w(&FONT_XS, BTN_NAME[b])) / 2, 20, &FONT_XS, BTN_NAME[b], C_DIM);
            nt++;
        }
    }
    for (i = 0; i < 4u; i++) {
        static const char *const KN[4] = {"K1", "K2", "K3", "K4"};
        int pn = PAGE[ui.page].p[i];
        int32_t x = 4 + (int32_t)i * 58, w = 56;
        int lit = ui.turned_at[EN_K1 + i] && now - ui.turned_at[EN_K1 + i] < 300u;
        cv_text(x + (w - text_w(&FONT_XS, KN[i])) / 2, 84, &FONT_XS, KN[i], C_DIM);
        if (pn < 0) {
            cv_rect(x + w / 2 - 6, 60, 12, 1, C_LINE);
            continue;
        }
        cv_text(x + (w - text_w(&FONT_XS, PAR[pn].name)) / 2, 38, &FONT_XS, PAR[pn].name, lit ? C_HI : C_GRAY);
        param_text(pn, t);
        if (PAR[pn].nopt) {
            cell(x, 56, w, t, 1);
        } else {
            int32_t v = pget(pn) * 127 / (pmax(pn) ? pmax(pn) : 1);   /* (a pattern setting too, by its range) */
            cv_rect(x, 54, w, 7, C_LINE);
            cv_rect(x + 1, 55, (v * (w - 2) + 63) / 127, 5, lit ? C_WHITE : C_HI);
            cv_text(x + (w - text_w(&FONT_XS, t)) / 2, 65, &FONT_XS, t, lit ? C_WHITE : C_AMB);
        }
    }
    if (ui.page == PG_EDIT) {
        /* the piano roll: the synth's steps (8, or 16), each note at its pitch and as long as its gate; a drum's 16
         * slots (steps and their halves), each hit a block. An active step's floor marked, the cursor boxed, the
         * playhead white; the part (PRESET) at the top left */
        int part = set.v[P_PART], drm = part != SEQ_SYNTH, n = drm || set.pat.steps16 ? 16 : 8;
        int c, lo = 127, hi = 0, span, base, sp = seq_slot(part);
        int32_t cw = 232 / n;
        for (c = 0; c < n && !drm; c++) {
            int sl = set.pat.steps16 ? c : 2 * c;
            if (set.pat.on[SEQ_SYNTH] >> sl & 1u) {
                lo = set.pat.note[sl] < lo ? set.pat.note[sl] : lo;
                hi = set.pat.note[sl] > hi ? set.pat.note[sl] : hi;
            }
        }
        if (lo > hi)
            lo = hi = 60;
        span = hi - lo < 12 ? 12 : hi - lo;
        base = lo - (span - (hi - lo)) / 2;
        for (c = 0; c < n; c++) {
            int sl = n == 16 ? c : 2 * c, now_on = sp >= 0 && (n == 16 ? sp : sp & ~1) == sl;
            int32_t x = 4 + (int32_t)c * cw;
            if (set.pat.active[part] >> (sl / 2) & 1u)
                cv_rect(x, 139, cw - 2, 1, C_LINE);
            if (set.pat.on[part] >> sl & 1u) {
                if (drm) {
                    cv_rect(x, 118, cw - 2, 10, now_on ? C_WHITE : (sl & 1) ? C_AMB : C_HI);
                } else {
                    int g = set.pat.gate[sl];
                    int32_t y = 134 - (set.pat.note[sl] - base) * 24 / span;
                    int32_t w = g >= SEQ_GATE_TIE ? cw : (cw - 2) * (14 + 986 * g / 127) / 1000;
                    cv_rect(x, y, w < 2 ? 2 : w, 4, now_on ? C_WHITE : C_HI);
                }
            }
            if (sl == (n == 16 ? ed_slot : ed_slot & ~1)) {   /* the cursor */
                cv_rect(x - 1, 107, cw, 1, C_WHITE);
                cv_rect(x - 1, 141, cw, 1, C_WHITE);
                cv_rect(x - 1, 107, 1, 35, C_WHITE);
                cv_rect(x + cw - 2, 107, 1, 35, C_WHITE);
            }
        }
        {
            int32_t x = cv_text(4, 95, &FONT_XS, PART_OPT[part], C_HI);
            cv_text(x + 6, 95, &FONT_XS, "PRESET", C_DIM);
        }
    } else if (ui.page == PG_SEQ) {
        /* the pattern: a row a part (the part edited lit), 16 slots, the steps in pairs: a slot that plays filled,
         * one in an active step outlined, the playhead white; the step cursor under the synth's row */
        uint32_t r, c;
        for (r = 0; r < SEQ_NPART; r++) {
            int32_t y = 98 + (int32_t)r * 11, sl = seq_slot((int)r);
            cv_text(4, y, &FONT_XS, PART_OPT[r], r == set.v[P_PART] ? C_HI : C_GRAY);
            for (c = 0; c < SEQ_SLOTS; c++) {
                int32_t x = 46 + (int32_t)c * 12;
                int act = set.pat.active[r] >> (c / 2u) & 1u, on = set.pat.on[r] >> c & 1u;
                int dead = r == SEQ_SYNTH && !set.pat.steps16 && (c & 1u);
                uint16_t col = (int32_t)c == sl ? C_WHITE : on && !dead ? (c & 1u ? C_AMB : C_HI) : act && !dead ? C_LINE
                                                                                                          : C_BLACK;
                cv_rect(x, y + 1, c & 1u ? 8 : 10, 8, col);
                if (!act && !dead)
                    cv_rect(x + 3, y + 4, 3, 2, C_LINE);
            }
        }
        cv_rect(46 + (int32_t)ed_slot * 12, 141, 10, 2, C_WHITE);
    } else if (ui.page == PG_DEVICES) {
        char ti[32];
        btdev_title(ti);
        cv_text(4, 100, &FONT_XS, ti, C_GRAY);
        cv_text(4, 116, &FONT_XS, btdev_busy() ? "SAVE LETS IT GO" : "TURN K1 TO PICK, SAVE CONNECTS", C_DIM);
    } else if (ui.page == PG_DRUMS) {
        cv_text(4, 108, &FONT_XS, "DRUMS", C_GRAY);
        for (i = 0; i < TRIBE_NDRUM; i++) {   /* each lights as it plays */
            int lit = ui.drum_at[i] && now - ui.drum_at[i] < 150u;
            cell(44 + (int32_t)i * 46, 104, 44, DRUM_NAME[i], lit);
        }
    } else if (PAGE[ui.page].button != B_GLO) {
        /* the switches on their own encoders: SELECT the VCO's WAVE; on LFO, ALGO its WAVE */
        int lt = ui.turned_at[EN_SELECT] && now - ui.turned_at[EN_SELECT] < 300u;
        param_text(P_WAVE, t);
        cv_text(4, 102, &FONT_XS, "VCO WAVE", lt ? C_HI : C_GRAY);
        cell(70, 98, 56, t, 1);
        cv_text(132, 102, &FONT_XS, "SELECT", C_DIM);
        if (ui.page == PG_LFO) {
            lt = ui.turned_at[EN_ALGO] && now - ui.turned_at[EN_ALGO] < 300u;
            param_text(P_LWAVE, t);
            cv_text(4, 124, &FONT_XS, "LFO WAVE", lt ? C_HI : C_GRAY);
            cell(70, 120, 56, t, 1);
            cv_text(132, 124, &FONT_XS, "ALGO", C_DIM);
        }
    }
    cv_blit(0, 36);
    /* the keys, black above white: a held key fills to its velocity (amber), its pressure over that (white) */
    cv_begin(240, 60, C_BLACK);
    for (i = 0; i < 16u; i++)
        key_cell(4 + (int32_t)i * 14, 14, 13, 14, WK[i], C_DIM);
    for (i = 0; i < 11u; i++) {
        static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each one follows */
        key_cell(4 + (int32_t)AT[i] * 14 + 8, 0, 11, 12, BK[i], C_LINE);
    }
    octave_text(t);
    cv_text(4, 36, &FONT_XS, "OCT", C_GRAY);
    cv_text(30, 36, &FONT_XS, t, C_HI);
    {                                        /* the transport: PLAY or STOP, the tempo (MIDI: the clock's), REC */
        int32_t x = cv_text(64, 36, &FONT_XS, seq_playing() ? "PLAY" : "STOP", seq_playing() ? C_WHITE : C_GRAY);
        if (set.v[P_SYNC])
            t[0] = 0, text_cat(t, "MIDI");
        else
            int_text(set.v[P_TEMPO], t);
        x = cv_text(x + 6, 36, &FONT_XS, t, C_AMB);
        if (half_tempo)
            x = cv_text(x + 4, 36, &FONT_XS, "/2", C_AMB);
        if (rec_armed)
            x = cv_text(x + 8, 36, &FONT_XS, "REC", C_HI);
        if (gate_hold)
            cv_text(x + 6, 36, &FONT_XS, "GATE", C_HI);
    }
    if (midi_count) {                        /* the last MIDI message in, and where from */
        static const char *const PORT[NPORTS] = {"USB", "BLE", "TRS"};
        static const char HEX[] = "0123456789ABCDEF";
        uint32_t m = midi_last, b;
        char h[12];
        for (b = 0; b < 3u; b++) {
            uint32_t v = (m >> (8u + 8u * b)) & 0xFFu;
            h[3 * b] = HEX[v >> 4];
            h[3 * b + 1] = HEX[v & 15u];
            h[3 * b + 2] = b < 2u ? ' ' : 0;
        }
        cv_text(4, 48, &FONT_XS, "MIDI IN", C_GRAY);
        cv_text(52, 48, &FONT_XS, midi_last_port < NPORTS ? PORT[midi_last_port] : "", C_HI);
        cv_text(82, 48, &FONT_XS, h, C_WHITE);
    }
    cv_text(178, 36, &FONT_XS, "VOL", C_GRAY);
    int_text((int32_t)pct, t);
    text_cat(t, "%");
    cv_text(204, 36, &FONT_XS, t, C_HI);
    cv_blit(0, 180);
}

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    ui.page = PG_HOME;                        /* the first page at each start */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, lit_until, last_ble, last_midi;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    speak();
    for (i = 0; i < NE; i++)                 /* an encoder's light goes out */
        if (ui.turned_at[i] && now - ui.turned_at[i] < 400u && now > lit_until)
            lit_until = ui.turned_at[i] + 300u;
    if (lit_until && now >= lit_until) {
        lit_until = 0;
        ui.redraw = 1;
    }
    if (FM1B_BLE_CENTRAL) {                  /* DEVICES: it scans while the page is up; a device found is shown */
        static int found;
        int up = ui.page == PG_DEVICES && !set.v[P_BLE], n = up ? btdev_count() : 0, ev;
        btdev_page(up);
        if (n != found)
            ui.redraw = 1;
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)   /* a device connected, given up or lost */
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
    }
    if (midi_count != last_midi) {
        last_midi = midi_count;
        ui.redraw = 1;
    }
    {                                        /* the playhead moved, or the transport changed: on screen */
        static int32_t last_pos = -2;
        int32_t p = seq_playing() * 1000 + seq_slot(0) + 16 * seq_slot(1) + 256 * seq_slot(2) + 4096 * seq_slot(3);
        if (p != last_pos) {
            last_pos = p;
            if (PAGE[ui.page].button == B_SEQ || !seq_slot(0) || seq_slot(0) < 0)
                ui.redraw = 1;
        }
    }
    {                                        /* a drum hit: its pad lights, then goes out */
        static uint32_t knobs_seen, drum_off;
        uint32_t h = drum_hits, d;
        if (h != ui.drum_seen) {
            for (d = 0; d < TRIBE_NDRUM; d++)
                if (((h ^ ui.drum_seen) >> (8 * d)) & 0xFFu)
                    ui.drum_at[d] = now ? now : 1;
            ui.drum_seen = h;
            drum_off = now + 160u;
            ui.redraw = 1;
        } else if (drum_off && now >= drum_off) {
            drum_off = 0;
            ui.redraw = 1;
        }
        if (midi_knobs != knobs_seen) {      /* a CC changed a setting */
            knobs_seen = midi_knobs;
            set_dirty = 1;
            engine_update();
            ui.redraw = 1;
        }
    }
    if (pct != last_pct || (uint32_t)(plat_ble_connected() | plat_ble_device() << 1) != last_ble ||
        (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        last_ble = (uint32_t)(plat_ble_connected() | plat_ble_device() << 1);
        if (now >= ui.msg_until)
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
    static const char *const P[NP] = {"cutoff", "peak", "wave", "noise", "eg", "level", "rhythm", "rate", "int",
                                      "target", "mode", "lwave", "synth_ch", "drum_ch", "ble_off", "bd_dec", "sn_dec",
                                      "hh_dec", "keylight", "glow", "tempo", "part", "gate", "swing", "sync",
                                      "clkout", "range", "motion", "preview", "steps16", "edstep", "ednote", "edgate", "edon", "bledev"};
    int i;
    for (i = 0; i < NP; i++)                 /* a setting by its name: its 0..127, or its switch's position */
        if (!strcmp(what, P[i])) {
            *got = pget(i);
            return 1;
        }
    /* the pattern: onN (part N's slots, bits), activeN, noteN and gateN (slot N); playing, rec, slotN (part N) */
    if (!strncmp(what, "on", 2) && what[2] >= '0' && what[2] <= '9')
        *got = set.pat.on[atoi(what + 2) % SEQ_NPART];
    else if (!strncmp(what, "active", 6))
        *got = set.pat.active[atoi(what + 6) % SEQ_NPART];
    else if (!strncmp(what, "note", 4) && what[4] >= '0' && what[4] <= '9')
        *got = set.pat.note[atoi(what + 4) % SEQ_SLOTS];
    else if (!strncmp(what, "gate", 4) && what[4] >= '0' && what[4] <= '9')
        *got = set.pat.gate[atoi(what + 4) % SEQ_SLOTS];
    else if (!strcmp(what, "playing"))
        *got = seq_playing();
    else if (!strcmp(what, "rec"))
        *got = rec_armed;
    else if (!strncmp(what, "slot", 4))
        *got = seq_slot(atoi(what + 4) % SEQ_NPART);
    else if (!strcmp(what, "mid_keys"))
        *got = (int)mid_keys_now;
    else
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "page"))         /* PG_*: HOME 0 1, VCO 2, VCF VCA 3, LFO 4, DRUMS 5, STEP 6, EDIT 7, GLO 8 9 10 */
        *got = ui.page;
    else if (!strcmp(what, "gate_live"))    /* GATE TIME's gate (-1 none), GATE TIME HOLD, HALF TEMPO */
        *got = gate_val;
    else if (!strcmp(what, "motion_live"))  /* MOTION VOLUME played on the keys (-1 none); volN slot N's recorded */
        *got = motion_live;
    else if (!strncmp(what, "vol", 3) && what[3] >= '0' && what[3] <= '9')
        *got = set.pat.vol[atoi(what + 3) % SEQ_SLOTS];
    else if (!strcmp(what, "gate_hold"))
        *got = gate_hold;
    else if (!strcmp(what, "half_tempo"))
        *got = half_tempo;
    else if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "dim_keys"))     /* the keys glowing (KEY LIGHT GUIDE), and lit */
        *got = (int)dim_keys_now;
    else if (!strcmp(what, "lit_keys"))
        *got = (int)lit_keys_now;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else if (!strncmp(what, "drum", 4))     /* drumN: hits of BD 0, SN 1, HH 2 */
        *got = (int)(drum_hits >> (8 * (atoi(what + 4) % TRIBE_NDRUM)) & 0xFFu);
    else
        return 0;
    return 1;
}
#endif
