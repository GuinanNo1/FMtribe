# FMtribe

FMtribe will recreate the sound and controls of a classic monophonic analogue groove box on the M-VAVE FM-1. This
build has the synth voice and the drums: one oscillator (saw, triangle or square) and noise into a two-pole
MS-style low-pass filter that self-oscillates, a VCA with three fixed envelopes, and a bass drum, snare and hi-hat.
The LFO, the sequencer and the ribbon modes come next. The sound is fitted to recordings of real units, so treat
it as an estimate until it has been compared side by side with one.

![The FMtribe screen at rest](img/screen-ready.png "FMtribe at rest")

---

## Contents

1. Install
2. The screen
3. Controls
4. Saving
5. MIDI and audio
6. The screen mirror
7. The screen reader
8. Troubleshooting

---

## 1. Install

To try it first, open the emulator. It runs the same code in your browser, with sound, and you can play it with a
mouse, a touch screen or your computer keyboard.

To install it from the browser, use Chrome or Edge. Connect the FM-1 by USB, open the installer and press Install.
The browser asks once to allow MIDI, so allow it. Don't unplug the cable while it writes.

To install it from a terminal, run `python3 base/tools/fm1_install.py fmtribe-VERSION.fwsc` from FMtribe's source.

To go back to the stock firmware, use the installer's last section, which writes M-VAVE's own firmware back. You
can recover an FM-1 that doesn't start, too. For details, see felucca-bsp's `tools/fm1_rescue.sh`.

---

## 2. The screen

![FMtribe while it plays: the five knobs as bars, SAW and ATTACK lit, the HH pad lit](img/screen-playing.png "Playing: the Attack envelope, the hi-hat just hit")

| Part | What it shows |
| --- | --- |
| Header | The name, and BLE, which lights while a computer or tablet is connected over Bluetooth. While you play a key, it shows the note. After a change, a message shows here for a moment, such as CUTOFF 100. |
| Knobs | CUTOFF, PEAK, NOISE, LEVEL and RHYTHM, each a bar and a value from 0 to 127, with the encoder that turns it underneath. A knob lights for a moment as you turn it. |
| WAVE and EG | The oscillator's wave and the envelope, with the one in use lit. |
| Drums | The BD, SN and HH pads, with the button that plays each. A pad lights as its drum plays. |
| Keys | The 27 keys, black above white. A held key fills up. |
| Bottom line | The octave, the last MIDI message in, and the volume (MASTER). |

---

## 3. Controls

| Control | What it does |
| --- | --- |
| Keys | Play the synth. The last key you pressed sounds, and each new key starts the envelope again, as the monotribe's ribbon does. Let go of it while holding another, and the pitch goes back to the one still held. The volume doesn't follow how hard you play, as on the monotribe. Each key also sends its note over USB and Bluetooth, on channel 1. |
| K1 · CUTOFF | The filter's cutoff, from about 80 Hz to about 18 kHz. |
| K2 · PEAK | The filter's resonance. Near the top, the filter whistles on its own at the cutoff while a note plays. |
| K3 · NOISE | White noise beside the oscillator, into the filter. |
| K4 · LEVEL | The synth's volume. |
| SELECT · RHYTHM | The drums' volume. |
| PRESET · WAVE | Saw, triangle or square. |
| ALGORITHM · EG | The envelope. DECAY jumps to full when a note starts and dies away over about 1.3 seconds, even while you hold the key. GATE stays full while you hold it. ATTACK rises over about 1.3 seconds while you hold it. All three close quickly when you let go. |
| EDIT · GLO · HOME | Play the bass drum, snare and hi-hat, and send them out on channel 10. They stand in for the sequencer until it arrives. |
| OCT− · OCT+ | Set the octave, in six steps from −3 to +2, as the monotribe's six octave positions. |
| MASTER | Sets the volume. |
| SAVE | Saves the knobs, the wave, the envelope, the octave and whether Bluetooth is on or off. SAVE lights while there's something to save. |
| REC | Press it to find and connect a Bluetooth MIDI device. Hold it for a second to switch Bluetooth off or on. For details, see MIDI and audio. |
| FX, SEL, ENV, LFO, ARP, SEQ, PLAY | Show their names, and do nothing else yet. The LFO and the sequencer will use them. |

---

## 4. Saving

Press SAVE to keep the knobs, the wave, the envelope, the octave and whether Bluetooth is on. They come back the
next time the FM-1 starts. SAVE stays lit while there are changes to save. Settings saved by FMtribe 0.1 don't
carry over, since this version keeps more of them.

![After turning CUTOFF, PEAK and RHYTHM and switching the envelope to GATE](img/screen-settings.png "Knobs turned, not saved yet")

---

## 5. MIDI and audio

MIDI comes in over USB, Bluetooth and the TRS jack, on the monotribe's own map:

| Message | What it does |
| --- | --- |
| Notes on channel 1 | Play the synth. Velocity is ignored. |
| Pitch bend on channel 1 | Bends the synth up to 2 semitones either way. |
| Notes 36, 40, 42 on channel 10 | Play the bass drum, snare and hi-hat. |
| CC 80 on channel 1 | The envelope: about 32 for DECAY, 64 for GATE, 96 for ATTACK. |
| CC 7 on channel 1 | LEVEL. |

The monotribe's LFO controllers (CC 1, 16, 81 to 83) arrive with the LFO. The keys send notes, with their velocity
and poly aftertouch, out over USB and Bluetooth on channel 1, and the drum buttons send theirs on channel 10.

Over Bluetooth, FMtribe shows up as "FM-1". On a Mac, open Audio MIDI Setup, choose Window > Show MIDI Studio,
click the Bluetooth button, then click Connect next to FM-1. On an iPad or iPhone, connect from a music app's
Bluetooth MIDI settings.

To switch Bluetooth off, hold REC for a second. The screen says BLUETOOTH OFF, and the FM-1 stops advertising and
drops any connection. Hold REC again to switch it back on. To keep your choice for the next time the FM-1 starts,
press SAVE.

To connect FMtribe to a Bluetooth MIDI device, such as a keyboard or a controller, first put the device in its
pairing or advertising mode. Then press REC, and the screen says BLE SCANNING. Turn ALGORITHM to pick the device
by its name, and press REC again to connect. DEV in the header lights while it's connected, and the bottom line
shows the last message it sent. To disconnect, press REC once more.

The keys sense how hard you play them, with no change to the FM-1. The 20 lowest keys
(F3 to C5) give velocity and pressure. The top seven give velocity only.

Over the USB cable, the FM-1 is also a sound card. Your computer can record what it plays from an input named
"FMtribe FM-1". It can also play through the FM-1: choose "FMtribe FM-1" as the output, and you'll hear it from
the FM-1's speaker and headphones, mixed with the FM-1's own sound, at MASTER's volume.

---

## 6. The screen mirror

The site's [Mirror](../mirror/) page shows the FM-1's screen and panel live on your computer, in Chrome or Edge,
over the USB cable. Press Connect and allow MIDI. Press Control to play the FM-1 from the page: click its buttons
and keys, and scroll over a knob to turn it. Close other programs that use the FM-1's MIDI port first.

---

## 7. The screen reader

FMtribe tells a screen reader what's on its screen, through the
[FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/). When the reader starts listening, it hears the
name and the settings, and then each change as it happens. The emulator does the same, with your own screen
reader or with its built-in voice.

---

## 8. Troubleshooting

If you hear no sound, turn MASTER and LEVEL up, and CUTOFF too: with CUTOFF low and PEAK low, the filter lets
little through. With the DECAY envelope, a note dies away after about a second even while you hold the key; choose
GATE with ALGORITHM for a note that lasts. For the drums, turn RHYTHM up.

If the installer doesn't find the FM-1, use Chrome or Edge, and allow MIDI when the browser asks. Close any other
programs that use the FM-1's MIDI port.

If the FM-1 doesn't start after an install, it starts in SAFE MODE after two failed starts. There's no sound, but
USB works, so the web installer can reach it. Install again, or put the stock firmware back from the same page.
Press PLAY to try starting the firmware again. If even safe mode fails twice, the FM-1 drops into the chip's own
update mode.

To put the FM-1 in update mode, hold REC while you switch it on. If you installed m-bootloader with the web
installer, the installer can reach the FM-1 from there, and holding PLAY instead opens m-bootloader's recovery
menu. Without m-bootloader, REC starts the chip's own update mode, which only the rescue can reach. For details,
see felucca-bsp's `tools/fm1_rescue.sh`.
