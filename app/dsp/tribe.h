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
enum { TRIBE_LFO_VCO, TRIBE_LFO_BOTH, TRIBE_LFO_VCF, TRIBE_NTARGET };   /* LFO TARGET: VCO, VCO+VCF, VCF */
enum { TRIBE_LFO_FAST, TRIBE_LFO_SLOW, TRIBE_LFO_1SHOT, TRIBE_NMODE };  /* LFO MODE */
enum { TRIBE_LFO_SAW, TRIBE_LFO_TRI, TRIBE_LFO_SQR, TRIBE_LFO_SH, TRIBE_NLWAVE };   /* LFO WAVE (S&H: v2) */

typedef struct {
    float cutoff;      /* 0..1: CUTOFF, exponential, ~80 Hz .. ~16 kHz */
    float peak;        /* 0..1: PEAK, self-oscillation near the top */
    float noise;       /* 0..1: NOISE, white noise beside the VCO into the filter */
    float level;       /* 0..1: the synth's LEVEL */
    float rhythm;      /* 0..1: RHYTHM, the drums' shared level */
    float lfo_rate;    /* 0..1: RATE, exponential over the MODE's range (FAST 1 Hz..5 kHz, SLOW and 1SHOT 0.05..18 Hz) */
    float lfo_int;     /* 0..1: INT., 0 none */
    float decay[3];    /* the drums' decays, BD SN HH: a multiple of the stock time (1 stock; the decay mods' range) */
    float glide;       /* seconds: the pitch's slide to a new note played legato (0 none; RANGE NARROW / WIDE) */
    uint8_t wave;      /* TRIBE_SAW .. */
    uint8_t eg;        /* TRIBE_DECAY .. */
    uint8_t lfo_target;   /* TRIBE_LFO_VCO .. */
    uint8_t lfo_mode;     /* TRIBE_LFO_FAST .. */
    uint8_t lfo_wave;     /* TRIBE_LFO_SAW .. */
} tribe_params_t;

void tribe_init(void);
void tribe_set(const tribe_params_t *p);        /* the panel's settings (copied; applied at the next block) */
void tribe_note_on(float pitch);                /* MIDI note number, fractional for bend; retriggers the EG */
void tribe_note_glide(float pitch);             /* change pitch while held, no retrigger (legato, bend) */
void tribe_note_off(void);
void tribe_motion(float level);                 /* MOTION VOLUME: the synth's level 0..1 instead of LEVEL (-1: LEVEL) */
void tribe_drum(int which);                     /* TRIBE_BD .. */
void tribe_panic(void);                         /* silence now */

/* n mono samples, about -1..1 (soft-limited), into out */
void tribe_render(float *out, uint32_t n);

/* for the host tests and the screen: the filter's cutoff in Hz, the EG level now, the LFO's rate in Hz for a RATE
 * and MODE, and its output now (-1..1, before INT) */
float tribe_cutoff_hz(float cutoff01);
float tribe_eg_level(void);
float tribe_pitch_now(void);                    /* the pitch sounding (a glide under way), as a MIDI note */
float tribe_lfo_hz(float rate01, int mode);
float tribe_lfo_out(void);
