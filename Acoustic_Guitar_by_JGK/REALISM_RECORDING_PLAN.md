# Acoustic Guitar by JGK — Realism Recording Plan

The plugin engine handles chords, capo, timing, strumming, note-length chops and palm mute. To make the final instrument convincingly real, feed it legally recorded multisamples rather than relying only on synthesis.

## Record these articulations
- Open picked/strummed notes for all 6 strings, every useful fret.
- 4 velocity layers: soft, medium, firm, hard.
- 4 round robins per layer so repeated chords do not repeat the exact same attack.
- Palm-muted versions at light, medium and heavy pressure.
- Short choke/release samples.
- Finger lift, fret movement and pick/fingernail noises as separate low-level layers.

## Recording character
Aim for a close, intimate small-body singer-songwriter acoustic tone: controlled low end, clear midrange and natural pick detail. Do not copy or redistribute samples from a commercial instrument library.

## Suggested capture
- 24-bit / 48 kHz WAV.
- Close condenser or clean small-diaphragm microphone around the 12th fret, roughly 20–35 cm away.
- Optional second body/bridge microphone for blend, phase checked.
- Record dry with no reverb, compression or limiting.
- Tune before every batch and keep the same pick, strings, seat and mic position.

## Naming idea
`S<1-6>_F<00-20>_V<1-4>_RR<1-4>_OPEN.wav`
`S<1-6>_F<00-20>_V<1-4>_RR<1-4>_PM.wav`

The engine can then select by string/fret/velocity/articulation while preserving the JGK chord and strumming logic.
