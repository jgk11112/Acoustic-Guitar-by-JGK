# Acoustic Guitar by JGK

Prototype Windows VST3 / Standalone acoustic-guitar instrument for FL Studio.

## Core behaviour
- Play or draw a **full MIDI chord** in FL Studio.
- The plugin detects the chord and converts it into a **six-string guitar voicing**.
- Each string is played a few milliseconds apart, creating a real **down-strum** instead of a keyboard-style chord hit.
- **Short MIDI chord:** releasing the notes quickly chokes/damps the strings.
- **Long MIDI chord:** the chord rings naturally until the MIDI notes are released.
- **Capo 0-12:** a neck-style horizontal capo control moves the sounding guitar position just like putting a capo on the neck.
- Controls: Capo, Strum, Humanize, Tone, Room, **Palm Mute**, Output.
- **Palm Mute 0-100%:** progressively shortens and darkens each individual string instead of simply lowering the volume. It can be automated from FL Studio.

## Sound
Version 0.2 uses a more humanised plucked-string physical model (Karplus-Strong style) tuned toward a tight, bright, intimate small-bodied acoustic character. Each string has subtly different level/stereo position, decay variation and pick excitation, with a continuous palm-mute articulation. It does **not** copy or contain samples from any commercial guitar or artist.

**Realism target:** the final production version should be sample-assisted. A synthetic model alone cannot perfectly reproduce the tiny pick, fret, body and string details of a real recorded acoustic. The architecture is intended to be paired with legally recorded JGK multi-samples for the finished instrument.

For the production-quality version, replace/augment the physical model with legally recorded multi-samples of:
- six individual strings
- multiple velocities
- dedicated open, palm-muted and choke/release articulations
- 3-5 round robins per velocity/articulation to stop repeated chords sounding identical
- fret/finger noises
- alternate down/up pick attacks

## Build on Windows
Requirements:
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.22+
- Git

Easiest route: double-click **BUILD_VST3_WINDOWS.bat**. It checks the tools, downloads JUCE through CMake/Git, and builds the Release VST3.

Manual route:

```powershell
cd "PATH\\TO\\Acoustic_Guitar_by_JGK"
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target AcousticGuitarByJGK_VST3
```

The VST3 will normally be produced under:

`build\\AcousticGuitarByJGK_artefacts\\Release\\VST3\\Acoustic Guitar by JGK.vst3`

Copy the `.vst3` bundle to:

`C:\\Program Files\\Common Files\\VST3\\`

Then in FL Studio: **Options > Manage plugins > Find installed plugins**.

## Suggested piano-roll workflow
1. Draw a chord such as C-E-G at the same start point.
2. Make it very short for a tight acoustic chop.
3. Make it one or more beats long to let the strings ring.
4. Turn Capo to 2 to hear the same guitar shape two semitones higher.
5. Keep Strum around 20-35 ms for singer-songwriter rhythm parts.

## Status
This is a functional source prototype with the chord/strum/capo/palm-mute architecture in place. It is deliberately honest about the current sound engine: the included physical model is useful for testing the instrument, but the **final realistic JGK sound should use real, legally recorded acoustic-guitar multisamples** layered into this engine.
