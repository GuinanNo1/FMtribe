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
UI (0.4): pages on buttons (app.c PAGE[]), HOME first (the start page, 1/2: LEVEL, RHYTHM, CUTOFF, PEAK; RANGE MOTION 2/2: RANGE, MOTION), then the monotribe's panel left to
right: a button shows its first page,
pressed again its next; K1–K4 edit the page; the header shows the page name (left, in place of the app name) and x/y (right). ENV VCO 1/2 (NOISE
RANGE; RANGE WIDE/NARROW = slow/fast glide (tribe glide, legato only; times GUESS), KEY none) and VCF VCA 2/2 (CUTOFF PEAK LEVEL EG); LFO (RATE INT TARGET
MODE); ARP DRUMS (RHYTHM, BD/SN/HH DECAY = a hardware mod, not stock; F#3 G#3 A#3 = BD SN HH pads, recorded with REC;
every other key plays the synth); SEQ STEP 1/2 (TEMPO SWING STEPS GATE) and
EDIT 2/2 (STEP NOTE GATE ON, piano roll); GLO MIDI 1/3 (SYNTH CH DRUM CH SYNC CLOCK OUT), GLOBAL 2/3 (BLE MIDI, KEY
LIGHT, GLOW, PREVIEW = DRUM PREVIEW, default on), DEVICES 3/3 (K1 picks, SAVE connects/lets go; scans while the page is up). Constant everywhere: OCT-/+
the octave; SELECT the VCO WAVE (on SEQ pages the step cursor ed_slot); ALGO the LFO WAVE on LFO; PRESET the part on SEQ
pages (part_pick; also the black part keys on STEP). EDIT edits the picked part: synth piano roll, or a drum's 16
slots (cursor by slot, K4 ON its hits, NOTE/GATE '-'). SEL, EDIT free (the user will assign SEL later). FX and F#5 (KEY_ROLL, on DRUMS and STEP)
toggle ROLL: DRUMS pads roll while held at ROLL RATE (ALGO on DRUMS, 8th..128th); on STEP with a drum part and playing,
GATE TIME + white key = roll (Ver.2), stops on GATE release. seq_roll_live (free-running when stopped);
pat.roll[drum][step] recorded with REC, played through the step; INITIALIZE RHYTHM/ALL clear. Keep to the monotribe's controls (no per-drum levels: RHYTHM only). Settings are PAR[] (knob 0..127 or switch), saved as "FMT9" with
the pattern. pget/pset reach pattern-backed settings (STEPS, EDIT's STEP/NOTE/GATE/ON) and the BLE device pick (NSAVED).
Sequencer (0.3, UI 0.4): app/seq/seq.c (o2), tests/host/seq_test.c, app/tests/seq.txt. 8 steps x 2 slots, parts
SYNTH/BD/SN/HH, per-part active steps, gate per synth slot (127 tie), 16-step synth, swing, 96 ppqn ticks in the
audio interrupt (app_render splits the block at ticks and sample-timed note offs), MIDI clock in (SYNC MIDI, 4 ticks
a clock) and out; seq_gate_live (GATE TIME). STEP is the monotribe's step row on the keys: 8-step parts spread, step s on white key 2s (blank keys between;
key_step), the 16-step synth on all 16 white keys; they are the step buttons and their lights the step LEDs (off: inactive/past 8; dim: active, empty; mid:
note/hit; lit: playhead, or the synth cursor when stopped). Black: F#3 G#3 A#3 C#4 = BD SN HH SYNTH (hold a drum's
key + step = half-step), D#4 ACTIVE STEP (hold + steps), F#4 GATE TIME: held while playing, white keys are a ribbon
(left 1.4 %, right 127/tie), recorded into each synth step with REC; held while stopped, the v2 multi-function key
(steps 1-4 INITIALIZE ALL/SYNTH/RHYTHM/ACTIVE, 5 S&H, 6 16-STEP SYNTH, 7 GATE TIME HOLD = Korg's: recording pitch
keeps recorded gates, 8 HALF TEMPO). MOTION VOLUME: pat.vol[slot] (SEQ_NO_VOL none) via seq_cb_motion ->
tribe_motion (replaces LEVEL); ACTIVE STEP held while playing = keys ribbon (left 0, right 127), recorded with REC;
LEVEL turned while recording is recorded too; MOTION switch plays them back. REC arms;
armed, keys play+record synth on any page (on STEP the white keys); on STEP the BD/SN/HH part keys play+record
drums (pad()). Manual section 4
"Setting gate times" documents the gate methods. Manual screenshots: app/tests/screens.txt -> app/docs/img.
Not yet: Flux.
LFO (0.3): tribe.c, rates from the manual; depth (±1 oct pitch, ±3 oct cutoff), INT square-law taper, 1SHOT on the
SLOW range and the falling saw are GUESSES.
Next: user testing in the emulator; then Flux (continuous synth recording), drum rolls, ribbon modes.
