/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's sound engine: one monotribe-style synth voice (VCO + noise -> two-pole Korg35-style low-pass -> VCA with
 * three fixed envelopes) and three drums (BD, SN, HH). Clean-room: written from the research doc's estimates, Pirkle's
 * Korg35 note and Zavalishin's TPT one-pole, not from any other emulation's code or audio.
 *
 * Float only (no double). tribe_render runs in the audio interrupt; the setters may be called from the main loop
 * (each writes one aligned word, read once per block). */
#pragma once
#include <stdint.h>

#define TRIBE_SR 44100.0f

enum { TRIBE_SAW, TRIBE_TRI, TRIBE_SQR, TRIBE_NWAVE };          /* VCO WAVE */
enum { TRIBE_DECAY, TRIBE_GATE, TRIBE_ATTACK, TRIBE_NEG };       /* EG (the VCA's) */
enum { TRIBE_BD, TRIBE_SN, TRIBE_HH, TRIBE_NDRUM };

typedef struct {
    float cutoff;      /* 0..1: CUTOFF, exponential, ~80 Hz .. ~16 kHz */
    float peak;        /* 0..1: PEAK, self-oscillation near the top */
    float noise;       /* 0..1: NOISE, white noise beside the VCO into the filter */
    float level;       /* 0..1: the synth's LEVEL */
    float rhythm;      /* 0..1: RHYTHM, the drums' shared level */
    uint8_t wave;      /* TRIBE_SAW .. */
    uint8_t eg;        /* TRIBE_DECAY .. */
} tribe_params_t;

void tribe_init(void);
void tribe_set(const tribe_params_t *p);        /* the panel's settings (copied; applied at the next block) */
void tribe_note_on(float pitch);                /* MIDI note number, fractional for bend; retriggers the EG */
void tribe_note_glide(float pitch);             /* change pitch while held, no retrigger (legato, bend) */
void tribe_note_off(void);
void tribe_drum(int which);                     /* TRIBE_BD .. */
void tribe_panic(void);                         /* silence now */

/* n mono samples, about -1..1 (soft-limited), into out */
void tribe_render(float *out, uint32_t n);

/* for the host tests: the filter's cutoff in Hz and the EG level now */
float tribe_cutoff_hz(float cutoff01);
float tribe_eg_level(void);
