# FMtribe licensing

FMtribe is free software under the **Mozilla Public License 2.0** (`MPL-2.0`, full text in `LICENSE`), file by file.
If you change one of its files and share the result, share that file's source under the MPL.

## Clean room

FMtribe recreates the behaviour and sound of the Korg monotribe without copying anyone's code:

- No Korg code, firmware or data. The monotribe's firmware is not decoded, disassembled or read.
- No code from other monotribe emulations. Clonotribe (GPL-3.0) is not ported, translated or used as a sound
  reference; FMtribe's DSP is written from published papers (Pirkle's Korg35 filter, Zavalishin's *The Art of VA
  Filter Design*), Korg's public documentation, and measurements and estimates from public recordings of real
  units.
- No samples. Every sound is synthesised.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| The platform: `base/` (boot, USB and USB audio, Bluetooth MIDI, the TRS jack, storage, the update path, screen, panel, the screen reader, the host simulator and the browser emulator, build and install tools) | [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal and [felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments, under MPL-2.0 with his permission | MPL-2.0 |
| The app's starting point: `app/` and `.github/workflows/release.yml` | fm1-base's `template/` | MPL-2.0 |
| The build and test scripts' shape | [GIO-1](https://github.com/GuinanNo1/fm1-gio) | MPL-2.0 |
| FMtribe's own: the instrument, its DSP, sequencer and screens | FMtribe | MPL-2.0 |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Fonts: Barlow Semi Condensed (The Barlow Project Authors), Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `base/assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| JieLi Bluetooth libraries, linked from the SDK when built with Bluetooth | Apache-2.0 | the AC79 SDK |

## Trademarks

"Korg" and "monotribe" are trademarks of Korg Inc. "M-VAVE" and "FM-1" are trademarks of their respective owners.
"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. They are used here only to say what FMtribe
recreates and runs on. FMtribe is independent firmware and is not affiliated with, endorsed by or supported by any
of them.
