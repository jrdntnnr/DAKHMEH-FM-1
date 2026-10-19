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

The Rababa preset is a synthesized sustained string approximation. A CC BY
rebab recording was located, but its Arabic provenance is unconfirmed and it
has not been downloaded or installed. The source link is in the instrument
notes. These compact models are dungeon-synth interpretations, not acoustic
reconstruction claims.

## Controls

Build with `EMSDK=/path/to/emsdk sh web/build_web.sh`, serve this checkout over
HTTP, and open `/web/dist/editor.html` in Chrome. Choose **Try virtual**, or
connect the FM-1 over USB MIDI. The tuning panel edits the selected track.

On the device/emulator, tap **SEL** through SCALE, CHORD, TUNE 1–3, TUNING and
TASHRIQ 1–3. TUNING selects MAQAM/TASHRIQ, PRESET/CUSTOM and LIVE/GLIDE/HOLD.
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

Track records are 111 bytes; working-project version is 9, slot-stream version
8, editor protocol 3 and object format 3. Slot objects are 2,768 bytes and the
working-project payload is 2,804 bytes. The editor's factory scratch area
starts at 2,816. [Protocol extension](EDITOR_PROTOCOL.md).

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

The final DAKHMEH target build passed on 2026-10-19:

| Region | Used / budget |
| --- | ---: |
| Application | 383,528 / 581,564 bytes |
| Static RAM | 86,264 / 98,304 bytes |
| Pool | 340,808 / 344,064 bytes |

Package: `build/dakhmeh-0.2-desert.fwsc` (610,110 bytes).
SHA-256: `3be4890b4876b4e48b923d622270298e24aa083c6b4090baac3047d7e55fca13`.
The installer parser verified its loader marker, header, file list and file
CRCs without opening MIDI or writing to a device. The target duplicate-symbol
check also passed. Hardware performance and updater/recovery testing remain
pending; the package still carries the development identity `FM-1_966`.

The earlier macOS `unknown userid 501` failure was in the Lima/SSH clients,
not the running VM. The local Docker wrapper now falls back to the existing
SSH ControlMaster using `tools/lima_shell.py`; see [building](BUILDING.md).

## Remaining broader port

Hardware CPU/audio/USB/retuning tests; production backup conversion; velocity
port; further DAKHMEH visual treatment; selected DREAM MAQAM/CANON
features; final package identity, updater/recovery qualification and firmware
release. This repository is the new DAKHMEH fork; the older SARDAB repository is unchanged. BLE and DX7
imports remain excluded.

## DAKHMEH rebrand

Persian دخمه splash, DAKHMEH application/editor labels and package filenames.
Seven current firmware captures are in `docs/screens/dakhmeh/`. The browser
also retains upstream `30aa882` (MIDI input device selection). Native/WASM
and tuning tests passed after rebranding. Compatibility format identifiers
(`grimm-backup`, `grimm-sound`, `sardab-tuning`) remain readable unchanged.
