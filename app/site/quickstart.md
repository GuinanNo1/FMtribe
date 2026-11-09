# Quickstart

Get FMtribe onto your FM-1 and making sound in five minutes. This build has the synth voice, the LFO, the drums and
the step sequencer.

---

## 1. Try it first

Open [the emulator](../emu/). It runs the same code as the FM-1, with sound. Click the keys, or play your
computer keyboard. Nothing needs installing.

## 2. Install

1. Use Chrome or Edge on a computer. Safari and Firefox don't have Web MIDI.
2. Connect the FM-1 to the computer with a USB-C data cable, and switch it on.
3. Open [the installer](../install/), press **Connect**, and allow MIDI when the browser asks.
4. Press **Install** and wait. Don't unplug the cable while it writes. The FM-1 restarts on its own.

To go back to M-VAVE's firmware, or to another one, open the same installer and use its last section.

## 3. Play

Press a page button, then turn K1 to K4 to edit that page. Press the button again for its next page; the header
shows which one, such as VCF VCA 2/2. FMtribe starts on HOME.

| Page (button) | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| HOME 1/2 (HOME) | LEVEL | RHYTHM | CUTOFF | PEAK |
| RANGE MOTION 2/2 (HOME) | RANGE | MOTION | | |
| VCO 1/2 (ENV) | NOISE | RANGE | | |
| VCF VCA 2/2 (ENV) | CUTOFF | PEAK | LEVEL | EG |
| LFO (LFO) | RATE | INT | TARGET | MODE |
| DRUMS (ARP) | RHYTHM | BD DECAY | SN DECAY | HH DECAY |
| STEP 1/2 (SEQ) | TEMPO | SWING | STEPS | GATE |
| EDIT 2/2 (SEQ) | STEP | NOTE | GATE | ON |
| MIDI 1/3 (GLO) | SYNTH CH | DRUM CH | SYNC | CLOCK OUT |
| GLOBAL 2/3 (GLO) | BLE MIDI | KEY LIGHT | GLOW | PREVIEW |
| DEVICES 3/3 (GLO) | DEVICE | | | |

On every page, OCT− and OCT+ set the octave and SELECT the VCO's wave. On the SEQ pages, SELECT moves the step cursor
and PRESET picks the part (SYNTH, BD, SN, HH). On LFO, ALGO sets the LFO's wave.

| Do this | And |
| --- | --- |
| Play the keys | The synth sounds. Each new key starts its envelope again. |
| On STEP (SEQ), the F♯3 G♯3 A♯3 black keys | Pick and play the bass drum, snare, hi-hat. |
| PLAY | Start or stop the pattern. |
| REC, then PLAY | Record: the keys land on the nearest step; on STEP, the part keys record the drums. |
| Turn MASTER | Volume. |
| SAVE | Keeps your settings for the next start (it lights when there's something to save). |

**A drum pattern in a minute:** press SEQ. On STEP the keys are the monotribe's step row: the first eight white keys
are steps 1–8, and their lights are the step LEDs. Press F♯3 to pick the bass drum and press white keys to switch
its steps on. G♯3 picks the snare, A♯3 the hi-hat and C♯4 the synth. Press PLAY.

**Gate times:** while it plays, hold F♯4 (GATE TIME) and touch the white keys: left is short, right is long. The
manual's "Setting gate times" has the rest.

## 4. Connect

- **USB-C:** the FM-1 is a MIDI device and a sound card. Record it from the input "FMtribe FM-1".
- **Bluetooth MIDI:** on a Mac, open Audio MIDI Setup › Window › Show MIDI Studio, click the Bluetooth button, then
  Connect next to FM-1. On an iPad or iPhone, connect from a music app's Bluetooth MIDI settings. Switch Bluetooth
  off or on with BLE MIDI on GLOBAL 2/3 (GLO).
- **A Bluetooth keyboard:** put it in pairing mode, press GLO until DEVICES 3/3 shows, turn K1 to pick it, press SAVE.
- **Screen mirror:** open [the Mirror page](../mirror/) in Chrome or Edge with the FM-1 on USB, and press Connect.
  Press Control to play it from the page.

## 5. If something goes wrong

- **No sound:** turn MASTER up, and LEVEL and CUTOFF (VCF VCA, ENV 2/2). A DECAY note fades after about a second; set EG to GATE there for held notes.
- **The installer can't find the FM-1:** close other programs using its MIDI port, try another cable (some are
  charge-only), and allow MIDI in the browser.
- **The FM-1 doesn't start after an install:** after two failed starts it opens in SAFE MODE, where the installer
  can still reach it. Install again, or put the stock firmware back.

The [manual](../manual/) has the details.
