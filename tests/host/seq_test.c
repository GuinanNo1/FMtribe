/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's sequencer on the host: timing, parts, active steps, gate times, 16-step synth, swing, MIDI clock in and
 * out, and recording's quantising.
 *   cc -O2 -Iapp/seq -o seq_test tests/host/seq_test.c && ./seq_test */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../app/seq/seq.c"

static int fails;
#define CHECK(cond, ...) do { if (cond) printf("  ok   "); else { printf("  FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { uint32_t t; char what; int v; } ev_t;
static ev_t ev[4096];
static int nev;
static uint32_t clock_samples;                   /* the time, in samples */
static void log_ev(char w, int v) { if (nev < 4096) ev[nev].t = clock_samples, ev[nev].what = w, ev[nev++].v = v; }
void seq_cb_note_on(uint8_t note) { log_ev('N', note); }
void seq_cb_note_off(void) { log_ev('O', 0); }
void seq_cb_drum(int part) { log_ev('D', part); }
void seq_cb_clock(uint8_t st) { log_ev('C', st); }
void seq_cb_motion(uint8_t vol) { if (vol != SEQ_NO_VOL) log_ev('V', vol); }

static void run(uint32_t samples)                /* as app_render would: blocks of 256, split at the ticks */
{
    uint32_t done = 0;
    while (done < samples) {
        uint32_t blk = samples - done < 256u ? samples - done : 256u, i = 0;
        while (i < blk) {
            uint32_t k = seq_frames(blk - i);
            clock_samples += k;
            seq_advance(k);
            i += k;
        }
        done += blk;
    }
}

static seq_pattern_t P;
static void fresh(void)
{
    memset(&P, 0, sizeof P);
    seq_clear(&P, SEQ_CLEAR_ALL);
    seq_init(&P);
    seq_tempo(120);
    seq_swing(0);
    seq_ext(0);
    seq_clock_out(0);
    if (playing)
        seq_play(0), run(1);
    nev = 0;
    clock_samples = 0;
}

static int count(char w, int v, uint32_t from, uint32_t to)
{
    int i, c = 0;
    for (i = 0; i < nev; i++)
        if (ev[i].what == w && (v < 0 || ev[i].v == v) && ev[i].t >= from && ev[i].t < to)
            c++;
    return c;
}
static int nth(char w, int v, int n)             /* the nth such event's index */
{
    int i;
    for (i = 0; i < nev; i++)
        if (ev[i].what == w && (v < 0 || ev[i].v == v) && n-- == 0)
            return i;
    return -1;
}

int main(void)
{
    int i, a, b;
    double step = 44100.0 * 60 / 120 / 4;        /* a 16th at 120 BPM: 5512.5 samples */

    printf("Timing\n");
    fresh();
    P.on[SEQ_BD] = 0x5555u;                      /* every step's on-slot */
    P.on[SEQ_SN] = 1u << 1;                      /* step 1's half slot */
    seq_play(1);
    run((uint32_t)(step * 16 + 100));
    CHECK(count('D', SEQ_BD, 0, 1u << 30) == 17, "BD on all 8 steps, two bars and the next: %d hits", count('D', SEQ_BD, 0, 1u << 30));
    a = nth('D', SEQ_BD, 0);
    CHECK(ev[a].t < 10, "step 1 at once: %u samples", ev[a].t);
    b = nth('D', SEQ_BD, 8);
    CHECK(fabs((ev[b].t - ev[a].t) / 8.0 - step) < 1.0, "a step %.1f samples (120 BPM: %.1f)", (ev[b].t - ev[a].t) / 8.0, step);
    b = nth('D', SEQ_SN, 0);
    CHECK(fabs((double)(ev[b].t - ev[a].t) - step / 2) < 60, "SN's half slot %u samples after step 1 (%.0f)", ev[b].t - ev[a].t, step / 2);

    printf("Active steps (per part)\n");
    fresh();
    P.on[SEQ_BD] = 0xFFFFu;
    P.on[SEQ_HH] = 1u;                           /* HH: step 1 only, of 3 active steps */
    P.active[SEQ_HH] = 0x07u;
    P.active[SEQ_SYNTH] = 0x01u;
    seq_play(1);
    run((uint32_t)(step * 12 + 100));            /* 12 steps and a bit: 13 step starts */
    CHECK(count('D', SEQ_HH, 0, 1u << 30) == 5, "HH over 3 steps: step 1 five times in 13 (%d)", count('D', SEQ_HH, 0, 1u << 30));
    CHECK(count('D', SEQ_BD, 0, 1u << 30) == 25, "BD all 16 slots: 25 in 12.5 steps (%d)", count('D', SEQ_BD, 0, 1u << 30));

    printf("Synth: gate times, tie, 16 steps\n");
    fresh();
    P.on[SEQ_SYNTH] = 1u | 1u << 2 | 1u << 3;    /* steps 1 and 2, and step 2's half slot */
    P.note[0] = 48; P.gate[0] = 63;
    P.note[2] = 50; P.gate[2] = SEQ_GATE_TIE;
    P.note[3] = 52; P.gate[3] = 0;
    seq_play(1);
    run((uint32_t)(step * 3));
    a = nth('N', 48, 0);
    b = nth('O', -1, 0);
    CHECK(a >= 0 && b > a && fabs((ev[b].t - ev[a].t) / step - 0.507) < 0.03, "gate 63: %.3f of the step (0.507)", (ev[b].t - ev[a].t) / step);
    CHECK(count('N', 52, 0, 1u << 30) == 0, "8-step mode: the half slot's note doesn't play");
    a = nth('N', 50, 0);
    CHECK(count('O', -1, ev[a].t, ev[a].t + (uint32_t)step * 2) == 0, "gate 127: tied, no note off before the next note");
    fresh();
    P.on[SEQ_SYNTH] = 1u << 2 | 1u << 3;
    P.note[2] = 50; P.gate[2] = 100;
    P.note[3] = 52; P.gate[3] = 0;
    P.steps16 = 1;
    seq_play(1);
    run((uint32_t)(step * 3));
    a = nth('N', 52, 0);
    b = nth('O', -1, 1);
    CHECK(a >= 0, "16-step mode: the half slot's note plays");
    CHECK(a >= 0 && b > a && (ev[b].t - ev[a].t) < 0.04 * step, "gate 0: %.3f of the step (1.4 %% of a half step: 0.007)", (ev[b].t - ev[a].t) / step);
    seq_play(0);
    run(10);
    CHECK(count('O', -1, 0, 1u << 30) >= 2 && seq_step(0) == -1, "stop: the note off, the parts at rest");

    printf("GATE TIME live\n");
    fresh();
    P.on[SEQ_SYNTH] = 1u;
    P.note[0] = 48; P.gate[0] = 0;
    seq_gate_live(63);
    seq_play(1);
    run((uint32_t)(step * 2));
    a = nth('N', 48, 0);
    b = nth('O', -1, 0);
    CHECK(a >= 0 && b > a && fabs((ev[b].t - ev[a].t) / step - 0.507) < 0.03, "live gate 63 over the step's 0: %.3f (0.507)", (ev[b].t - ev[a].t) / step);
    seq_gate_live(-1);
    seq_play(0);
    run(10);

    printf("MOTION VOLUME\n");
    fresh();
    P.vol[0] = 40;                               /* step 1's level; step 2's half slot ignored in 8-step mode */
    P.vol[3] = 90;
    P.vol[4] = 127;
    seq_play(1);
    run((uint32_t)(step * 3));
    a = nth('V', -1, 0);
    CHECK(a >= 0 && ev[a].v == 40 && ev[a].t < 10, "step 1's level at once: %d", a >= 0 ? ev[a].v : -1);
    CHECK(count('V', 90, 0, 1u << 30) == 0, "8-step mode: a half slot's level is ignored");
    CHECK(count('V', 127, 0, 1u << 30) == 1, "step 3's level");
    seq_play(0);
    run(10);

    printf("Swing\n");
    fresh();
    P.on[SEQ_BD] = 0x5555u;
    seq_swing(1.0f);
    seq_play(1);
    run((uint32_t)(step * 4 + 100));
    a = nth('D', SEQ_BD, 0);
    b = nth('D', SEQ_BD, 1);
    i = nth('D', SEQ_BD, 2);
    CHECK(fabs((double)(ev[b].t - ev[a].t) / (ev[i].t - ev[b].t) - 2.0) < 0.02, "full swing: 2:1, a triplet feel (%.2f)", (double)(ev[b].t - ev[a].t) / (ev[i].t - ev[b].t));
    CHECK(fabs((ev[i].t - ev[a].t) - 2 * step) < 2, "the pair still takes two steps");

    printf("Tempo range\n");
    fresh();
    P.on[SEQ_BD] = 0x5555u;
    seq_tempo(600);
    seq_play(1);
    run(44100);
    CHECK(count('D', SEQ_BD, 0, 44100) == 40, "600 BPM: 40 steps a second (%d)", count('D', SEQ_BD, 0, 44100));
    fresh();
    P.on[SEQ_BD] = 0x5555u;
    seq_tempo(10);
    seq_play(1);
    run(44100 * 6 / 4 + 100);
    CHECK(count('D', SEQ_BD, 0, 1u << 30) == 2, "10 BPM: a step 1.5 s (%d in 1.5 s)", count('D', SEQ_BD, 0, 1u << 30));

    printf("MIDI clock out and in\n");
    fresh();
    seq_clock_out(1);
    seq_play(1);
    run((uint32_t)(step * 4) - 50);              /* just short of a quarter note */
    CHECK(count('C', 0xFA, 0, 1u << 30) == 1 && count('C', 0xF8, 0, 1u << 30) == 24, "start, then 24 clocks a quarter (%d)", count('C', 0xF8, 0, 1u << 30));
    seq_play(0);
    run(10);
    CHECK(count('C', 0xFC, 0, 1u << 30) == 1, "stop");
    fresh();
    P.on[SEQ_BD] = 0x5555u;
    seq_ext(1);
    seq_midi_rt(0xFA);
    for (i = 0; i < 24; i++) {
        run(500);                                /* (the internal tempo plays no part) */
        seq_midi_rt(0xF8);
    }
    CHECK(count('D', SEQ_BD, 0, 1u << 30) == 4, "24 clocks in: 4 steps (%d)", count('D', SEQ_BD, 0, 1u << 30));
    seq_midi_rt(0xFC);
    seq_midi_rt(0xF8);
    CHECK(count('D', SEQ_BD, 0, 1u << 30) == 4 && !seq_playing(), "stop: clocks after it play nothing");

    printf("Recording's quantising\n");
    fresh();
    seq_play(1);
    run((uint32_t)(step * 0.1));
    CHECK(seq_rec_slot(SEQ_SYNTH) == 0 && seq_rec_slot(SEQ_BD) == 0, "early in step 1: slot 0");
    run((uint32_t)(step * 0.4));
    CHECK(seq_rec_slot(SEQ_BD) == 1, "half-way: a drum's half slot (1)");
    CHECK(seq_rec_slot(SEQ_SYNTH) == 2, "the synth in 8-step mode: the next step (slot 2), got %d", seq_rec_slot(SEQ_SYNTH));
    run((uint32_t)(step * 0.3));
    CHECK(seq_rec_slot(SEQ_BD) == 2, "late in step 1: step 2 (slot 2), got %d", seq_rec_slot(SEQ_BD));
    P.active[SEQ_HH] = 0x01u;
    CHECK(seq_rec_slot(SEQ_HH) == 0, "a part with one active step: late lands on it again (slot 0), got %d", seq_rec_slot(SEQ_HH));

    printf("Clear\n");
    P.on[SEQ_BD] = 0xFFu; P.on[SEQ_SYNTH] = 1; P.active[SEQ_SN] = 3;
    seq_clear(&P, SEQ_CLEAR_DRUMS);
    CHECK(P.on[SEQ_BD] == 0 && P.on[SEQ_SYNTH] == 1, "drums cleared, synth kept");
    seq_clear(&P, SEQ_RESET_ACTIVE);
    CHECK(P.active[SEQ_SN] == 0xFF && P.active[SEQ_HH] == 0xFF, "active steps all on again");

    printf(fails ? "seq_test: %d FAILED\n" : "seq_test: all passed\n", fails);
    return fails != 0;
}
