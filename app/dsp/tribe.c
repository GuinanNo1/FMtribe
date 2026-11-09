/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's sound engine (tribe.h). Sources, and nothing else:
 *   - the research doc "Korg monotribe — emulation research for FM-1" (block diagram, controls) and its
 *     "Estimates from public recordings" table (drum pitches and decays, the Decay EG curve, filter range);
 *   - W. Pirkle, "Virtual Analog (VA) Korg35 Lowpass Filter", app note AN-5: the Korg35 as three TPT one-poles
 *     around a zero-delay feedback loop, with a saturator in the loop;
 *   - V. Zavalishin, "The Art of VA Filter Design": the TPT one-pole and SVF;
 *   - V. Välimäki et al. on PolyBLEP / PolyBLAMP oscillators.
 * Figures marked EST are estimates from recordings; figures marked GUESS have no measurement yet (CLAUDE.md
 * "Unknown") and are for the prototype comparison to settle. */
#include "tribe.h"
#include "fastmath.h"

#define SR TRIBE_SR
#define SR2 (2.0f * TRIBE_SR)                   /* the VCO and filter run 2x oversampled */

/* ------------------------------------------------------------ the settings --- */
static tribe_params_t par = {.cutoff = 0.6f, .peak = 0.3f, .level = 0.8f, .rhythm = 0.7f, .lfo_rate = 0.5f};
static volatile uint32_t par_seq;               /* bumped by tribe_set: the render copies at a block's start */
static tribe_params_t live;                     /* the render's copy */
static uint32_t live_seq;

/* --------------------------------------------------------------- the noise --- */
static uint32_t rng = 0x9E3779B9u;
static inline float white(void)                 /* xorshift32, uniform -1..1 */
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

/* ----------------------------------------------------------------- the VCO --- */
static float vco_ph, vco_dt;                    /* phase in turns; increment per 2x sample */
static volatile float note_pitch = 60.0f;       /* MIDI note, fractional */

static inline float blep(float t, float dt)     /* PolyBLEP residual for a unit step at t = 0 */
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
static inline float blamp(float t, float dt)    /* PolyBLAMP residual for a unit slope change at t = 0 */
{
    if (t < dt) {
        t = t / dt - 1.0f;
        return -(1.0f / 3.0f) * t * t * t;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt + 1.0f;
        return (1.0f / 3.0f) * t * t * t;
    }
    return 0.0f;
}

static inline float vco(uint8_t wave)           /* one 2x sample, -1..1 */
{
    float t = vco_ph, dt = vco_dt, t2 = t + 0.5f, s;
    if (t2 >= 1.0f)
        t2 -= 1.0f;
    if (wave == TRIBE_SAW) {
        s = 2.0f * t - 1.0f - blep(t, dt);
    } else if (wave == TRIBE_SQR) {
        s = (t < 0.5f ? 1.0f : -1.0f) + blep(t, dt) - blep(t2, dt);
    } else {                                    /* triangle: corners at t = 0 (top) and 0.5 (bottom), slope 4 */
        s = 4.0f * fm_fabsf(t - 0.5f) - 1.0f;
        s += 8.0f * dt * (blamp(t2, dt) - blamp(t, dt));
    }
    vco_ph = t + dt;
    if (vco_ph >= 1.0f)
        vco_ph -= 1.0f;
    return s;
}

/* ------------------------------------------------------ the Korg35 low-pass --- */
/* Pirkle AN-5: LPF1 -> [loop: LPF2, with HPF1 on its output in the feedback path], K the loop gain (self-oscillates
 * at K = 2), all three TPT one-poles with the same G = g / (1 + g), g = tan(pi fc / fs). The loop's zero-delay solve is
 * linear; the saturator (tanh) sits on the loop's input, which bounds self-oscillation. */
static float z1, z2, z3;                        /* the three one-poles' states */
static float cut_s, peak_s;                     /* the knobs, smoothed (~5 ms) */

#define CUT_LO 80.0f                            /* EST: lowest self-oscillation ~83 Hz */
#define CUT_OCT 7.8f                            /* 80 Hz .. ~17.8 kHz: EST "> 15 kHz"; exponential taper (EST, low) */
#define K_MAX 2.06f                             /* PEAK full: just past the self-oscillation point */

float tribe_cutoff_hz(float c) { return CUT_LO * fm_exp2f(fm_clampf(c, 0.0f, 1.0f) * CUT_OCT); }

typedef struct { float G, b2, b3, a0, K; } k35_t;

static inline void k35_coef(k35_t *c, float fc, float K)
{
    float g = fm_tanf(FM_PI * fm_minf(fc, 0.45f * SR2) / SR2);
    c->G = g / (1.0f + g);
    c->K = K;
    c->b2 = (K - K * c->G) / (1.0f + g);
    c->b3 = -1.0f / (1.0f + g);
    c->a0 = 1.0f / (1.0f - K * c->G + K * c->G * c->G);
}

static inline float k35(const k35_t *c, float x)
{
    float v, y1, u, lp;
    v = (x - z1) * c->G;                        /* LPF1 */
    y1 = v + z1;
    z1 = y1 + v;
    u = c->a0 * (y1 + c->b2 * z2 + c->b3 * z3); /* the loop's input, solved without a delay */
    u = fm_tanhf(u);                            /* the loop's saturation */
    v = (u - z2) * c->G;                        /* LPF2 */
    lp = v + z2;
    z2 = lp + v;
    v = (c->K * lp - z3) * c->G;                /* HPF1 (its state is all the loop needs) */
    z3 = v + z3 + v;
    return lp;
}

/* ------------------------------------------- 2x -> 1x: a 35-tap halfband --- */
/* remez, pass 0..19 kHz, stop 25.1 kHz .. (-47 dB); the odd taps either side of the centre (0.5) */
static const float HB[9] = {0.315381628f, -0.101135128f, 0.0560901429f, -0.0355158806f, 0.0233777312f,
                            -0.0153765348f, 0.00981690198f, -0.00594435043f, 0.00440713979f};
#define HB_N 35
static float hb_buf[2 * HB_N];
static uint32_t hb_i;

static inline void hb_push(float x)
{
    hb_buf[hb_i] = x;
    hb_buf[hb_i + HB_N] = x;
    if (++hb_i == HB_N)
        hb_i = 0;
}
static inline float hb_out(void)                /* after an even number of pushes */
{
    const float *w = &hb_buf[hb_i];             /* w[0] oldest .. w[HB_N - 1] newest */
    float s = 0.5f * w[17];
    uint32_t k;
    for (k = 0; k < 9u; k++)
        s += HB[k] * (w[16 - 2 * k] + w[18 + 2 * k]);
    return s;
}

/* ----------------------------------------------------------------- the EGs --- */
/* Decay (EST, high): a = (e^(-t/tau) - c) / (1 - c), tau 380 ms, c 0.04: silent at ~1.25 s, held or not.
 * Gate and Attack (GUESS): Gate full while held; Attack the Decay curve upside down (full at ~1.25 s).
 * Note off (GUESS, from the short notes in the synth-as-kick demo): a fast close, tau ~12 ms.
 * The VCA follows the EG through a ~0.4 ms slew: a sharp edge, as the hardware's, without a hard step. */
#define EG_TAU 0.380f
#define EG_C 0.04f
#define REL_TAU 0.012f
#define VCA_TAU 0.0004f
static float eg_e;                              /* e^(-t/tau) since note on */
static float eg_a, eg_vca;                      /* the EG's level, the VCA's (slewed) */
static uint8_t eg_gate;                         /* held */
static volatile uint8_t ev_on, ev_off;          /* note on / off from the main loop, taken by the render */
static float k_eg, k_rel, k_vca;

float tribe_eg_level(void) { return eg_vca; }

static inline float eg_step(uint8_t mode)
{
    if (eg_gate) {
        float d = (eg_e - EG_C) * (1.0f / (1.0f - EG_C));
        eg_e *= k_eg;
        if (d < 0.0f)
            d = 0.0f;
        eg_a = mode == TRIBE_DECAY ? d : mode == TRIBE_GATE ? 1.0f : 1.0f - d;
    } else {
        eg_a *= k_rel;
    }
    eg_vca += (eg_a - eg_vca) * k_vca;
    return eg_vca;
}

/* --------------------------------------------------------------- the drums --- */
/* BD (EST, high): a pinged resonator: ~2 ms rise to full scale, then a cosine at 44 Hz decaying with tau 55 ms; the
 * first half-cycle stretched (here at 0.8 x), no pitch sweep after it.
 * SN (EST): the same at 244 Hz, tau 9 ms, 1 ms rise; plus broadband noise (one-pole high-pass 400 Hz), tau 16 ms
 * (-20 dB at ~37 ms; EST 25..45 ms). The tone / noise balance is a GUESS.
 * HH (EST): white noise through a two-pole high-pass, 4 kHz Q 0.707 (11-13 dB/oct below 2.5 kHz, flat above 5: EST -11), tau 17 ms (-20 dB at
 * ~39 ms, EST 32..41). The levels: RMS over the first 100 ms, SN 7 dB and HH 21 dB under BD (EST, low; one RHYTHM
 * setting) — tests/host/tribe_test.c checks them. */
#define BD_HZ 44.0f
#define BD_TAU 0.055f
#define BD_RISE 0.002f
#define SN_HZ 244.0f
#define SN_TAU 0.009f
#define SN_RISE 0.001f
#define SN_NTAU 0.016f
#define HH_TAU 0.017f
#define BD_GAIN 1.0f
#define SN_TONE 1.03f
#define SN_NOISE 0.50f
#define HH_GAIN 0.205f

typedef struct {
    float ph, amp, rise;                        /* phase in turns, decay level, rise 0..1 */
    uint8_t first;                              /* in the first half-cycle */
} ping_t;
static ping_t bd, sn;
static float sn_namp, sn_hp, hh_amp, hh_s1, hh_s2;
static volatile uint8_t drum_ev;                /* bits: TRIBE_BD .. triggered, taken by the render */
static float k_bd, k_sn, k_snn, k_hh, r_bd, r_sn, hp_sn_g, hh_g, hh_r;

static inline float ping(ping_t *p, float hz, float k, float rinc)
{
    float s, inc = hz * (1.0f / SR);
    if (p->amp < 1e-5f)
        return 0.0f;
    if (p->rise < 1.0f)
        p->rise = fm_minf(1.0f, p->rise + rinc);
    s = 0.5f - 0.5f * fm_cosf(FM_PI * p->rise); /* the rise, raised-cosine */
    s *= p->amp * fm_sin_turns(p->ph + 0.25f);  /* a cosine: full at the trigger */
    if (p->first) {                             /* the first half-cycle, stretched */
        inc *= 0.8f;
        if (p->ph >= 0.5f)
            p->first = 0;
    }
    p->ph += inc;
    if (p->ph >= 1.0f)
        p->ph -= 1.0f;
    p->amp *= k;
    return s;
}

static inline void ping_hit(ping_t *p)
{
    p->ph = 0.0f;
    p->amp = 1.0f;
    p->rise = 0.0f;
    p->first = 1;
}

static inline float drums(void)
{
    float s, n, v, hp;
    s = BD_GAIN * ping(&bd, BD_HZ, k_bd, r_bd);
    s += SN_TONE * ping(&sn, SN_HZ, k_sn, r_sn);
    if (sn_namp > 1e-5f || hh_amp > 1e-5f) {
        n = white();
        v = (n - sn_hp) * hp_sn_g;              /* SN noise: a one-pole high-pass (TPT) */
        hp = n - (v + sn_hp);
        sn_hp += v + v;
        s += SN_NOISE * sn_namp * hp;
        sn_namp *= k_snn;
        /* HH: a TPT state-variable filter's high-pass, 4 kHz, Q 0.707 */
        hp = (n - (hh_r + hh_g) * hh_s1 - hh_s2) / (1.0f + hh_r * hh_g + hh_g * hh_g);   /* hh_r: 2R */
        v = hh_g * hp;
        {
            float bp = v + hh_s1;
            hh_s1 = bp + v;
            v = hh_g * bp;
            hh_s2 = hh_s2 + v + v;
        }
        s += HH_GAIN * hh_amp * hp;
        hh_amp *= k_hh;
    }
    return s;
}

/* ----------------------------------------------------------------- the LFO --- */
/* From the manual (research doc, Synth voice): RATE over FAST ~1 Hz..5 kHz or SLOW ~0.05..18 Hz, exponential; INT.
 * fully left none; TARGET VCO, VCO+VCF or VCF; 1SHOT plays the first half-cycle at note on, then stops; key sync
 * restarts the phase at note on in FAST and 1SHOT; WAVE saw, triangle, square, and v2's sample and hold.
 * GUESS (no measurements yet): 1SHOT runs at the SLOW range; depth at INT full is +-1 octave of pitch and +-3 octaves
 * of cutoff; INT's taper is square-law (fine at the bottom); the saw falls. The waves, over a phase 0..1:
 *   saw 1 - 2p (falling), triangle 0 -> 1 -> -1 -> 0, square +1 then -1, S&H a new random level each cycle.
 * In 1SHOT the half-cycle ends at 0 for saw and triangle (no modulation after), and the square's after it too. */
#define LFO_FAST_LO 1.0f
#define LFO_FAST_HI 5000.0f
#define LFO_SLOW_LO 0.05f
#define LFO_SLOW_HI 18.0f
#define LFO_PITCH_SEMI 12.0f
#define LFO_CUT_OCT 3.0f
static float lfo_ph, lfo_sh, lfo_now;
static uint8_t lfo_done;                        /* 1SHOT: the half-cycle played */

float tribe_lfo_hz(float r, int mode)
{
    r = fm_clampf(r, 0.0f, 1.0f);
    if (mode == TRIBE_LFO_FAST)
        return LFO_FAST_LO * fm_exp2f(r * 12.2877124f);     /* log2(5000 / 1) */
    return LFO_SLOW_LO * fm_exp2f(r * 8.4918531f);          /* log2(18 / 0.05) */
}
float tribe_lfo_out(void) { return lfo_now; }

static inline float lfo_wave(uint8_t w, float p)
{
    if (w == TRIBE_LFO_SAW)
        return 1.0f - 2.0f * p;
    if (w == TRIBE_LFO_TRI)
        return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
    if (w == TRIBE_LFO_SQR)
        return p < 0.5f ? 1.0f : -1.0f;
    return lfo_sh;
}

static inline float lfo_step(float inc, uint8_t w, uint8_t mode)   /* one sample, -1..1 */
{
    float v;
    if (mode == TRIBE_LFO_1SHOT && lfo_done)
        return lfo_now = (w == TRIBE_LFO_SH ? lfo_sh : 0.0f);
    v = lfo_wave(w, lfo_ph);
    lfo_ph += inc;
    if (mode == TRIBE_LFO_1SHOT && lfo_ph >= 0.5f)
        lfo_done = 1;
    if (lfo_ph >= 1.0f) {
        lfo_ph -= (float)(int32_t)lfo_ph;
        lfo_sh = white();
    }
    return lfo_now = v;
}

static void lfo_sync(uint8_t mode, uint8_t w)   /* note on */
{
    if (mode == TRIBE_LFO_SLOW)
        return;
    lfo_ph = 0.0f;
    lfo_done = 0;
    if (w == TRIBE_LFO_SH)
        lfo_sh = white();
}

/* ------------------------------------------------------------- the render --- */
static float dc_x, dc_y;                        /* the output's DC block (~5 Hz) */

static inline float soft_limit(float x)         /* linear to 0.8, then a tanh knee to 1 */
{
    float a = fm_fabsf(x);
    if (a <= 0.8f)
        return x;
    a = 0.8f + 0.2f * fm_tanhf((a - 0.8f) * 5.0f);
    return x < 0.0f ? -a : a;
}

void tribe_render(float *out, uint32_t n)
{
    uint32_t i, ev;
    float cut_t, peak_t, k_sm, lvl, rhy, nz, lfo_inc, amt, pdep, cdep, pitch, dt0;
    k35_t c;
    if (live_seq != par_seq) {
        live_seq = par_seq;
        if (par.lfo_mode != live.lfo_mode)
            lfo_done = 1;                       /* (switching into 1SHOT waits for a note) */
        live = par;
    }
    if (ev_on) {                                /* note on: the EG from the top (and the VCO's phase kept) */
        ev_on = 0;
        eg_gate = 1;
        eg_e = 1.0f;
        lfo_sync(live.lfo_mode, live.lfo_wave);
    }
    if (ev_off) {
        ev_off = 0;
        eg_gate = 0;
    }
    ev = drum_ev;
    if (ev) {
        drum_ev = 0;
        if (ev & 1u << TRIBE_BD)
            ping_hit(&bd);
        if (ev & 1u << TRIBE_SN) {
            ping_hit(&sn);
            sn_namp = 1.0f;
        }
        if (ev & 1u << TRIBE_HH)
            hh_amp = 1.0f;
    }
    pitch = note_pitch;
    dt0 = 440.0f * fm_exp2f((pitch - 69.0f) * (1.0f / 12.0f)) / SR2;
    lfo_inc = tribe_lfo_hz(live.lfo_rate, live.lfo_mode) * (1.0f / SR);
    amt = live.lfo_int * live.lfo_int;
    pdep = live.lfo_target != TRIBE_LFO_VCF ? amt * LFO_PITCH_SEMI : 0.0f;
    cdep = live.lfo_target != TRIBE_LFO_VCO ? amt * LFO_CUT_OCT / CUT_OCT : 0.0f;   /* in CUTOFF's 0..1 units */
    cut_t = live.cutoff;
    peak_t = live.peak;
    lvl = live.level * 0.9f;
    rhy = live.rhythm;
    nz = live.noise;
    k_sm = 1.0f - fm_expf(-1.0f / (0.005f * SR));
    for (i = 0; i < n; i++) {
        float s, x, l = lfo_step(lfo_inc, live.lfo_wave, live.lfo_mode);
        cut_s += (cut_t - cut_s) * k_sm;
        peak_s += (peak_t - peak_s) * k_sm;
        vco_dt = pdep != 0.0f ? dt0 * fm_exp2f(l * pdep * (1.0f / 12.0f)) : dt0;
        if (vco_dt > 0.45f)
            vco_dt = 0.45f;
        x = cut_s + l * cdep;                   /* CUTOFF's exponential taper carries on past its ends */
        k35_coef(&c, CUT_LO * fm_exp2f(fm_clampf(x, -0.5f, 1.2f) * CUT_OCT), fm_maxf(0.01f, K_MAX * peak_s));
        x = 0.55f * (vco(live.wave) + nz * white());
        hb_push(k35(&c, x));
        x = 0.55f * (vco(live.wave) + nz * white());
        hb_push(k35(&c, x));
        s = hb_out() * eg_step(live.eg) * lvl;
        s += drums() * rhy;
        x = s - dc_x + 0.99929f * dc_y;         /* DC block, 5 Hz */
        dc_x = s;
        dc_y = x;
        out[i] = soft_limit(x);
    }
    z1 = fm_flush(z1);
    z2 = fm_flush(z2);
    z3 = fm_flush(z3);
    hh_s1 = fm_flush(hh_s1);
    hh_s2 = fm_flush(hh_s2);
}

/* ---------------------------------------------------- from the main loop --- */
void tribe_set(const tribe_params_t *p)
{
    par = *p;
    if (par.wave >= TRIBE_NWAVE)
        par.wave = TRIBE_SAW;
    if (par.eg >= TRIBE_NEG)
        par.eg = TRIBE_DECAY;
    if (par.lfo_target >= TRIBE_NTARGET)
        par.lfo_target = TRIBE_LFO_VCO;
    if (par.lfo_mode >= TRIBE_NMODE)
        par.lfo_mode = TRIBE_LFO_FAST;
    if (par.lfo_wave >= TRIBE_NLWAVE)
        par.lfo_wave = TRIBE_LFO_TRI;
    par_seq++;
}

void tribe_note_on(float pitch)
{
    note_pitch = pitch;
    ev_off = 0;
    ev_on = 1;
}
void tribe_note_glide(float pitch) { note_pitch = pitch; }
void tribe_note_off(void) { ev_off = 1; }
void tribe_drum(int which)
{
    if (which >= 0 && which < TRIBE_NDRUM)
        drum_ev |= (uint8_t)(1u << which);
}

void tribe_panic(void)
{
    eg_gate = 0;
    eg_a = eg_vca = 0.0f;
    bd.amp = sn.amp = sn_namp = hh_amp = 0.0f;
    z1 = z2 = z3 = 0.0f;
}

void tribe_init(void)
{
    float g;
    k_eg = fm_expf(-1.0f / (EG_TAU * SR));
    k_rel = fm_expf(-1.0f / (REL_TAU * SR));
    k_vca = 1.0f - fm_expf(-1.0f / (VCA_TAU * SR));
    k_bd = fm_expf(-1.0f / (BD_TAU * SR));
    k_sn = fm_expf(-1.0f / (SN_TAU * SR));
    k_snn = fm_expf(-1.0f / (SN_NTAU * SR));
    k_hh = fm_expf(-1.0f / (HH_TAU * SR));
    r_bd = 1.0f / (BD_RISE * SR);
    r_sn = 1.0f / (SN_RISE * SR);
    g = fm_tanf(FM_PI * 400.0f / SR);
    hp_sn_g = g / (1.0f + g);
    hh_g = fm_tanf(FM_PI * 4000.0f / SR);
    hh_r = 1.41421356f;                         /* the SVF's 2R: Q 0.707 */
    cut_s = par.cutoff;
    peak_s = par.peak;
    lfo_done = 1;
    lfo_sh = 0.0f;
    live = par;
    live_seq = par_seq;
}
