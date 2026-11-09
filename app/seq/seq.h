/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's step sequencer, after the monotribe's (owner's manual and Korg's v2 feature page, via the research doc):
 * one pattern of 8 steps, a 16th note each, for four parts (SYNTH, BD, SN, HH). Each step has two slots, on the step
 * and half-way through it: the drums' half-steps, and the synth's in-between steps in 16-step mode (v2). Each part
 * has its own active steps (v2: polyrhythms), skipped in playback. The synth's steps keep a note and a gate time.
 *
 * Time runs at 96 ticks a quarter note (24 a step), in the audio interrupt: the render asks how many samples to the
 * next tick (seq_frames), renders that many, then calls seq_tick. On MIDI clock (24 ppqn) each clock is 4 ticks.
 * What it plays goes out through the app's callbacks (seq_cb_*), from the audio interrupt. */
#pragma once
#include <stdint.h>

enum { SEQ_SYNTH, SEQ_BD, SEQ_SN, SEQ_HH, SEQ_NPART };
#define SEQ_STEPS 8
#define SEQ_SLOTS 16                     /* slot 2s: step s; 2s + 1: half-way through it */
#define SEQ_TPS 24                       /* ticks a step (a 16th note at 96 ppqn) */
#define SEQ_GATE_TIE 127                 /* gate 127: 100 %, the note runs on into the next */
#define SEQ_NO_VOL 255                   /* a slot with no MOTION VOLUME recorded */
#define SEQ_NROLL 7                      /* roll rates: 1 8ths, 2 16ths, 3 16th triplets, 4 32nds, 5 32nd triplets,
                                          * 6 64ths, 7 128ths (Ver.2's, left to right along the ribbon) */

typedef struct {
    uint8_t note[SEQ_SLOTS];             /* the synth's note at each slot (MIDI) */
    uint8_t gate[SEQ_SLOTS];             /* and its gate time, 0 (1.4 % of the step) .. 127 (100 %, tied) */
    uint16_t on[SEQ_NPART];              /* the slots that play, per part (bit = slot) */
    uint8_t active[SEQ_NPART];           /* the active steps, per part (bit = step; never all off) */
    uint8_t roll[3][SEQ_STEPS];          /* drum rolls (Ver.2): BD SN HH, each step's roll rate (1..SEQ_NROLL; 0 none) */
    uint8_t vol[SEQ_SLOTS];              /* MOTION VOLUME: the synth's level at each slot, 0..127 (SEQ_NO_VOL: none) */
    uint8_t steps16;                     /* the synth's half slots play too (v2 16-step synth) */
    uint8_t rsv;
} seq_pattern_t;

/* the app's side: called from the audio interrupt */
void seq_cb_note_on(uint8_t note);
void seq_cb_note_off(void);
void seq_cb_drum(int part);              /* SEQ_BD .. SEQ_HH */
void seq_cb_clock(uint8_t status);       /* MIDI clock out: 0xF8, 0xFA start, 0xFC stop (when clock out is on) */
void seq_cb_motion(uint8_t vol);         /* a synth slot comes round: its MOTION VOLUME (0..127, or SEQ_NO_VOL) */

void seq_init(seq_pattern_t *p);         /* the pattern the sequencer plays (the app's, saved with its settings) */
void seq_clear(seq_pattern_t *p, int what);   /* SEQ_CLEAR_* */
enum { SEQ_CLEAR_ALL, SEQ_CLEAR_SYNTH, SEQ_CLEAR_DRUMS, SEQ_RESET_ACTIVE };

/* settings (main loop) */
void seq_tempo(float bpm);               /* 10 .. 600 */
void seq_swing(float s);                 /* 0 .. 1: the second step of each pair later, up to a triplet feel */
void seq_ext(int on);                    /* follow MIDI clock instead of the tempo */
void seq_clock_out(int on);              /* send MIDI clock, start and stop */
/* GATE TIME played live (the monotribe's GATE TIME button and ribbon): every synth note's gate from now on, 0 .. 127
 * as the pattern's, instead of its own; -1 the pattern's again */
void seq_gate_live(int gate);
/* a drum rolled live (a DRUMS pad held with ROLL on, or GATE TIME and a key with a drum part): part SEQ_BD..SEQ_HH
 * hits at once and then every roll interval, at the tempo, playing or stopped; rate 0 stops it */
void seq_roll_live(int part, int rate);

/* transport (either side) */
void seq_play(int on);                   /* from step 1 */
int seq_playing(void);
void seq_midi_rt(uint8_t status);        /* MIDI in, real time: 0xF8 clock, 0xFA start, 0xFB continue, 0xFC stop */

/* the render (audio interrupt) */
uint32_t seq_frames(uint32_t max);       /* samples to the next tick (max if none comes sooner, or stopped) */
void seq_advance(uint32_t frames);       /* those samples rendered: the ticks that fell due */

/* where it is (any side): each part's step (0..7, -1 stopped), the slot playing (0..15), the ticks into the step */
int seq_step(int part);
int seq_slot(int part);
/* recording: the slot (0..15) a part's hit or note made now lands on, quantised to the nearest slot it can play (for
 * the synth in 8-step mode, the nearest step) */
int seq_rec_slot(int part);
uint32_t seq_now(void);                  /* ticks since PLAY: to time a held note's gate */
