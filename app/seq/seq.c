/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's step sequencer (seq.h). Written from the monotribe's owner's manual and Korg's v2 feature list as the
 * research doc summarises them; the timing is FMtribe's own (96 ppqn, MIDI clock at 24 ppqn, not the original's 12). */
#include "seq.h"

#define SR 44100.0f

static seq_pattern_t *pat;
static volatile uint8_t playing, req;            /* req: 1 play (from step 1), 2 stop, 3 continue: taken by the render */
static volatile uint8_t ext, clk_out;
static volatile int16_t gate_live = -1;          /* GATE TIME held: the gate every synth note gets (-1 none) */
static const uint8_t ROLL_T[SEQ_NROLL] = {48, 24, 16, 12, 8, 6, 3};   /* a roll's interval, in ticks (96 a beat) */
static volatile uint8_t roll_live[SEQ_NPART];    /* each drum's live roll rate (0 none) */
static uint32_t roll_n[SEQ_NPART];               /* ticks into each live roll */
static float roll_acc;                           /* stopped: samples into the free-running roll tick */
static volatile float spt = 44100.0f * 60.0f / (120.0f * 96.0f);   /* samples a tick */
static volatile uint32_t swing_t;                /* ticks the second step of a pair starts late (0 .. 8: triplets) */
static float acc;                                /* samples into the tick */
static volatile uint32_t now;                    /* ticks since PLAY */
static uint32_t pair_t;                          /* ticks into the pair of steps (0 .. 47) */
static uint8_t first;                            /* the first step after PLAY: the parts start where they are */
static volatile int8_t pos[SEQ_NPART];           /* each part's step, -1 stopped */
static volatile uint8_t half[SEQ_NPART];         /* past the step's half slot */
static volatile uint32_t step_t0, step_len;      /* the step playing: its first tick, its length */
static float off_left = -1.0f;                   /* samples to the synth's note off (-1 none: tied, or none sounding) */
static uint8_t sounding;
static uint32_t samples, clock_last;             /* samples since boot; at the last MIDI clock in */
static float spt_ext = 44100.0f * 60.0f / (120.0f * 96.0f);   /* samples a tick, measured from MIDI clock */

void seq_init(seq_pattern_t *p)
{
    int i;
    pat = p;
    for (i = 0; i < SEQ_NPART; i++) {
        if (!pat->active[i])
            pat->active[i] = 0xFFu;
        pos[i] = -1;
    }
}

void seq_clear(seq_pattern_t *p, int what)
{
    int i;
    if (what == SEQ_CLEAR_ALL || what == SEQ_CLEAR_SYNTH) {
        p->on[SEQ_SYNTH] = 0;
        for (i = 0; i < SEQ_SLOTS; i++) {
            p->note[i] = 60;
            p->gate[i] = 64;
            p->vol[i] = SEQ_NO_VOL;
        }
    }
    if (what == SEQ_CLEAR_ALL || what == SEQ_CLEAR_DRUMS) {
        p->on[SEQ_BD] = p->on[SEQ_SN] = p->on[SEQ_HH] = 0;
        for (i = 0; i < 3 * SEQ_STEPS; i++)
            p->roll[i / SEQ_STEPS][i % SEQ_STEPS] = 0;
    }
    if (what == SEQ_CLEAR_ALL || what == SEQ_RESET_ACTIVE)
        for (i = 0; i < SEQ_NPART; i++)
            p->active[i] = 0xFFu;
    if (what == SEQ_CLEAR_ALL)
        p->steps16 = 0;
}

void seq_tempo(float bpm)
{
    if (bpm < 10.0f)
        bpm = 10.0f;
    if (bpm > 600.0f)
        bpm = 600.0f;
    spt = SR * 60.0f / (bpm * 96.0f);
}
void seq_swing(float s) { swing_t = (uint32_t)(s < 0.0f ? 0.0f : s > 1.0f ? 8.0f : s * 8.0f + 0.5f); }
void seq_ext(int on) { ext = (uint8_t)(on != 0); }
void seq_clock_out(int on) { clk_out = (uint8_t)(on != 0); }
void seq_roll_live(int part, int rate)
{
    if (part < SEQ_BD || part >= SEQ_NPART)
        return;
    if (rate > SEQ_NROLL)
        rate = SEQ_NROLL;
    if (rate > 0 && !roll_live[part])
        roll_n[part] = 0;                        /* (a new roll hits at once) */
    roll_live[part] = (uint8_t)(rate < 0 ? 0 : rate);
}
void seq_gate_live(int gate) { gate_live = (int16_t)(gate < 0 ? -1 : gate > SEQ_GATE_TIE ? SEQ_GATE_TIE : gate); }
void seq_play(int on) { req = on ? 1u : 2u; }
int seq_playing(void) { return playing || req == 1u; }
int seq_step(int part) { return playing ? pos[part] : -1; }
int seq_slot(int part) { return playing && pos[part] >= 0 ? 2 * pos[part] + half[part] : -1; }
uint32_t seq_now(void) { return now; }

static int next_active(int part, int from)       /* the step after from that plays (from -1: the first) */
{
    int i, s;
    uint8_t m = pat->active[part] ? pat->active[part] : 0xFFu;
    for (i = 1; i <= SEQ_STEPS; i++) {
        s = (from + i) % SEQ_STEPS;
        if (s < 0)
            s += SEQ_STEPS;
        if (m >> s & 1u)
            return s;
    }
    return 0;
}

static void synth_off(void)
{
    if (sounding) {
        sounding = 0;
        seq_cb_note_off();
    }
    off_left = -1.0f;
}

static float fm1_max1(float x) { return x < 1.0f ? 1.0f : x; }

static void slot(int part, int k, uint32_t len)  /* a part's slot comes round */
{
    if (part == SEQ_SYNTH && (!(k & 1) || pat->steps16))   /* the synth's level for the slot (MOTION VOLUME) */
        seq_cb_motion(pat->vol[k]);
    if (!(pat->on[part] >> k & 1u))
        return;
    if (part != SEQ_SYNTH) {
        seq_cb_drum(part);
        return;
    }
    if ((k & 1) && !pat->steps16)
        return;
    {
        uint32_t g = gate_live >= 0 ? (uint32_t)gate_live : pat->gate[k], slen = pat->steps16 ? len / 2u : len;
        seq_cb_note_on(pat->note[k]);
        sounding = 1;
        if (g >= SEQ_GATE_TIE)
            off_left = -1.0f;                    /* tied: until the next note, or stop */
        else                                     /* 1.4 % (gate 0) .. ~99 % (126) of the slot, to the sample */
            off_left = fm1_max1((float)slen * (0.014f + 0.986f * (float)g / 127.0f) * (ext ? spt_ext : spt));
    }
}

static void stop_now(void)
{
    int i;
    playing = 0;
    synth_off();
    for (i = 0; i < SEQ_NPART; i++)
        pos[i] = -1;
    if (clk_out)
        seq_cb_clock(0xFC);
}

static void start_now(int from_top)
{
    int i;
    playing = 1;
    if (from_top) {
        now = 0;
        pair_t = 0;
        acc = spt;                               /* step 1 at once */
        first = 1;
        for (i = 0; i < SEQ_NPART; i++)
            pos[i] = -1;
    }
    if (clk_out)
        seq_cb_clock(from_top ? 0xFA : 0xFB);
}

static void requests(void)
{
    uint8_t r = req;
    if (!r)
        return;
    req = 0;
    if (r == 2u) {
        if (playing)
            stop_now();
    } else {
        start_now(r == 1u);
    }
}

static void rolls(void)                          /* each tick (and stopped, the free tick): the rolls' hits */
{
    int i;
    for (i = SEQ_BD; i < SEQ_NPART; i++) {
        uint32_t r = roll_live[i], t;
        if (r) {                                 /* live: from its start */
            if (roll_n[i]++ % ROLL_T[r - 1] == 0)
                seq_cb_drum(i);
        } else if (playing && pos[i] >= 0 && (r = pat->roll[i - SEQ_BD][pos[i]]) != 0 && r <= SEQ_NROLL) {
            t = now - step_t0;                   /* recorded: through its step, from the step's start */
            if (t % ROLL_T[r - 1] == 0 && !(t == 0 && (pat->on[i] >> (2 * pos[i]) & 1u)))
                seq_cb_drum(i);
        }
    }
}

static void tick(void)                           /* one tick: the steps and slots on it, then on to the next */
{
    uint32_t a = SEQ_TPS + swing_t, b = SEQ_TPS - swing_t, t = pair_t;
    int i;
    if (t == 0u || t == a) {                     /* a step starts */
        uint32_t len = t == 0u ? a : b;
        step_t0 = now;
        step_len = len;
        for (i = 0; i < SEQ_NPART; i++) {
            pos[i] = (int8_t)(first ? next_active(i, -1) : next_active(i, pos[i]));
            half[i] = 0;
        }
        first = 0;
        for (i = 0; i < SEQ_NPART; i++)
            slot(i, 2 * pos[i], len);
    } else if (t == a / 2u || t == a + b / 2u) { /* half-way through it */
        for (i = 0; i < SEQ_NPART; i++) {
            half[i] = 1;
            slot(i, 2 * pos[i] + 1, step_len);
        }
    }
    rolls();
    if (clk_out && !ext && (now & 3u) == 0u)
        seq_cb_clock(0xF8);
    now++;
    if (++pair_t >= a + b)
        pair_t = 0;
}

void seq_midi_rt(uint8_t st)                     /* audio interrupt (the MIDI in) */
{
    int i;
    if (!ext)
        return;
    if (st == 0xFAu)
        req = 1;
    else if (st == 0xFBu)
        req = 3;
    else if (st == 0xFCu)
        req = 2;
    else if (st == 0xF8u) {
        uint32_t d = samples - clock_last;
        clock_last = samples;
        if (d > 0u && d < 44100u)                /* the tempo, for the gate times: four ticks a clock */
            spt_ext += ((float)d * 0.25f - spt_ext) * 0.3f;
        requests();
        if (playing)
            for (i = 0; i < 4; i++)              /* 24 ppqn: four ticks a clock */
                tick();
    }
}

uint32_t seq_frames(uint32_t max)
{
    float left;
    requests();
    if (off_left >= 0.0f && off_left + 0.5f < (float)max)   /* the note off, to the sample */
        max = off_left < 1.0f ? 1u : (uint32_t)(off_left + 0.5f);
    if (!playing && (roll_live[SEQ_BD] | roll_live[SEQ_SN] | roll_live[SEQ_HH])) {   /* stopped, a roll's tick */
        left = spt - roll_acc;
        return left <= 1.0f ? 1u : left < (float)max ? (uint32_t)left : max;
    }
    if (!playing || ext || !pat)
        return max;
    left = spt - acc;
    if (left <= 1.0f)
        return 1u;
    return left < (float)max ? (uint32_t)left : max;
}

void seq_advance(uint32_t frames)
{
    samples += frames;
    if (off_left >= 0.0f) {
        off_left -= (float)frames;
        if (off_left <= 0.5f)
            synth_off();
    }
    if (!playing) {                              /* stopped: a live roll keeps the tempo's time */
        if (roll_live[SEQ_BD] | roll_live[SEQ_SN] | roll_live[SEQ_HH]) {
            roll_acc += (float)frames;
            while (roll_acc >= spt - 0.5f) {
                roll_acc -= spt;
                rolls();
            }
        } else {
            roll_acc = spt;                      /* (the next roll's first hit at once) */
        }
        return;
    }
    if (ext || !pat)
        return;
    acc += (float)frames;
    while (acc >= spt - 0.5f && playing) {       /* (the half: a tick lands on the nearest sample) */
        acc -= spt;
        tick();
    }
}

int seq_rec_slot(int part)
{
    int p = pos[part], nx;
    float f;
    if (!playing || p < 0)
        return -1;
    f = ((float)(now - step_t0) + acc / spt) / (float)(step_len ? step_len : SEQ_TPS);
    nx = next_active(part, p);
    if (part == SEQ_SYNTH && !pat->steps16)
        return f < 0.5f ? 2 * p : 2 * nx;
    return f < 0.25f ? 2 * p : f < 0.75f ? 2 * p + 1 : 2 * nx;
}
