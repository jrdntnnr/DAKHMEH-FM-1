# Desert synthesis models in the Grimm port

The santur, mijwiz and arghul are compact adaptations of instrument designs
published by Quilcom / Spogg in SIM-SANTUR v1.00 and SIM-MIJWIZ v1.00:
https://flowstoners.com/quilcom. Reference package URLs and hashes are in
[the provenance manifest](../assets/desert/synth-sources.json).

The source packages were inspected during the previous SARDAB implementation.
Their plugin-info files permit reuse of parts, without supplying a standard
open-source license. No Quilcom binary, manual, waveform or preset table is
bundled. These adaptations use the existing GPL-3.0 firmware and its credited
DaisySP/Rings components; they are not imported desktop plugins.

- **SANTUR / DUSK SANTUR:** PHYS model 4, three detuned string resonators and a
  hammer burst. Structure sets string spread, damping controls decay, and the
  fourth control sets hammer position. The dark preset is our own variation.
  The three-voice PHYS cap still applies; four-note chords can steal voices.
- **MIJWIZ:** VA paired band-limited pulses with unison beating and the existing
  filter. The published design's custom reed waves and comb body are omitted.
- **ARGHUL:** the paired-pulse model with a quieter second pipe on the track's
  tuned tonic (C3 plus root), independent of the played melody note. It follows
  live track tuning and shares the voice envelope. Polyphonic chords layer a
  drone per voice; a separate sequencer track permits independent articulation.
- **DUNE NEY:** an additive WHEEL registration, with rotary disabled. It is a
  compact flute-like color, not a breath simulation.
- **RABABA MODEL:** sustained physical string approximation. No rababa
  recording was used. See the [sample notes](../assets/desert/README.md) for
  the uninstalled rebab recording candidate and its provenance limitations.

All twelve DESERT presets are exercised by `tests/run_desert.sh`, including
sample playback. These are stylized dungeon-synth instruments; acoustic
realism and hardware CPU/listening checks remain separate from software tests.
