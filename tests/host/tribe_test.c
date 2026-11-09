/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* FMtribe's engine against the research doc's estimates (CLAUDE.md), on the host:
 *   cc -O2 -ffp-contract=off -Ibase/include -Iapp/dsp -o tribe_test tests/host/tribe_test.c -lm && ./tribe_test [dir]
 * With a directory, it also writes the renders as 16-bit WAVs there, to listen to. Built from tests/run_tests.sh. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../app/dsp/tribe.c"                 /* the unit itself: its statics are in reach */

static int fails;
#define CHECK(cond, ...) do { if (cond) printf("  ok   "); else { printf("  FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define NMAX (44100 * 3)
static float buf[NMAX];
static const char *wav_dir;

static void wav(const char *name, const float *x, int n)
{
    char path[512];
    FILE *f;
    int i;
    uint32_t v;
    if (!wav_dir)
        return;
    snprintf(path, sizeof path, "%s/%s", wav_dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    v = 36 + 2 * (uint32_t)n;
    fwrite("RIFF", 1, 4, f); fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    { uint16_t h[2] = {1, 1}; fwrite(h, 2, 2, f); }
    v = 44100; fwrite(&v, 4, 1, f); v = 88200; fwrite(&v, 4, 1, f);
    { uint16_t h[2] = {2, 16}; fwrite(h, 2, 2, f); }
    fwrite("data", 1, 4, f); v = 2 * (uint32_t)n; fwrite(&v, 4, 1, f);
    for (i = 0; i < n; i++) {
        int16_t s = (int16_t)lrintf(fmaxf(-1.f, fminf(1.f, x[i])) * 32767.f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

static void render(float *x, int n)
{
    int i;
    for (i = 0; i < n; i += 256)
        tribe_render(x + i, (uint32_t)(n - i < 256 ? n - i : 256));
}

static void reset(const tribe_params_t *p)
{
    tribe_panic();
    memset(hb_buf, 0, sizeof hb_buf);
    dc_x = dc_y = 0;
    tribe_set(p);
    par_seq++;
    tribe_init();
}

static double rms(const float *x, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}
static double db(double v) { return 20.0 * log10(v + 1e-12); }

static double freq_zc(const float *x, int n)    /* rising zero crossings, interpolated */
{
    int i, c = 0;
    double first = -1, last = 0;
    for (i = 1; i < n; i++)
        if (x[i - 1] < 0 && x[i] >= 0) {
            double t = i - 1 + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0)
                first = t;
            last = t;
            c++;
        }
    return c > 1 ? (c - 1) * 44100.0 / (last - first) : 0;
}

static double goertzel(const float *x, int n, double hz)   /* amplitude of one frequency */
{
    double w = 2 * M_PI * hz / 44100, cw = 2 * cos(w), s1 = 0, s2 = 0;
    int i;
    for (i = 0; i < n; i++) {
        double win = 0.5 - 0.5 * cos(2 * M_PI * i / (n - 1)), s = x[i] * win + cw * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return sqrt(s1 * s1 + s2 * s2 - cw * s1 * s2) / (n * 0.25);
}

/* the filter alone, linear (a small input): gain at f for a cutoff, through the 2x path and the decimator */
static double filter_gain(float cut01, float peak01, double hz)
{
    k35_t c;
    int i, n = 22050;
    double ph = 0, a = 0.01;
    z1 = z2 = z3 = 0;
    memset(hb_buf, 0, sizeof hb_buf);
    k35_coef(&c, tribe_cutoff_hz(cut01), fmaxf(0.01f, K_MAX * peak01));
    for (i = 0; i < n; i++) {
        hb_push(k35(&c, (float)(a * sin(ph)))); ph += M_PI * hz / 44100;
        hb_push(k35(&c, (float)(a * sin(ph)))); ph += M_PI * hz / 44100;
        buf[i] = hb_out();
    }
    return goertzel(buf + n / 2, n / 2, hz) / a;
}

int main(int argc, char **argv)
{
    tribe_params_t p = {0.6f, 0.0f, 0.0f, 1.0f, 1.0f, TRIBE_SAW, TRIBE_DECAY};
    int i, n;
    double f, g1, g2;
    wav_dir = argc > 1 ? argv[1] : 0;
    tribe_init();

    printf("VCF (Korg35, linear)\n");
    {
        float c = 0.5f;                          /* ~1.2 kHz */
        double fc = tribe_cutoff_hz(c);
        g1 = filter_gain(c, 0.0f, fc * 4);
        g2 = filter_gain(c, 0.0f, fc * 8);
        CHECK(fabs(db(g1 / g2) - 12.0) < 1.5, "slope above cutoff %.1f dB/oct (2-pole: 12)", db(g1 / g2));
        g1 = filter_gain(c, 0.0f, fc / 8);
        CHECK(fabs(db(g1)) < 1.0, "pass band %.2f dB at fc/8", db(g1));
        g1 = filter_gain(c, 0.0f, fc);
        CHECK(db(g1) < -3.0 && db(g1) > -10.0, "%.2f dB at cutoff, PEAK 0 (a low-Q 2-pole)", db(g1));
        g2 = filter_gain(c, 0.85f, fc);
        CHECK(db(g2) > db(g1) + 6.0, "PEAK 0.85 lifts the cutoff by %.1f dB", db(g2) - db(g1));
    }

    printf("VCF self-oscillation (PEAK full, no input)\n");
    for (i = 0; i < 5; i++) {
        static const float C[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        k35_t c;
        int j;
        n = 44100;
        z1 = z3 = 0;
        z2 = 0.01f;                              /* a nudge */
        memset(hb_buf, 0, sizeof hb_buf);
        k35_coef(&c, tribe_cutoff_hz(C[i]), K_MAX);
        for (j = 0; j < n; j++) {
            hb_push(k35(&c, 0.0f));
            hb_push(k35(&c, 0.0f));
            buf[j] = 0.55f * hb_out();           /* the engine's scale with LEVEL 1 (0.9) and the EG at full */
        }
        f = freq_zc(buf + n / 2, n / 2);
        CHECK(f > 0 && fabs(f / tribe_cutoff_hz(C[i]) - 1) < 0.25 && rms(buf + n / 2, n / 2) > 0.05,
              "cutoff %.2f: oscillates at %.0f Hz (cutoff %.0f), %.1f dBFS", C[i], f, tribe_cutoff_hz(C[i]),
              db(rms(buf + n / 2, n / 2)));
        if (i == 0) {
            CHECK(f > 70 && f < 100, "lowest self-oscillation %.0f Hz (EST ~83)", f);
            wav("selfosc_low.wav", buf, n);
        }
    }

    printf("VCO (filter open, gate EG)\n");
    {
        static const char *W[3] = {"saw", "triangle", "square"};
        int w;
        for (w = 0; w < 3; w++) {
            double f0 = 440.0 * pow(2.0, (100 - 69) / 12.0), harm = 0, all;
            int k;
            p.cutoff = 1.0f; p.peak = 0.0f; p.wave = (uint8_t)w; p.eg = TRIBE_GATE;
            reset(&p);
            tribe_note_on(100.0f);
            n = 32768;
            render(buf, n);
            all = rms(buf + 8192, 16384);
            for (k = 1; k * f0 < 21000; k++)
                harm += pow(goertzel(buf + 8192, 16384, k * f0), 2) / 2;
            harm = sqrt(harm);
            CHECK(harm / all > 0.998, "%s at %.0f Hz: aliases %.1f dB under the harmonics", W[w], f0,
                  db(sqrt(fmax(1e-9, 1 - pow(harm / all, 2))) / (harm / all)));
            p.cutoff = 0.62f;
            reset(&p);
            tribe_note_on(45.0f);
            n = 44100;
            render(buf, n);
            f = freq_zc(buf + 4410, n - 4410);
            CHECK(fabs(f / 110.0 - 1) < 0.01, "%s: pitch %.2f Hz (A2 110)", W[w], f);
            {
                char nm[32];
                snprintf(nm, sizeof nm, "vco_%s.wav", W[w]);
                wav(nm, buf, n);
            }
        }
    }

    printf("Decay EG (EST: -3 dB at 100 ms, -10.5 at 400, -22 at 800, silent by ~1.3 s)\n");
    {
        static const int MS[3] = {100, 400, 800};
        static const double DB[3] = {-3.0, -10.5, -22.0};
        float lv[NMAX / 256 + 1];
        int b, j;
        p.cutoff = 0.62f; p.peak = 0.2f; p.wave = TRIBE_SAW; p.eg = TRIBE_DECAY;
        reset(&p);
        tribe_note_on(57.0f);
        n = 44100 * 2;
        for (b = 0, i = 0; i < n; i += 256, b++) {
            tribe_render(buf + i, 256);
            lv[b] = tribe_eg_level();
        }
        for (j = 0; j < 3; j++) {
            double l = db(lv[MS[j] * 441 / 2560]);
            CHECK(fabs(l - DB[j]) < 1.0, "%d ms: %.1f dB", MS[j], l);
        }
        CHECK(lv[1300 * 441 / 2560] < 1e-3, "1.3 s: %.1f dB", db(lv[1300 * 441 / 2560]));
        wav("eg_decay.wav", buf, n);
        reset(&p);                               /* a short note: the gate closes it */
        tribe_note_on(57.0f);
        render(buf, 4410);
        tribe_note_off();
        render(buf + 4410, 44100);
        CHECK(rms(buf + 4410 + 4410, 441) < 0.01 * rms(buf + 4410 - 441, 441), "released at 100 ms: -40 dB 100 ms later");
    }

    printf("Drums\n");
    {
        double r[3];
        static const char *D[3] = {"bd", "sn", "hh"};
        int d;
        p.level = 0.0f; p.rhythm = 1.0f;
        for (d = 0; d < 3; d++) {
            char nm[16];
            reset(&p);
            tribe_drum(d);
            n = 22050;
            render(buf, n);
            r[d] = db(rms(buf, 4410));
            snprintf(nm, sizeof nm, "%s.wav", D[d]);
            wav(nm, buf, n);
            if (d == TRIBE_BD) {
                double a1, a2;
                f = freq_zc(buf + 1323, 8820);
                CHECK(f > 42 && f < 46, "BD %.1f Hz (EST 43-45)", f);
                a1 = rms(buf + 4410, 2205);      /* 100-150 ms against 200-250 ms */
                a2 = rms(buf + 8820, 2205);
                CHECK(fabs(0.1 / log(a1 / a2) * 1000 - 55) < 6, "BD tau %.0f ms (EST 53-56)", 0.1 / log(a1 / a2) * 1000);
                for (i = 0; i < 300 && fabs(buf[i]) < 0.5 * BD_GAIN; i++)
                    ;
                CHECK(i < 120, "BD reaches half scale at %.1f ms (EST ~2 ms step)", i / 44.1);
            }
            if (d == TRIBE_HH) {                 /* band levels: 1.25 kHz, 2.5 kHz, 10 kHz */
                double b[3] = {0, 0, 0};
                static const double C[3] = {1250, 2500, 10000};
                int k, j;
                for (j = 0; j < 3; j++) {
                    for (k = -15; k <= 15; k++)
                        b[j] += pow(goertzel(buf, 4096, C[j] * (1 + k * 0.012)), 2);
                    b[j] = 10 * log10(b[j] / 31);
                }
                CHECK(b[1] - b[0] > 8 && b[1] - b[0] < 15, "HH %.1f dB/oct below 2.5 kHz (EST ~11)", b[1] - b[0]);
                CHECK(b[2] - b[1] < 9, "HH %.1f dB from 2.5 to 10 kHz (EST flat above ~5)", b[2] - b[1]);
            }
        }
        printf("       RMS 0-100 ms: BD %.1f, SN %.1f, HH %.1f dBFS\n", r[0], r[1], r[2]);
        CHECK(fabs(r[0] - r[1] - 7) < 1.5, "SN %.1f dB under BD (EST 7)", r[0] - r[1]);
        CHECK(fabs(r[0] - r[2] - 21) < 1.5, "HH %.1f dB under BD (EST 21)", r[0] - r[2]);
    }

    printf("Everything at once (the limiter)\n");
    {
        float pk = 0;
        p.cutoff = 0.8f; p.peak = 1.0f; p.level = 1.0f; p.rhythm = 1.0f; p.noise = 1.0f; p.eg = TRIBE_GATE;
        reset(&p);
        tribe_note_on(36.0f);
        tribe_drum(0); tribe_drum(1); tribe_drum(2);
        render(buf, 44100);
        for (i = 0; i < 44100; i++) {
            if (!(buf[i] == buf[i])) { pk = 99; break; }
            pk = fmaxf(pk, fabsf(buf[i]));
        }
        CHECK(pk <= 1.0f, "peak %.3f (no NaN, never past full scale)", pk);
        wav("all.wav", buf, 44100);
    }

    printf(fails ? "tribe_test: %d FAILED\n" : "tribe_test: all passed\n", fails);
    return fails != 0;
}
