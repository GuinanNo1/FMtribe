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

Press a page button, then turn K1 to K4 to edit that page. Press the button again for its next page; the top right of
the screen shows which one, such as 1/2.

| Page (button) | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| VCO VCF (SEL) | CUTOFF | PEAK | WAVE | NOISE |
| EG VCA (ENV) | EG | LEVEL | | |
| LFO 1/2 (LFO) | RATE | INT | TARGET | MODE |
| LFO 2/2 (LFO) | WAVE | | | |
| DRUMS (ARP) | RHYTHM | BD DECAY | SN DECAY | HH DECAY |
| SEQ 1/3 (SEQ) | TEMPO | PART | GATE | SWING |
| SEQ STEPS 2/3 (SEQ) | STEPS | | | PART |
| SEQ EDIT 3/3 (SEQ) | STEP | NOTE | GATE | ON |
| GLOBAL 1/3 (GLO) | SYNTH CH | DRUM CH | SYNC | CLOCK OUT |
| GLOBAL 2/3 (GLO) | KEY LIGHT | GLOW | BLE MIDI | |
| DEVICES 3/3 (GLO) | DEVICE | | | |

| Do this | And |
| --- | --- |
| Play the keys | The synth sounds. Each new key starts its envelope again. |
| On DRUMS (ARP), the F♯ G♯ A♯ black keys | Bass drum, snare, hi-hat. The white keys still play the synth. |
| OCT− · OCT+ | Octave down or up. |
| PLAY | Start or stop the pattern. |
| REC, then PLAY | Record: the keys (and the DRUMS pads) land on the nearest step. |

**A drum pattern in a minute:** press SEQ. Press F♯3 to pick the bass drum, and press white keys to switch its
slots on: each white key is a step or the half of one, low to high. G♯3 picks the snare and A♯3 the hi-hat, C♯4
the synth. Press PLAY.
| Turn MASTER | Volume. |
| SAVE | Keeps your settings for the next start (it lights when there's something to save). |

## 4. Connect

- **USB-C:** the FM-1 is a MIDI device and a sound card. Record it from the input "FMtribe FM-1".
- **Bluetooth MIDI:** on a Mac, open Audio MIDI Setup › Window › Show MIDI Studio, click the Bluetooth button, then
  Connect next to FM-1. On an iPad or iPhone, connect from a music app's Bluetooth MIDI settings. Switch Bluetooth
  off or on with BLE MIDI on GLOBAL 2/3 (GLO).
- **A Bluetooth keyboard:** put it in pairing mode, press GLO until DEVICES 3/3 shows, turn K1 to pick it, press SAVE.
- **Screen mirror:** open [the Mirror page](../mirror/) in Chrome or Edge with the FM-1 on USB, and press Connect.
  Press Control to play it from the page.

## 5. If something goes wrong

- **No sound:** turn MASTER up, LEVEL (ENV page) and CUTOFF (SEL page). A DECAY note fades after about a second; set EG to GATE on the ENV page for held notes.
- **The installer can't find the FM-1:** close other programs using its MIDI port, try another cable (some are
  charge-only), and allow MIDI in the browser.
- **The FM-1 doesn't start after an install:** after two failed starts it opens in SAFE MODE, where the installer
  can still reach it. Install again, or put the stock firmware back.

The [manual](../manual/) has the details.
