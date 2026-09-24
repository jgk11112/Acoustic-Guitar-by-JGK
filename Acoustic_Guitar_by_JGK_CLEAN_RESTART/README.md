# Acoustic Guitar by JGK

Clean-restart v0.4 Windows VST3 prototype.

This version prioritises getting a reliable, playable build into FL Studio. It uses embedded CC0 recordings of a 2017 Martin HD28 as the core tone.

## Controls
- Capo: 0-12 semitones
- Strum: spreads MIDI chord notes into a down-strum
- Humanize: small timing and level variation
- Tone: brightness
- Room: reverb amount
- Palm Mute: shortens and darkens the strings
- Output: final level

Short MIDI chords choke quickly after release. Longer chords ring naturally.

## Build
GitHub Actions builds the Windows x64 VST3 automatically after a push. The artifact is named `Acoustic-Guitar-by-JGK-VST3`.

Install the resulting `Acoustic Guitar by JGK.vst3` bundle to:

`C:\Program Files\Common Files\VST3\`

Then open FL Studio > Options > Manage plugins > Find installed plugins.
