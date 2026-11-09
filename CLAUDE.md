# FMtribe — notes for coding sessions

Open-source (MPL-2.0) firmware for the M-VAVE FM-1 that recreates the Korg monotribe's sound and interface.
App in `app/`, fm1-base in `base/` (submodule, pinned at b0976f2). Build: `./build.sh --host`; test:
`sh tests/run_tests.sh`. See BUILDING.md.

## Clean-room rules (do not break)
- Do not read, port, translate or paraphrase Clonotribe's source (GPL-3.0), and do not use its audio as a reference.
- Do not decode, disassemble or read Korg's monotribe firmware.
- Write DSP from: the research doc ("Korg monotribe — emulation research for FM-1",
  https://claude.ai/artifact/Hfd18xumo1hYLJgNnDKKBd), Pirkle's Korg35 paper, Zavalishin's *The Art of VA Filter
  Design*, Korg's public manuals, and the estimates below.
- No samples; everything synthesised.

## Platform constraints
- `float` DSP only: no `double` (the build rejects it). Use `fastmath.h` (`fm_sinf`, `fm_exp2f`, ...).
- RAM is the constraint: ~16 KB free (.data+.bss 81,924 of 98,304 B in 0.1-alpha: template + BLE + BLE devices + mirror). Check the build's report; `ble_central = false` or `mirror = false` win RAM back if needed.
- DSP units go in `app/` subdirs listed in `app/fm1.toml` `include`/`o2` (as GIO-1's `dsp/gio.c`).
- Audio: 44.1 kHz, `APP_BLOCK` 256 frames, `app_render` in the audio interrupt. A second core is available if needed.
- fm1-base is beta; move the pin deliberately (BUILDING.md). Keep `./build.sh --no-ble` working.
- Package identity `FM-1_59` (GIO-1 is `FM-1_58`).

## MIDI defaults (stock monotribe map)
Channel 1 synth; channel 10 drums on notes 36 (BD), 40 (SN), 42 (HH); CCs 1, 16, 80–83, 7, 11.

## Estimates from public recordings (see research doc for evidence and confidence)
- VCF: 2-pole (12 dB/oct) Korg35-style, ZDF; self-oscillates cleanly ~83 Hz to >15 kHz; cutoff taper ~exponential.
- Decay EG (VCA): a = (exp(−t/τ) − c)/(1 − c), τ ≈ 365–395 ms, c ≈ 0.03–0.05, silent ~1.2–1.3 s; pitch-independent.
- BD: damped sine 43–45 Hz, no pitch sweep, ~2 ms trigger step, τ 53–56 ms.
- SN: damped ~244 Hz tone (τ ~9 ms) + broadband noise (−20 dB at 25–45 ms).
- HH: high-passed noise (~−11 dB/oct below ~5 kHz), −20 dB at 32–41 ms.
- Drum mix at one setting: SN ~7 dB, HH ~21 dB below BD.
- Unknown (leave for the prototype comparison): Attack and Gate EG times, gate/release, LFO ranges, ribbon span,
  VCO waveform shapes, VCA click, drum mix at RHYTHM max.

## Implementation order
VCO → Korg35 ZDF filter → EGs → LFO → drums → sequencer. Add a host unit test and a render-to-WAV scenario for
each, as GIO-1's `tests/host/` does.

Done (0.2): `app/dsp/tribe.c` — PolyBLEP/BLAMP VCO and the Korg35 (Pirkle AN-5) run 2x oversampled with a 35-tap
halfband; Decay/Gate/Attack EGs; BD/SN/HH. `tests/host/tribe_test.c` checks each against the estimates above (and
writes WAVs); `app/tests/sound.txt` covers the panel and MIDI map. Constants marked GUESS in tribe.c (Gate/Attack,
release ~12 ms, SN tone/noise balance) wait for the prototype comparison. Known gap: self-oscillation level rises
~2.5 dB from 80 Hz to 18 kHz; the recordings suggest it falls ~6 dB from 200 Hz to 3.5 kHz.
UI: pages on buttons (app.c PAGE[]): a button shows its first page, pressed again its next; K1–K4 edit the page;
the header's top right shows x/y (page of that button's pages). SEL VCO VCF (CUTOFF PEAK WAVE NOISE), ENV EG VCA
(EG LEVEL), LFO 1/2 (RATE INT TARGET MODE) and 2/2 (WAVE), ARP DRUMS (RHYTHM, BD/SN/HH DECAY; black keys F#-G#-A#
groups are BD SN HH pads, glowing with KEY LIGHT GUIDE), GLO GLOBAL 1/3 MIDI (SYNTH CH, DRUM CH, SYNC INT/MIDI,
CLOCK OUT), 2/3 (KEY LIGHT OFF/HELD/GUIDE, GLOW, BLE MIDI on/off). Settings are PAR[] (knob 0..127 or switch), saved as "FMT6" with the pattern. Lights via lights()
(plat_lights: lit held keys, dim guide keys, mid for the playhead). PAR[] has lo/hi; pget/pset reach pattern-backed
settings (STEPS, SEQ EDIT's STEP/NOTE/GATE/ON) and the BLE device pick, which are not in set.v (NSAVED).
Sequencer (0.3): app/seq/seq.c (o2), tests/host/seq_test.c, app/tests/seq.txt. 8 steps x 2 slots, parts
SYNTH/BD/SN/HH, per-part active steps, gate per synth slot (127 tie), 16-step synth, swing, 96 ppqn ticks in the
audio interrupt (app_render splits the block at ticks and sample-timed note offs), MIDI clock in (SYNC MIDI, 4 ticks
a clock) and out. SEQ 1/3 TEMPO PART GATE SWING (white keys = part's 16 slots, black F#3 G#3 A#3 C#4 = BD SN HH
SYNTH), SEQ STEPS 2/3 STEPS - - PART (white 1-8 active steps, black F#3 G#3 A#3 C#4 clear all / synth /
drums / reset active), SEQ EDIT 3/3 STEP NOTE GATE ON (keys write notes, cursor advances). PLAY start/stop (green,
red recording); REC arms; armed, keys play+record synth on any page, DRUMS pads record drums. Pattern in settings.
BLE devices: GLOBAL DEVICES 3/3 (K1 picks, SAVE connects/lets go; scans while the page is up).
SELECT/ALGO/PRESET free. Not yet: Flux, drum rolls, volume automation, ribbon modes, gate-time hold.
LFO (0.3): tribe.c, rates from the manual; depth (±1 oct pitch, ±3 oct cutoff), INT square-law taper, 1SHOT on the
SLOW range and the falling saw are GUESSES.
Next: user testing in the emulator; then Flux (continuous synth recording), drum rolls, ribbon modes.
