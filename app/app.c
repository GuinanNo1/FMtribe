/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* The template app: every control on the screen as it arrives, by the name the app knows it by (plat.h B_*, EN_*,
 * the keys), and a few of them doing something: the keys play a voice and go out over USB-MIDI (MIDI in plays it
 * too), OCT- / OCT+ set the octave, PRESET the wave, MASTER the volume, SAVE stores the settings; the keys are
 * played with velocity and pressure (lib/touch.h), shown on them. One of each thing the platform offers, kept short. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "tribe.h"

#define SET_MAGIC 0x32544D46u            /* "FMT2": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +2: six positions, as the monotribe's 64' .. 2' */
    uint8_t wave;                        /* VCO WAVE: TRIBE_SAW, TRIBE_TRI, TRIBE_SQR */
    uint8_t ble_off;                     /* Bluetooth switched off (REC held); 0, the default: on */
    uint8_t eg;                          /* EG: TRIBE_DECAY, TRIBE_GATE, TRIBE_ATTACK */
    uint8_t knob[5];                     /* 0..127: CUTOFF, PEAK, NOISE, LEVEL, RHYTHM */
    uint8_t rsv[3];
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

enum { KN_CUTOFF, KN_PEAK, KN_NOISE, KN_LEVEL, KN_RHYTHM, NKN };
static const char *const KNOB_NAME[NKN] = {"CUTOFF", "PEAK", "NOISE", "LEVEL", "RHYTHM"};
static const uint8_t KNOB_DEF[NKN] = {80, 30, 0, 100, 90};
static const char *const WAVE_NAME[TRIBE_NWAVE] = {"SAW", "TRIANGLE", "SQUARE"};
static const char *const EG_NAME[TRIBE_NEG] = {"DECAY", "GATE", "ATTACK"};
static const char *const DRUM_NAME[TRIBE_NDRUM] = {"BD", "SN", "HH"};
static const uint8_t DRUM_NOTE[TRIBE_NDRUM] = {36, 40, 42};   /* channel 10, the monotribe's (GM) */
/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
/* the panel for now (no sequencer yet): K1..K4 CUTOFF, PEAK, NOISE, LEVEL; SELECT RHYTHM; ALGO the EG; PRESET the
 * VCO's wave; EDIT, GLO, HOME play BD, SN, HH */
static const int8_t ENC_KNOB[NE] = {KN_RHYTHM, -1, -1, KN_CUTOFF, KN_PEAK, KN_NOISE, KN_LEVEL};
static int drum_button(uint32_t k) { return k == B_EDIT ? TRIBE_BD : k == B_GLO ? TRIBE_SN : k == B_HOME ? TRIBE_HH : -1; }

static void engine_update(void)          /* the settings to the engine */
{
    tribe_params_t p;
    p.cutoff = set.knob[KN_CUTOFF] / 127.0f;
    p.peak = set.knob[KN_PEAK] / 127.0f;
    p.noise = set.knob[KN_NOISE] / 127.0f;
    p.level = set.knob[KN_LEVEL] / 127.0f;
    p.rhythm = set.knob[KN_RHYTHM] / 127.0f;
    p.wave = set.wave;
    p.eg = set.eg;
    tribe_set(&p);
}

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
        if (ch == 9u) {                      /* channel 10: the drums */
            if (ty == 0x90u && d2)
                for (d = 0; d < TRIBE_NDRUM; d++)
                    if (d1 == DRUM_NOTE[d])
                        drum(d);
        } else if (ch == 0u) {               /* channel 1: the synth */
            if (ty == 0x90u && d2)
                note_on((int32_t)d1, d2);
            else if (ty == 0x80u || ty == 0x90u)
                note_off((int32_t)d1);
            else if (ty == 0xE0u) {
                bend_q = (int32_t)(d1 | (d2 << 7)) - 8192;
                if (voice_note >= 0)
                    tribe_note_glide(note_pitch(voice_note));
            } else if (ty == 0xB0u && d1 == 80u) {   /* EG shape: 32 decay, 64 gate, 96 attack */
                set.eg = d2 < 48u ? TRIBE_DECAY : d2 < 80u ? TRIBE_GATE : TRIBE_ATTACK;
                midi_knobs++;
            } else if (ty == 0xB0u && d1 == 7u) {    /* volume: LEVEL */
                set.knob[KN_LEVEL] = (uint8_t)d2;
                midi_knobs++;
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
    set.wave = TRIBE_SAW;
    set.eg = TRIBE_DECAY;
    set.ble_off = 0;
    for (i = 0; i < NKN; i++)
        set.knob[i] = KNOB_DEF[i];
}

void app_init(void)
{
    uint32_t i, bad = 0;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.octave < -3 || set.octave > 2 || set.wave >= TRIBE_NWAVE || set.eg >= TRIBE_NEG || set.ble_off > 1u)
        bad = 1;
    for (i = 0; i < NKN; i++)
        bad |= set.knob[i] > 127u;
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

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[128], o[4];
    if (ann_on() && ann_where()) {
        octave_text(o);
        t[0] = 0;
        text_cat(t, APP_NAME ". OCTAVE ");
        text_cat(t, o);
        text_cat(t, ". WAVE ");
        text_cat(t, WAVE_NAME[set.wave]);
        text_cat(t, ". EG ");
        text_cat(t, EG_NAME[set.eg]);
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

int fmtribe_ble_off(void) { return set.ble_off; }

/* REC held a second: Bluetooth off or on (kept with the settings, so it stays so at the next power-on) */
static void ble_toggle(void)
{
    set.ble_off = (uint8_t)!set.ble_off;
    set_dirty = 1;
    plat_ble_enable(!set.ble_off);
    app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
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

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.last_key = (int32_t)k;
            key_down(note, v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);   /* BLE: if built with it */
        } else if (!(keys & bit) && (ui.keys & bit)) {
            ui.prs[k] = 0;
            key_up(note);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {   /* pressure: poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
            plat_midi_send(0x0Au | 0xA0u << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24, TO_USB | TO_BLE);
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed: its name, and what it does here */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (drum_button(k) >= 0) {           /* EDIT, GLO, HOME: BD, SN, HH (and out on channel 10) */
            int d = drum_button(k);
            drum(d);
            plat_midi_send(0x09u | 0x99u << 8 | (uint32_t)DRUM_NOTE[d] << 16 | 100u << 24, TO_USB | TO_BLE);
            plat_midi_send(0x08u | 0x89u << 8 | (uint32_t)DRUM_NOTE[d] << 16, TO_USB | TO_BLE);
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
            if (FM1B_BLE_CENTRAL && !set.ble_off)
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
        } else if (k == EN_PRESET) {
            set.wave = (uint8_t)((set.wave + (e > 0 ? 1 : TRIBE_NWAVE - 1)) % TRIBE_NWAVE);
            set_dirty = 1;
            engine_update();
            app_say("WAVE", WAVE_NAME[set.wave]);
        } else if (k == EN_ALGO) {
            set.eg = (uint8_t)((set.eg + (e > 0 ? 1 : TRIBE_NEG - 1)) % TRIBE_NEG);
            set_dirty = 1;
            engine_update();
            app_say("EG", EG_NAME[set.eg]);
        } else if (ENC_KNOB[k] >= 0) {       /* a detent a step; turned fast, three */
            int kn = ENC_KNOB[k];
            int32_t v = clamp((int32_t)set.knob[kn] + (e > 1 || e < -1 ? 3 * e : e), 0, 127);
            if (v != set.knob[kn]) {
                set.knob[kn] = (uint8_t)v;
                set_dirty = 1;
                engine_update();
            }
            int_text(v, t);
            app_say(KNOB_NAME[kn], t);
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);                            /* the unlit buttons glow, readable in the dark */
    plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u), keys);
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
        int32_t x = cv_text(10, 6, &FONT_B, APP_NAME, C_WHITE);
        if (FM1B_BLE)                        /* Bluetooth: grey while it waits, lit while a computer is connected */
            x = cv_text(x + 8, 10, &FONT_XS, "BLE", plat_ble_connected() ? C_HI : C_DIM);
        if (FM1B_BLE_CENTRAL)                /* a Bluetooth MIDI device: lit while connected */
            cv_text(x + 6, 10, &FONT_XS, "DEV", plat_ble_device() == BLE_DEV_READY ? C_HI : C_DIM);
        if (note >= 0) {                     /* the note, and the last key's velocity and pressure */
            int32_t x;
            note_name(note, t);
            x = 230 - text_w(&FONT_B, t);
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
    cv_blit(0, 0);
    /* the panel: the five knobs (CUTOFF, PEAK, NOISE, LEVEL on K1..K4, RHYTHM on SELECT), lit for a moment after a
     * turn; the WAVE and EG switches (PRESET, ALGO); the drum pads (EDIT, GLO, HOME), lit as they hit */
    cv_begin(240, 144, C_BLACK);
    for (i = 0; i < NKN; i++) {
        static const uint8_t ENC_OF[NKN] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT};
        static const char *const UNDER[NKN] = {"K1", "K2", "K3", "K4", "SELECT"};
        int32_t x = 4 + (int32_t)i * 47, w = 43, v = set.knob[i];
        int lit = ui.turned_at[ENC_OF[i]] && now - ui.turned_at[ENC_OF[i]] < 300u;
        cv_text(x + (w - text_w(&FONT_XS, KNOB_NAME[i])) / 2, 0, &FONT_XS, KNOB_NAME[i], lit ? C_HI : C_GRAY);
        cv_rect(x, 15, w, 7, C_LINE);
        cv_rect(x + 1, 16, (v * (w - 2) + 63) / 127, 5, lit ? C_WHITE : C_HI);
        int_text(v, t);
        cv_text(x + (w - text_w(&FONT_XS, t)) / 2, 25, &FONT_XS, t, lit ? C_WHITE : C_AMB);
        cv_text(x + (w - text_w(&FONT_XS, UNDER[i])) / 2, 38, &FONT_XS, UNDER[i], C_DIM);
    }
    cv_text(4, 56, &FONT_XS, "WAVE", C_GRAY);
    cv_text(186, 56, &FONT_XS, "PRESET", C_DIM);
    for (i = 0; i < TRIBE_NWAVE; i++) {
        static const char *const W[TRIBE_NWAVE] = {"SAW", "TRI", "SQUARE"};
        cell(44 + (int32_t)i * 46, 54, 44, W[i], set.wave == i);
    }
    cv_text(4, 80, &FONT_XS, "EG", C_GRAY);
    cv_text(186, 80, &FONT_XS, "ALGO", C_DIM);
    for (i = 0; i < TRIBE_NEG; i++)
        cell(44 + (int32_t)i * 46, 78, 44, EG_NAME[i], set.eg == i);
    cv_text(4, 108, &FONT_XS, "DRUMS", C_GRAY);
    for (i = 0; i < TRIBE_NDRUM; i++) {
        static const char *const B[TRIBE_NDRUM] = {"EDIT", "GLO", "HOME"};
        int lit = ui.drum_at[i] && now - ui.drum_at[i] < 150u;
        cell(44 + (int32_t)i * 46, 104, 44, DRUM_NAME[i], lit);
        cv_text(44 + (int32_t)i * 46 + (44 - text_w(&FONT_XS, B[i])) / 2, 125, &FONT_XS, B[i], C_DIM);
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
        btdev_page(ui.looking && !set.ble_off);
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
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "wave"))
        *got = set.wave;
    else if (!strcmp(what, "eg"))
        *got = set.eg;
    else if (!strncmp(what, "knob", 4))     /* knobN: CUTOFF 0, PEAK 1, NOISE 2, LEVEL 3, RHYTHM 4 */
        *got = set.knob[atoi(what + 4) % NKN];
    else if (!strncmp(what, "drum", 4))     /* drumN: hits of BD 0, SN 1, HH 2 */
        *got = (int)(drum_hits >> (8 * (atoi(what + 4) % TRIBE_NDRUM)) & 0xFFu);
    else if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif
