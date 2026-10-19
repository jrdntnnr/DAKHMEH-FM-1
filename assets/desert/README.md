# SARDAB / Grimm desert instruments

The four **factory sample slots** are OUD (1), QANUN (2), NEY AIR (3), TAR (4).
They are embedded in the firmware. Empty slots are populated at boot; occupied
slots are retained. To replace existing samples, use the editor's **Samples →
Restore factory samples** after exporting anything you want to retain. That
operation replaces all four sample slots. No separate transfer is needed for
an empty device/emulator. The DESERT preset category selects these slots.

Synthesized presets: DESERT OUD, BRASS QANUN, SANTUR, DUSK SANTUR, DUNE NEY,
MIJWIZ, ARGHUL and RABABA MODEL. These are compact interpretations, not sampled
replicas. The rababa model is a sustained physical string approximation; it is
not derived from the recording linked below. DESERT user sounds can be stored
in user slots 1–3 (or any of the 64 slots) by selecting CATEGORY DESERT before
Store. The four sample slots and the user sound slots are different stores.

## Recordings and credits

Oud recordings by **hammondman**, Turkish kanun by **barisbozkurt**, and ney by
**aliaref**: CC0-1.0. [sources.json](sources.json) records recording pages,
license links and source hashes. The public MP3 previews were isolated, mixed
to mono, resampled to 22050 Hz, DC-corrected, pitch-calibrated, normalized and
faded in the previous SARDAB project. These calibrated WAVs are retained.
Ney Air contains natural breath and pitch movement, not steady tuning tones.

**Persian tar — CC BY:** adapted from the [Berklee Intersectional Soundbox
Archive](https://remix.berklee.edu/bisa-persian-tar/), 2025. Performer **Erfan
Hessampour**, recording engineer **Yangyang Hua**, editor **Shuyi Xiong**.
Sources: Forte C3, C4, Ab4 and Db5; [tar-sources.json](tar-sources.json) contains
individual recording pages and hashes. The publisher declares CC BY without
specifying a version. Preserve these credits and links with redistributed
recordings/banks. Public AAC previews were trimmed, mixed to mono, resampled,
pitch-calibrated, normalized and faded. The Grimm bank additionally shortens
four tails to fit 73,216 bytes. It does not remove a recorded root.

All four banks play as one-shots with nearest-root key zones. Sparse recordings
change character with large transpositions. No sustain loops are asserted.

Rebuild `python3 tools/build_desert_banks.py` (or `tools/gen_samples.py`).
`banks.json` contains target zones and output hashes; `source-banks.json`
contains the previous SARDAB processing/calibration provenance. The new GSMP
version-2 `.hdr`/`.bin` files are for this port, **not** old SARDAB/Felucca slots.
The continuous ADPCM stream shares Grimm's existing seek/decode memory.

## Rababa source found

[Rebab by redafs](https://freesound.org/people/redafs/sounds/348175/),
CC BY 4.0, 49.816 seconds, stereo 44.1 kHz. The uploader calls it rebab/rabab/
rababa and tags it “java”; that does not establish an Arabic Bedouin rababa.
The original WAV requires Freesound login. It has **not** been downloaded,
cut into playable roots, pitch-calibrated or installed. It is a candidate for
a future replacement sample bank, not an authenticated Arabic instrument pack.
