# FMtribe

FMtribe is open-source firmware for the M-VAVE FM-1 that recreates the sound and controls of a classic monophonic
analogue groove box: one VCO, a two-pole MS-style filter, an LFO, three analogue-style drums and an eight-step
sequencer. It's a clean-room recreation, written from scratch.

![FMtribe while a key is held](manual/img/screen-playing.png "FMtribe on the FM-1's screen")

This is version {{VERSION}}, an early development build. You install it at your own risk, and the installer can put
M-VAVE's own firmware back.

- [Try it in the browser](emu/). It runs the same code as the FM-1, with sound.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Quickstart](quickstart/) gets you installed and playing in five minutes.
- The [Manual](manual/) explains the screen, the controls and MIDI.
- The [Mirror](mirror/) shows the FM-1's screen live on your computer, and lets you play it from there.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/GuinanNo1/FMtribe) is under MPL-2.0. It's built on fm1-base and felucca-bsp,
  which come from Felucca by Leo Kuroshita, Hügelton Instruments.

Korg and monotribe are trademarks of Korg Inc.; M-VAVE and FM-1 are trademarks of their owners. FMtribe is not
affiliated with either.
