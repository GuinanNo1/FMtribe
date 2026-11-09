# FMtribe

FMtribe will recreate the sound and controls of a classic monophonic analogue groove box on the M-VAVE FM-1. This
build has the synth voice, the LFO, the drums and the step sequencer: one oscillator (saw, triangle or square) and
noise into a two-pole MS-style low-pass filter that self-oscillates, a VCA with three fixed envelopes, an LFO on
the pitch, the filter or both, a bass drum, snare and hi-hat, and an 8-step sequencer for all four parts. The
ribbon modes, Flux recording and drum rolls come next. The sound is fitted to recordings of real units, so treat
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

![The SEQ page: TEMPO 120, PART SYNTH, GATE 64, SWING 50 %, and the pattern, a row a part](img/screen-playing.png "The SEQ page")

| Part | What it shows |
| --- | --- |
| Header | The name, and BLE, which lights while a computer or tablet is connected over Bluetooth. While you play a key, it shows the note. After a change, a message shows here for a moment, such as CUTOFF 100. At the top right, which page of its button you're on, such as 1/2. |
| Tabs | The pages' buttons: VCF (SEL), EG (ENV), LFO, DRUM (ARP), SEQ and GLO. The one you're on is lit, and so is its button. |
| K1 to K4 | What the four knobs do on this page. A knob shows a bar and a value from 0 to 127; a switch shows its position. Each lights for a moment as you turn it. |
| Drums | The BD, SN and HH pads, with the black keys that play them on the DRUMS page. A pad lights as its drum plays. On the SEQ pages, the pattern instead. |
| Keys | The 27 keys, black above white. A held key fills up. |
| Bottom line | The octave, PLAY or STOP and the tempo (REC when armed), the last MIDI message in, and the volume (MASTER). |

---

## 3. Controls

The four knobs K1 to K4 edit the page on the screen. A page's button shows it; press the button again for its next
page, if it has more than one. A switch moves one position for each click of the knob.

| Page (button) | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| VCO VCF (SEL) | CUTOFF | PEAK | WAVE | NOISE |
| EG VCA (ENV) | EG | LEVEL | | |
| LFO 1/2 (LFO) | RATE | INT | TARGET | MODE |
| LFO 2/2 (LFO) | WAVE | | | |
| DRUMS (ARP) | RHYTHM | BD DECAY | SN DECAY | HH DECAY |
| SEQ 1/3 (SEQ) | TEMPO | PART | GATE | SWING |
| SEQ STEPS 2/3 (SEQ) | STEPS | SYNC | CLOCK OUT | PART |
| SEQ EDIT 3/3 (SEQ) | STEP | NOTE | GATE | ON |
| GLOBAL 1/3 (GLO) | SYNTH CH | DRUM CH | BLE MIDI | |
| GLOBAL 2/3 (GLO) | KEY LIGHT | GLOW | | |
| DEVICES 3/3 (GLO) | DEVICE | | | |

### The sequencer

FMtribe plays one pattern of 8 steps, a 16th note each, for four parts: SYNTH, BD, SN and HH, as the monotribe
does. Each step has a second slot half-way through it, for the drums' in-between hits and, with STEPS on 16, the
synth's in-between notes. Each part has its own active steps, so a part with fewer steps cycles against the others.

PLAY starts the pattern from step 1 and stops it. PLAY lights green while it plays, red while it records.

On the SEQ pages the keys change job:

| Page | White keys | Black keys |
| --- | --- | --- |
| SEQ 1/3 | The part's 16 slots, low to high: step 1, its half, step 2, its half... A key switches its slot on or off. A synth step switched on takes the last note you played on the keys. | F♯3, G♯3 and A♯3 pick BD, SN and HH (and play them), C♯4 picks SYNTH. |
| SEQ STEPS 2/3 | The lowest eight: the part's active steps, on or off. One always stays on. | F♯3 clears everything, G♯3 the synth, A♯3 the drums; C♯4 turns every part's steps back on. |
| SEQ EDIT 3/3 | Play a note to put it in the step shown (K1), which then moves on to the next step. | Play notes too. |

With KEY LIGHT on GUIDE (GLOBAL 2/3), the keys show the pattern: on SEQ 1/3 a slot that plays is lit, an empty
slot in an active step glows, and the step playing shows half-lit; on SEQ STEPS the active steps are lit. The
screen shows the whole pattern, a row a part, with the playhead in white.

To record, press REC (it lights), then PLAY. While REC is on, the keys play and record the synth on any page,
landing on the nearest step (or half-step in 16-step mode), and how long you hold a note sets its gate time; on
the DRUMS page the pads record the drums. Recording adds to the pattern; to start again, clear a part on SEQ
STEPS. Press REC again to stop recording.

| Setting | What it does |
| --- | --- |
| TEMPO | 10 to 600 BPM. |
| PART | The part the keys and the active steps edit. |
| GATE | The gate time a new synth step gets, from 1.4 % of the step to 100 % (TIE: the note runs into the next). |
| SWING | Plays every second step later, from straight (50 %) to a triplet feel (67 %). |
| STEPS | 8, or 16: the synth's half-steps play too. |
| SYNC | INT runs at TEMPO; MIDI follows MIDI clock, start and stop from a computer or another instrument. |
| CLOCK OUT | Sends MIDI clock, start and stop, so other gear follows FMtribe. |
| STEP · NOTE · GATE · ON | SEQ EDIT: pick a step (or half-step) of the synth, and set its note, gate time and whether it plays. |

The pattern is kept with the settings when you press SAVE.


On the DRUMS page, the black keys in the two F♯–G♯–A♯ groups play the bass drum, snare and hi-hat, and glow to
show it (KEY LIGHT on GUIDE). The white keys still play the synth, so you can play both. The other black keys do
nothing there.

| Setting | What it does |
| --- | --- |
| CUTOFF | The filter's cutoff, from about 80 Hz to about 18 kHz. |
| PEAK | The filter's resonance. Near the top, the filter whistles on its own at the cutoff while a note plays. |
| WAVE | The oscillator: saw, triangle or square. |
| NOISE | White noise beside the oscillator, into the filter. |
| EG | The envelope. DECAY jumps to full when a note starts and dies away over about 1.3 seconds, even while you hold the key. GATE stays full while you hold it. ATTACK rises over about 1.3 seconds while you hold it. All three close quickly when you let go. |
| LEVEL | The synth's volume. |
| RHYTHM | The drums' volume. |
| BD DECAY · SN DECAY · HH DECAY | Each drum's decay, as the popular hardware decay mods give it. 32 is the stock decay; lower is shorter, higher is up to about 15 times as long. |
| RATE | The LFO's speed: about 1 Hz to 5 kHz in FAST, 0.05 Hz to 18 Hz in SLOW and 1 SHOT. |
| INT | How much the LFO moves the pitch or the filter, from none at 0. At the top, the pitch swings an octave either way and the cutoff three octaves. |
| TARGET | What the LFO moves: VCO (the pitch), VCO+VCF (both) or VCF (the cutoff). |
| MODE | FAST and SLOW run all the time. FAST and 1 SHOT start again with each new note. 1 SHOT plays half a cycle when a note starts and then stops, like a second envelope. |
| WAVE (LFO) | Saw (falling), triangle, square, or S&H, a new random level each cycle. |
| SYNTH CH | The MIDI channel the synth plays on and the keys send on, 1 to 16 (1 at first). |
| DRUM CH | The MIDI channel the drums play on and send on, 1 to 16 (10 at first). |
| BLE MIDI | Bluetooth MIDI on or off. |
| DEVICE | On DEVICES, the Bluetooth MIDI device to connect to: see MIDI and audio. |
| KEY LIGHT | The keys' lights: OFF; HELD, the keys you hold; or GUIDE, those and, dimly, the keys that do something special on the page you're on, such as the drum pads. |
| GLOW | How brightly the unlit buttons and the guide keys glow: OFF, LOW or HIGH. |

| Button | What it does |
| --- | --- |
| Keys | Play the synth. The last key you pressed sounds, and each new key starts the envelope again, as the monotribe's ribbon does. Let go of it while holding another, and the pitch goes back to the one still held. The volume doesn't follow how hard you play, as on the monotribe. Each key also sends its note on the synth's channel. |
| OCT− · OCT+ | Set the octave, in six steps from −3 to +2, as the monotribe's six octave positions. |
| MASTER | Sets the volume. |
| SAVE | Saves every setting, the octave and the pattern. SAVE lights while there's something to save. On the DEVICES page, SAVE connects to a Bluetooth MIDI device instead. |
| PLAY · REC | Start and stop the sequencer; record into it. See The sequencer. |
| EDIT · HOME · FX | Show their names, and do nothing else yet. |

---

## 4. Saving

Press SAVE to keep every setting, the octave and whether Bluetooth is on. They come back the
next time the FM-1 starts. SAVE stays lit while there are changes to save. Settings saved by an earlier FMtribe
don't carry over, since this version keeps more of them.

![GLOBAL 2/2: KEY LIGHT GUIDE, GLOW LOW](img/screen-settings.png "The second GLOBAL page")

---

## 5. MIDI and audio

MIDI comes in over USB, Bluetooth and the TRS jack, on the monotribe's own map. The channels are set on the GLOBAL
page; at first the synth is on 1 and the drums on 10.

| Message | What it does |
| --- | --- |
| Notes on the synth's channel | Play the synth. Velocity is ignored. |
| Pitch bend on the synth's channel | Bends the synth up to 2 semitones either way. |
| Notes 36, 40, 42 on the drums' channel | Play the bass drum, snare and hi-hat. |
| CC 1 · CC 16 | LFO INT · LFO RATE. |
| CC 80 | EG: about 32 DECAY, 64 GATE, 96 ATTACK. |
| CC 81 | LFO TARGET: 32 VCO, 64 VCO+VCF, 96 VCF. |
| CC 82 | LFO MODE: 32 FAST, 64 SLOW, 96 1 SHOT. |
| CC 83 | LFO WAVE: 32 saw, 64 triangle, 96 square, 127 S&H. |
| CC 7 | LEVEL. |
| Clock, start, stop | With SYNC on MIDI (SEQ STEPS), the sequencer follows them. |

The CCs are read on the synth's channel. The sequencer sends its notes out too, on the same channels, and MIDI
clock, start and stop with CLOCK OUT on. The keys send notes, with their velocity and poly aftertouch, out over USB
and Bluetooth on the synth's channel, and the drum pads send theirs on the drums' channel.

Over Bluetooth, FMtribe shows up as "FM-1". On a Mac, open Audio MIDI Setup, choose Window > Show MIDI Studio,
click the Bluetooth button, then click Connect next to FM-1. On an iPad or iPhone, connect from a music app's
Bluetooth MIDI settings.

To switch Bluetooth off, set BLE MIDI to OFF on the GLOBAL page (GLO). The FM-1 stops advertising and drops any
connection. To keep your choice for the next time the FM-1 starts, press SAVE.

To connect FMtribe to a Bluetooth MIDI device, such as a keyboard or a controller, first put the device in its
pairing or advertising mode. Then open the DEVICES page (GLO, pressed until it shows 3/3): the FM-1 looks for
devices while the page is up. Turn K1 to pick the device by its name, and press SAVE to connect. DEV in the header
lights while it's connected, and the bottom line shows the last message it sent. To disconnect, press SAVE on that
page again.

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
little through. With the DECAY envelope, a note dies away after about a second even while you hold the key; on the
EG VCA page (ENV), set EG to GATE for a note that lasts. For the drums, turn RHYTHM up on the DRUMS page (ARP). If the
pitch wobbles, turn INT down on the LFO page.

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
