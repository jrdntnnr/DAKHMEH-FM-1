# DAKHMEH: desert instruments and tuning

2026-10-19. Formerly the SARDAB Grimm port; now DAKHMEH. Based on Grimm
`815ca6be81d973445851238dbaf12b4536d1e6d8`. This checkout is separate from
production SARDAB `main`. It is a development port, not a migration release.

## Included

- 49 factory scale entries plus CUSTOM, independent tonic, keyboard layout,
  tuning and chord generation for each of four tracks.
- **Tashriq:** twelve absolute C–B switches, initially off, each initially
  −50 cents. The browser can change each amount from −100 to +100 cents.
  Changes apply across octaves. This mode does not stack the maqam table or
  temperament on top. All switches off restores 12-TET (at the global tuning
  reference). Use CHR and chords off to retain all twelve independent keys.
- **Maqam tuning:** factory intonations or twelve root-relative custom offsets
  in 0.1-cent units (±200 cents), with additional ±63-cent trims. Tonic, scale
  membership, tuning table and layout are separate. Factory tables also offer
  EQUAL/PYTH/MEAN/JUST temperament adjustments; custom tables replace these.
- LIVE, GLIDE and HOLD policies for a played note's tuning. HOLD captures its
  initial offset; a retrigger captures the new tuning. GLIDE smooths changes
  over successive audio blocks. Separate tonic drones and sympathetic ROOT
  resonances remain live references to the track tuning.
- Actual oscillator, FM and sample pitch changes; each generated chord tone,
  sequencer note and arpeggiator note receives its own tuning. Loading an
  instrument retains the track tuning. Incoming MIDI retains note identity
  and bypasses the physical-key layout.
- Browser controls, JSON tuning preset import/export, Scala import, and a
  DiArMaqAr historical tuning selector using the same public data service as
  Tanghim. See [data provenance](../web/data/README.md).
- DESERT category with eight synthesized presets (oud, qanun, two santurs,
  ney, mijwiz, arghul and RABABA MODEL) plus four sample presets.
- Four embedded multi-zone one-shot sample banks: **OUD, QANUN, NEY AIR, TAR**.
  They share the existing sampler memory. Empty slots populate at boot;
  occupied slots are retained. Editor **Samples → Restore factory samples**
  replaces all four slots after confirmation. No separate transfer is needed
  on an empty device. [Instrument sources and installation](../assets/desert/README.md).

- **Velocity** (from SARDAB): every note carries a velocity from the key,
  the MIDI note or the step, and its level follows it (Q15 gain
  v·(127+v)/(2·127²): 127 is unchanged, 64 is about −8 dB). Recorded steps
  keep theirs, and the STEP page edits it. Ratchets, chords, the arp and
  restarts on a new source keep it. SYSTEM 2 VELOCITY turns on SARDAB's key
  sensing (Charles Vestal's fm1-velocity ADC/weak-drive method,
  `hal/fm1_velocity.h`, `src/key_velocity.h`). It is FIXED by default: every
  key at 127, as before.
- **CANON** (from SARDAB; ARP's second page): a second playhead half the
  pattern behind, a scale interval (3RD/5TH/7TH/OCT, down too) or backwards
  (R modes). It plays each step's note or chord for its LENGTH at 3/4
  velocity, tuned per note like the first playhead.
- **DREAM / DREAM MAQAM** (from SARDAB; GLO's second page): a key composes a
  four-chord ambient piece over the four tracks' sounds. MOOD MAQAM (the
  default) carries the selected track's maqam and full tuning to all four.
  Pads are scale-triad chord steps, so they are tuned. DENSITY, EVOLVE and
  DRIFT work as on SARDAB. It is undoable and refuses to run in song mode.
- **ARP TRILL and DRONE** (from SARDAB): TRILL plays each note then its
  upper scale neighbour (tuned per note); DRONE holds the lowest note while
  the others go UP over it, moving with the lowest note. They are appended
  as modes 6 and 7 (older projects are unchanged).

The Rababa preset is a synthesized sustained string approximation. A CC BY
rebab recording was located, but its Arabic provenance is unconfirmed and it
has not been downloaded or installed. The source link is in the instrument
notes. These compact models are dungeon-synth interpretations, not acoustic
reconstruction claims.

## Controls

Build with `EMSDK=/path/to/emsdk sh web/build_web.sh`, serve this checkout over
HTTP, and open `/web/dist/editor.html` in Chrome. Choose **Try virtual**, or
connect the FM-1 over USB MIDI. The **MAQAM & TUNING** panel sits in the
track editor (the EDITOR view, under the track's knobs) and edits the
selected track; since 0.4.2 it is there from the start, not only once
connected.

On the device/emulator, tap **SEL** through SCALE, CHORD, TUNE 1–3, TUNING and
TASHRIQ 1–3. TUNING selects MAQAM/TASHRIQ (MODE, two positions), PRESET/CUSTOM
(TABLE, two) and LIVE/GLIDE/HOLD (RETUNE, three). A knob of up to nine
choices moves one choice a turn pass, however fast the flick (0.4.2), and
its bar shows which choice: a two-way knob empty or full, a three-way 2, 5
or 8 segments.
The three TASHRIQ pages toggle C–B in groups of four. The browser exposes the
exact custom table and switch amounts; TUNE pages are additional maqam trims.

For Rast: choose ROOT C, SCALE RAST, MAP CHR, MODE MAQAM, TABLE PRESET and
TEMP EQUAL. E and B are each 50 cents flat. For keyboard-style tashriq: choose
MODE TASHRIQ, MAP CHR, and switch E and B on. Setting E's amount to −37 cents
changes every E independently of B. Keyboard markers show enabled switches.

For chords, hold SEL to enable chord mode and use the CHORD page to choose
the voicing (DIA7 includes the tuned third and seventh). Out-of-scale chord
roots snap to a scale member. CHR with chords off does not snap. WHITE/ALL
map successive degrees; SNAP intentionally permits adjacent-key convergence;
SEQ also snaps sequencer playback. WHITE's black keys are silent.

Open **Historical maqam tuning** in the browser for tradition, starting note,
maqam, transposition and ascending/descending selection. Ronzevalle (1904),
yegāh is bundled; other combinations need network access. The import rejects
mappings that cannot fit twelve octave-repeating pitch classes instead of
silently merging incompatible notes. Save a tuning JSON to preserve source
names, archive identity and melodic-degree metadata. Device projects preserve
the numerical mapping, not the archive's full bibliographic identity.

Scala supports octave-repeating scales with up to twelve distinct mapped
notes. It reports approximation error; unsupported/non-octave scales and KBM
are not imported. This is not an implementation of maqam melodic grammar.
External MIDI output still uses ordinary note numbers: MPE/MTS is not included.

## Storage and compatibility

Track records are 111 bytes; working-project version is 10, slot-stream version
8, editor protocol 3 and object format 4. Slot objects are 3,040 bytes and the
working-project payload is 3,080 bytes. Version 10 appends a 68-byte block per
track (64 step velocities, CANON) plus DREAM's four settings. The slots keep
their stream version: their blocks live in the stream's previously zero tail
after the FM patches (12,160 of 12,240 bytes now used). Version 9 projects and
older slot streams load with velocity 127 and CANON OFF. The editor still
accepts format-3 objects, and the settings gain VELOCITY (169 bytes; older
168/167-byte settings load as FIXED). [Protocol extension](EDITOR_PROTOCOL.md).

Older working-project readers supply defaults for missing tuning fields.
Older slot streams/backups and production SARDAB GBM2 projects are **not**
converted. Save complete backups before later device migration; reflashing
alone cannot restore overwritten sample or project sectors. GSMP v1 tape
samples remain supported; new factory banks use GSMP v2 zone headers.

Twelve voices are shared across four tracks, including release tails. PHYS is
limited to three shared voices and SAMPLE to six; other engines use the full
budget. Grimm's overload guard remains active. Voice count and memory fit do
not establish hardware CPU or interrupt-stack margins.

## Validation and build status

- 0.5: `tests/run_host.sh` runs `enc_test` (SARDAB's cases and Clipper's), `clean_test` (48 cases, all clean)
  and `split_test` in four modes (one core and two the same, bit for bit; a job lost falls back). The host run's
  sound is byte for byte 0.4.3's (HALL and the split change nothing that sounded before). The native/WASM
  comparison passes. The device build links with `.c1_text` in RAM and 220 B of pool to spare.

- `sh tests/run_desert.sh`: sanitized firmware integration checks, including
  7,924,800 factory pitch combinations (worst oscillator error 0.2130 cents,
  FM 0.2894 cents), separate tuning modes, custom offsets, retune policies,
  chords, sequences, storage, instrument retention, all twelve desert presets,
  all sixteen sample zones, one-shot endings and same-note retriggers.
- `node web/test_tuning.mjs`: Scala/JSON, protocol and track isolation, all 52
  factory objects, and 10,794 representable historical forms/directions;
  806 incompatible forms explicitly rejected. These are not distinct maqams.
- `sh tests/run_host.sh` then `node web/test_web.mjs`: 705,536 native/WASM audio
  frames matched at the harness's documented master-volume scaling; four
  screen comparisons and editor/sample/backup checks passed.
- Seed-1 sanitized host stress passed (`STRESS OK`, 10,384,640 rendered
  frames). The target-only duplicate-symbol check also passed after repairing
  VM access on 2026-10-19. Logs are local under `build/assessment/`. Browser interaction passed before the
  final responsive layout adjustment; final visual recheck was blocked by
  browser sandbox IPC.
- `tools/gen_samples.py` reproduced all four embedded banks byte for byte.
- 2026-10-19, velocity/CANON/DREAM: `sh tests/run_desert.sh` also runs
  `tests/velocity_test.c` (sanitized). It covers gains, key/MIDI/step/
  ratchet/arp velocity, the sensing filter, ARP TRILL (Bayati, CHR) and
  DRONE (hold, move, gate, mute, mode change, end, every mode played), all 13 CANON modes (scale,
  maqam, chord, reverse, gate, stop), storage v10/v9 plus slot stream,
  flash boot and editor format 3/4, and DREAM in every mood and density
  with 300 EVOLVE changes kept in scale. The native/WASM comparison
  (`web/test_web.mjs`, 28 checks), `web/test_tuning.mjs` and the sanitized
  stress test (seeds 1, 2 and 3) all passed.

The final DAKHMEH target build passed on 2026-10-19:

| Region | Used / budget |
| --- | ---: |
| Application | 383,528 / 581,564 bytes |
| Static RAM | 86,264 / 98,304 bytes |
| Pool | 340,808 / 344,064 bytes |

Package: `build/dakhmeh-0.2-desert.fwsc` (610,110 bytes).
SHA-256: `3be4890b4876b4e48b923d622270298e24aa083c6b4090baac3047d7e55fca13`.

With velocity, CANON and DREAM (2026-10-19, development build
`build/dakhmeh.fwsc`), the ROM-reference, `.ram_text` and hal-only register
checks all passed:

| Region | Used / budget |
| --- | ---: |
| Application | 394,760 / 581,564 bytes |
| Static RAM | 89,928 / 98,304 bytes |
| Pool | 341,356 / 344,064 bytes |
The installer parser verified its loader marker, header, file list and file
CRCs without opening MIDI or writing to a device. The target duplicate-symbol
check also passed. Hardware performance and updater/recovery testing remain
pending; the package still carries the development identity `FM-1_966`.

The earlier macOS `unknown userid 501` failure was in the Lima/SSH clients,
not the running VM. The local Docker wrapper now falls back to the existing
SSH ControlMaster using `tools/lima_shell.py`; see [building](BUILDING.md).

## Remaining broader port

Hardware CPU/audio/USB/retuning tests; **hardware validation of key
velocity sensing** (the scan now owns the ADC: MASTER and battery read
from its cache, as on SARDAB; untested on this build's hardware); production
backup conversion; further visual
treatment; final package identity, updater/recovery qualification and
firmware release. This repository is the new DAKHMEH fork; the older SARDAB repository is unchanged. BLE and DX7
imports remain excluded.

## DAKHMEH rebrand

Persian دخمه splash, DAKHMEH application/editor labels and package filenames.
Thirteen current firmware captures (including HOME, MIXER, COLOUR, STEP velocity, CANON and DREAM) are in `docs/screens/dakhmeh/`. The browser
also retains upstream `30aa882` (MIDI input device selection). Native/WASM
and tuning tests passed after rebranding. Compatibility format identifiers
(`grimm-backup`, `grimm-sound`, `sardab-tuning`) remain readable unchanged.

### The desert look (0.4), the knobs (0.4.2), the demo's MIDI channel (0.4.3)

- **COLOUR DESERT** (SYSTEM): SARDAB's palette, obsidian to bone with copper for the active thing, as a fifth
  theme beside GR!MM's four accents; a fresh device starts on it, a saved COLOUR is kept. The editor accepts
  settings with COLOUR 0..4 (`editor.c`).
- **HOME**: a night over the dunes. The stars sparkle with the output; a lantern a track burns with its level and
  peak (muted or LEVEL 0: dark glass). GR!MM's eye and candles are gone. Drawn as before: a frame every
  `UI_ANIM_MS`, only the cells that changed (`ui_home_frame`).
- **Tuning rail** (SCALE, CHORD, TUNE, TUNING, TASHRIQ): under the keys, a datum with every in-scale key's cents off
  12-TET as a tick (up sharp, down flat, 3 px a hundred cents, a dot in tune) and the root as a diamond.
- **MIXER meters**: a meter beside each track's column, its peak before LEVEL on a log scale (12 px a doubling), a
  frame every `UI_ANIM_MS` (`ui_meter_frame`).
- **Scrolling names**: a slab value whose full text (`knob_text`) is longer than its six characters scrolls it,
  held at each end (`ui_window`, `UI_SCROLL_MS`); so does the header's left text when cut.
- **Key lights**: the maqam pages glow the keys in the scale and light the root's and TASHRIQ's switched keys;
  CANON glows the key of the step its playhead is at; DREAM playing breathes all keys (off, glow, lit, glow, 2.4 s);
  at boot a light runs the keys right to left once, two glowing behind it.
- **Knobs** (0.4.2): the detent decoder is SARDAB's (#126: every frame's state counts, a two-state jump from
  the detent goes the way of a recent click), tested by `tests/enc_test.c`; GR!MM's 2-sample filter lost whole
  clicks on fast turns. A knob of up to nine choices steps one choice a pass.
- **Demo's MIDI channel** (0.4.3, GR!MM 43e710d): a CH selector beside MIDI IN under the browser panel passes
  one channel of 1..16 or ALL, kept in the browser; the firmware is unchanged.

### Two cores, three more reverbs, no clicks (0.5)

- **The second core** (`hal/fm1_cpu1.h`, X0X's by way of SLOOP and Clipper; `cpu1.c`): started at power-on before
  the audio and the timers, proved with a self-test job, held again if it fails to report or answers wrong. Each
  control step `voices_render` does the control for every voice (the envelopes, the tuning), weighs the sounding
  tracks by their engines (`ENG_COST`) and hands core 1 the heavier half of the tracks; it renders the rest and
  waits. A track renders on one core in a step, so the engines' per-track state is one core's; a voice's state is
  its own; the FM6 modulator buses moved onto the stack, VOICE's breath noise became one stream a track. Integer
  sums: the output is the same bit for bit whoever renders what (`tests/split_test.c`: one core against two under
  the host double, inline, on a thread, on a slow thread, with a job lost). A job lost (8 ms): that step's tracks
  rendered on core 0 once more, then one core until power-off. SYSTEM's header shows `2C` or `1C`. The effects
  (DELAY, SPACE, GRAIN, TAPE) stay on core 0. No menu switch: the fallback is automatic. **Not yet run on an FM-1.**
- **SPACE TYPE** (SPACE 2): HALL as before, bit for bit; PLATE, CAVERN and SHIMMER are Clipper's Dattorro plate
  (`hall.c`), in HALL's memory (a union: the pool is full), so their PRE-DELAY stops at 54 ms and SIZE 127 is a
  long decay, not a freeze. A TYPE change fades the return out, clears, fades in. Saved in the working project's
  song block (a spare byte: older saves read HALL).
- **No clicks** (`tests/clean_test.c`, after Clipper's): a chord held, every knob of the mix and the effects turned,
  the output's second difference watched for a step. Fixed by it: DELAY TIME (the new tap fades in over the old,
  46 ms, instead of a glide that read the tape at 20 x), VOICE's knobs (they glide, ~3 ms), PHYS (the old model
  fades out before the new is struck; STRUCTURE, DAMP and BOW glide). PHYS's MODEL strike and STRUCTURE's retuning
  are transients by design and exempt.
