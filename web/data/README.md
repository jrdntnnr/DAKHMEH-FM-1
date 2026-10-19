# Historical tuning snapshot

Retrieved 2026-10-18 from the public Digital Archive of Arabic Maqam API:

- `tuning-systems.json`: https://diarmaqar.netlify.app/api/tuning-systems
- `ronzevalle_1904-yegah.json`: https://diarmaqar.netlify.app/api/tuning-systems/ronzevalle_1904/yegah/maqamat?includeMaqamDegrees=true&includeTranspositions=true&includeDegreeDetails=true

The catalog contains 28 systems. The bundled Ronzevalle (1904) / yegāh
response contains 80 maqams, their available transpositions and separate
ascending/descending degree data. Source versions remain in the JSON.
Other system/starting-note combinations are fetched on demand, not bundled.

Tanghim (https://github.com/Music-Intelligence-Lab/tanghim and
https://tanghim.app/ar/) provided the integration reference. SARDAB uses the
public archive responses; it does not bundle or claim to implement the
Tanghim plugin. Archive data and scholarly attributions remain those of
the named archive contributors and tuning sources.

`maqam_archive.js` keeps paired `midiNotePlusCentsDeviation` and
`centsDeviation` from each contextual degree. It rejects conflicting
assignments to a pitch class and non-octave repetition, and quantizes accepted
offsets to the device's 0.1-cent resolution. Source identity, chosen direction
and degree metadata survive exported tuning JSON. Only the numeric table,
root and scale membership are transferred to the firmware. This does not
encode the maqam's full melodic behavior or an automatic ascending/descending
intonation switch.
