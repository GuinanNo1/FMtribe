/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* The template app: every control on the screen as it arrives, by the name the app knows it by (plat.h B_*, EN_*,
 * the keys), and a few of them doing something: the keys play a voice and go out over USB-MIDI (MIDI in plays it
 * too), OCT- / OCT+ set the octave, PRESET the wave, MASTER the volume, SAVE stores the settings; the keys are
 * played with velocity and pressure (lib/touch.h), shown on them. One of each thing the platform offers, kept short. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "tribe.h"

/* ------------------------------------------------------------- the settings --- */
/* every setting the panel edits, by number: a knob's 0..127, or a switch's position (its OPT names) */
enum { P_CUTOFF, P_PEAK, P_WAVE, P_NOISE, P_EG, P_LEVEL, P_RHYTHM, P_LRATE, P_LINT, P_LTARGET, P_LMODE, P_LWAVE,
       P_SYNTH_CH, P_DRUM_CH, P_BLE, P_BD_DEC, P_SN_DEC, P_HH_DEC, P_KEYLIGHT, P_GLOW, NP };
enum { KL_OFF, KL_HELD, KL_GUIDE, NKL };   /* KEY LIGHT: none, the keys held, those and the page's key functions */
typedef struct {
    const char *name;
    uint8_t nopt;                        /* 0: a knob, 0..127; else a switch with this many positions */
    const char *const *opt;              /* a switch's positions, as the screen shows them */
    uint8_t def;
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
static const param_t PAR[NP] = {
    {"CUTOFF", 0, 0, 80},
    {"PEAK", 0, 0, 30},
    {"WAVE", TRIBE_NWAVE, WAVE_OPT, TRIBE_SAW},
    {"NOISE", 0, 0, 0},
    {"EG", TRIBE_NEG, EG_OPT, TRIBE_DECAY},
    {"LEVEL", 0, 0, 100},
    {"RHYTHM", 0, 0, 90},
    {"RATE", 0, 0, 64},
    {"INT", 0, 0, 0},
    {"TARGET", TRIBE_NTARGET, TARGET_OPT, TRIBE_LFO_VCO},
    {"MODE", TRIBE_NMODE, MODE_OPT, TRIBE_LFO_SLOW},
    {"WAVE", TRIBE_NLWAVE, LWAVE_OPT, TRIBE_LFO_TRI},
    {"SYNTH CH", 16, CH_OPT, 0},        /* the MIDI channels: the synth's (in and out), the drums' */
    {"DRUM CH", 16, CH_OPT, 9},
    {"BLE MIDI", 2, BLE_OPT, 0},         /* Bluetooth MIDI on or off (REC held, too) */
    {"BD DECAY", 0, 0, 32},              /* the drums' decays: 32 stock (the decay mods' pots, as a setting) */
    {"SN DECAY", 0, 0, 32},
    {"HH DECAY", 0, 0, 32},
    {"KEY LIGHT", NKL, KL_OPT, KL_GUIDE},
    {"GLOW", 3, GLOW_OPT, 1},            /* the unlit buttons' and the guide keys' glow */
};

/* the pages: a button shows its page and K1..K4 edit it; pressed again, its next page (LFO has two) */
typedef struct {
    const char *name;                    /* on the tab */
    uint8_t button;                      /* plat.h B_* */
    int8_t p[4];                         /* the settings on K1..K4 (-1: none) */
} page_t;
enum { PG_VCF, PG_VCA, PG_LFO1, PG_LFO2, PG_DRUMS, PG_GLOBAL, PG_GLOBAL2, NPG };   /* (SEQ: the sequencer's, next) */
static const page_t PAGE[NPG] = {
    {"VCO VCF", B_SEL, {P_CUTOFF, P_PEAK, P_WAVE, P_NOISE}},
    {"EG VCA", B_ENV, {P_EG, P_LEVEL, -1, -1}},
    {"LFO", B_LFO, {P_LRATE, P_LINT, P_LTARGET, P_LMODE}},
    {"LFO", B_LFO, {P_LWAVE, -1, -1, -1}},
    {"DRUMS", B_ARP, {P_RHYTHM, P_BD_DEC, P_SN_DEC, P_HH_DEC}},   /* and the black keys play them */
    {"GLOBAL", B_GLO, {P_SYNTH_CH, P_DRUM_CH, P_BLE, -1}},
    {"GLOBAL", B_GLO, {P_KEYLIGHT, P_GLOW, -1, -1}},
};

#define SET_MAGIC 0x35544D46u            /* "FMT5": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +2: six positions, as the monotribe's 64' .. 2' */
    uint8_t v[NP];                       /* the settings above */
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

static const char *const DRUM_NAME[TRIBE_NDRUM] = {"BD", "SN", "HH"};
static const uint8_t DRUM_NOTE[TRIBE_NDRUM] = {36, 40, 42};   /* channel 10, the monotribe's (GM) */
/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
/* the DRUMS page's pads: the black keys of the F#-G#-A# groups, BD SN HH (key 0 is F3); the white keys still play the
 * synth there, and the other black keys nothing */
static const uint8_t PAD_KEY[2][TRIBE_NDRUM] = {{1, 3, 5}, {13, 15, 17}};
#define PAD_KEYS (1u << 1 | 1u << 3 | 1u << 5 | 1u << 13 | 1u << 15 | 1u << 17)
#define BLACK_KEYS (PAD_KEYS | 1u << 8 | 1u << 10 | 1u << 20 | 1u << 22 | 1u << 25)
static int key_pad(uint32_t k);          /* (UI: the drum a key plays on this page, or -1) */

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
    tribe_set(&p);
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
    tribe_render(render_buf, n);
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
    for (i = 0; i < NP; i++)
        set.v[i] = PAR[i].def;
}

void app_init(void)
{
    uint32_t i, bad = 0;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.octave < -3 || set.octave > 2)
        bad = 1;
    for (i = 0; i < NP; i++)
        bad |= PAR[i].nopt ? set.v[i] >= PAR[i].nopt : set.v[i] > 127u;
    if (bad)
        settings_defaults();
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
    uint8_t looking;                     /* Bluetooth MIDI devices: REC pressed, the scan's list up (lib/btdev) */
    uint32_t rec_t;                      /* REC went down (0: not held, or its hold already acted) */
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

static void param_text(int p, char *t)   /* a setting's value as the screen shows it */
{
    if (PAR[p].nopt)
        t[0] = 0, text_cat(t, PAR[p].opt[set.v[p]]);
    else
        int_text(set.v[p], t);
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

/* REC held a second: Bluetooth off or on (kept with the settings, so it stays so at the next power-on) */
static void ble_toggle(void)
{
    set.v[P_BLE] = (uint8_t)!set.v[P_BLE];
    set_dirty = 1;
    plat_ble_enable(!set.v[P_BLE]);
    app_say("BLUETOOTH", set.v[P_BLE] ? "OFF" : "ON");
    ui.redraw = 1;
}

/* REC: a Bluetooth MIDI device (FM1B_BLE_CENTRAL, lib/btdev): look for devices, then connect to the one ALGORITHM
 * picks, then let it go */
static void ble_device_say(void)
{
    char name[21];
    if (btdev_found(btdev_pick(), name, 0))
        app_say("DEVICE", name);
    else
        app_say("BLE", "SCANNING");
}
static void ble_device_button(void)
{
    int ev;
    if (!btdev_busy() && !ui.looking) {
        ui.looking = 1;
        app_say("BLE", "SCANNING");
        return;
    }
    ev = btdev_save();
    ui.looking = 0;
    app_say(ev == BTDEV_CONNECTING ? btdev_word(ev) : "BLE", ev == BTDEV_CONNECTING ? btdev_name() : btdev_word(ev));
}

/* the lights: the held buttons, the page's button and SAVE (something to save) lit, the others glowing (GLOW); the keys
 * by KEY LIGHT: none, the held ones, or those and the page's key functions dimly (the DRUMS page's pads) */
static uint32_t lit_keys_now, dim_keys_now;  /* (the host's checks) */
static void lights(uint32_t btn, uint32_t keys)
{
    uint32_t b = btn | 1u << PAGE[ui.page].button | (set_dirty ? 1u << B_SAVE : 0u), lit = 0, dim = 0;
    int kl = set.v[P_KEYLIGHT], glow = set.v[P_GLOW];
    if (kl >= KL_HELD)
        lit = keys;
    if (kl == KL_GUIDE && ui.page == PG_DRUMS)
        dim = PAD_KEYS;
    lit_keys_now = lit;
    dim_keys_now = dim;
    if (glow == 0 && dim) {                  /* the guide wants a glow: the keys' then, but not the buttons' */
        glow = 1;
        plat_lights(b, lit, 0, dim, glow);
    } else {
        plat_lights(b, lit, glow ? ~b & ((1u << NB) - 1u) : 0, dim, glow);
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if (ui.page == PG_DRUMS && (BLACK_KEYS & bit) && (keys & bit)) {   /* the DRUMS page's black keys (a key let
                                                                             * go goes on: a note it held stops) */
            int d = key_pad(k);
            if (d >= 0 && (keys & bit) && !(ui.keys & bit)) {
                drum(d);
                plat_midi_send(0x09u | (0x90u | set.v[P_DRUM_CH]) << 8 | (uint32_t)DRUM_NOTE[d] << 16 | 100u << 24,
                               TO_USB | TO_BLE);
                plat_midi_send(0x08u | (0x80u | set.v[P_DRUM_CH]) << 8 | (uint32_t)DRUM_NOTE[d] << 16, TO_USB | TO_BLE);
            }
            continue;
        }
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.last_key = (int32_t)k;
            key_down(note, v);
            plat_midi_send(0x09u | (0x90u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            ui.prs[k] = 0;
            key_up(note);
            plat_midi_send(0x08u | (0x80u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {   /* pressure: poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
            plat_midi_send(0x0Au | (0xA0u | set.v[P_SYNTH_CH]) << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24,
                           TO_USB | TO_BLE);
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
        else if (k == B_REC && FM1B_BLE)
            ui.rec_t = plat_ms();                /* a press: on release (a hold switches Bluetooth) */
        else if (k == B_SAVE) {
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
    if (FM1B_BLE && ui.rec_t) {             /* REC: held a second, Bluetooth on / off; a press, a device */
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) {
                ui.rec_t = 0;
                ble_toggle();
            }
        } else {
            ui.rec_t = 0;
            if (FM1B_BLE_CENTRAL && !set.v[P_BLE])
                ble_device_button();
            else
                app_say("BUTTON", "REC");
        }
    }
    for (k = 0; k < NE; k++) {               /* an encoder turned: a knob's value, the EG, the wave */
        char t[12];
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.turns[k] += e;
        ui.turned_at[k] = plat_ms();
        ui.redraw = 1;
        if (k == EN_ALGO && FM1B_BLE_CENTRAL && ui.looking) {
            btdev_choose(btdev_pick() + e);
            ble_device_say();
        } else if (k >= EN_K1 && PAGE[ui.page].p[k - EN_K1] >= 0) {
            /* K1..K4: the page's settings; a knob a step a detent (three, turned fast), a switch a position */
            int pn = PAGE[ui.page].p[k - EN_K1];
            int32_t v = (int32_t)set.v[pn], hi = PAR[pn].nopt ? PAR[pn].nopt - 1 : 127;
            v = clamp(v + (PAR[pn].nopt ? (e > 0 ? 1 : -1) : e > 1 || e < -1 ? 3 * e : e), 0, hi);
            if (v != set.v[pn]) {
                set.v[pn] = (uint8_t)v;
                set_dirty = 1;
                engine_update();
                if (pn == P_BLE && FM1B_BLE)
                    plat_ble_enable(!set.v[P_BLE]);
                if (pn == P_SYNTH_CH && voice_note >= 0)   /* (its note off would go out on the new channel) */
                    note_off(voice_note), nheld = 0;
            }
            param_text(pn, t);
            app_say(PAR[pn].name, t);
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    lights(btn, keys);
}

void app_input_only(void) { input(); }

static int key_pad(uint32_t k)
{
    int g, d;
    if (ui.page != PG_DRUMS)
        return -1;
    for (g = 0; g < 2; g++)
        for (d = 0; d < TRIBE_NDRUM; d++)
            if (PAD_KEY[g][d] == k)
                return d;
    return -1;
}

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
        int32_t x = cv_text(10, 6, &FONT_B, APP_NAME, C_WHITE);
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
            text_cat(t, PAGE[i].name);
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
            int32_t v = set.v[pn];
            cv_rect(x, 54, w, 7, C_LINE);
            cv_rect(x + 1, 55, (v * (w - 2) + 63) / 127, 5, lit ? C_WHITE : C_HI);
            cv_text(x + (w - text_w(&FONT_XS, t)) / 2, 65, &FONT_XS, t, lit ? C_WHITE : C_AMB);
        }
    }
    cv_text(4, 108, &FONT_XS, "DRUMS", C_GRAY);
    for (i = 0; i < TRIBE_NDRUM; i++) {
        int lit = ui.drum_at[i] && now - ui.drum_at[i] < 150u;
        cell(44 + (int32_t)i * 46, 104, 44, DRUM_NAME[i], lit);
        {
            static const char *const KN[TRIBE_NDRUM] = {"F#", "G#", "A#"};   /* the keys that play them */
            cv_text(44 + (int32_t)i * 46 + (44 - text_w(&FONT_XS, KN[i])) / 2, 125, &FONT_XS, KN[i],
                    ui.page == PG_DRUMS ? C_AMB : C_DIM);
        }
    }
    cv_blit(0, 36);
    /* the keys, black above white: a held key fills to its velocity (amber), its pressure over that (white) */
    cv_begin(240, 60, C_BLACK);
    for (i = 0; i < 16u; i++)
        key_cell(4 + (int32_t)i * 14, 14, 13, 14, WK[i], C_DIM);
    for (i = 0; i < 11u; i++) {
        static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each one follows */
        key_cell(4 + (int32_t)AT[i] * 14 + 8, 0, 11, 12, BK[i], key_pad(BK[i]) >= 0 ? C_AMB : C_LINE);
    }
    octave_text(t);
    cv_text(4, 36, &FONT_XS, "OCT", C_GRAY);
    cv_text(30, 36, &FONT_XS, t, C_HI);
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
    ui.page = PG_VCF;                        /* the first page at each start */
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
    if (FM1B_BLE_CENTRAL) {                  /* looking: a device found is named at once (REC connects to it) */
        static int found;
        int n = ui.looking ? btdev_count() : 0, ev;
        btdev_page(ui.looking && !set.v[P_BLE]);
        if (n > found && btdev_pick() < n)
            ble_device_say();
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)   /* a device connected, given up or lost */
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
    }
    if (midi_count != last_midi) {
        last_midi = midi_count;
        ui.redraw = 1;
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
                                      "hh_dec", "keylight", "glow"};
    int i;
    for (i = 0; i < NP; i++)                 /* a setting by its name: its 0..127, or its switch's position */
        if (!strcmp(what, P[i])) {
            *got = set.v[i];
            return 1;
        }
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "page"))         /* PG_*: 0 VCO VCF, 1 EG VCA, 2 LFO 1, 3 LFO 2, 4 DRUMS, 5 GLOBAL 1, 6 2 */
        *got = ui.page;
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
