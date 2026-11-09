# FMtribe

Open-source firmware for the M-VAVE FM-1 that recreates the sound and controls of a classic monophonic analogue
groove box (the Korg monotribe): one VCO, a two-pole MS-style filter, an LFO, three analogue-style drums and an
eight-step sequencer.

**Status: scaffolding.** The repository builds and runs fm1-base's template voice under the FMtribe name. The
instrument itself comes next.

FMtribe is a clean-room recreation. It contains no Korg code or firmware and no code from other emulations; its
sound is modelled from published circuit papers and fitted to public recordings of real units. See
[LICENSING.md](LICENSING.md).

## Plan

1. Synth voice: VCO (saw, triangle, square), Korg35-style zero-delay-feedback filter, the three EG modes, LFO.
2. Drums: kick, snare and hi-hat from measured estimates.
3. Sequencer: eight steps, synth and drum lanes, flux mode, active step.
4. MIDI: the stock monotribe map (channel 1 synth, channel 10 drums on 36/40/42, CCs 1/16/80–83/7/11) over USB,
   Bluetooth and TRS.
5. A side-by-side comparison against a real unit.

## Build

```
git clone --recursive https://github.com/GuinanNo1/FMtribe
cd FMtribe
./build.sh --host           # the host simulator: app/build/host/fmtribe_host
sh tests/run_tests.sh       # the tests
./build.sh                  # the firmware: app/build/fmtribe.fwsc (JieLi toolchain and AC79 SDK)
./build.sh --no-ble         # USB-only fallback build
```

See [BUILDING.md](BUILDING.md) for the toolchain.

Built on [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal and
[felucca-bsp](https://github.com/hugelton/felucca-bsp), from Felucca by Leo Kuroshita (Hügelton Instruments).

M-VAVE and FM-1 are trademarks of their owners; Korg and monotribe are trademarks of Korg Inc. FMtribe is
independent and is not affiliated with, endorsed by or supported by any of them.
