# BOOM FM – a mini DAW on the VIBE

![BOOM FM](icon.png)

**BOOM FM by Sven Trogus (zvenson)**

BOOM FM turns the VIBE into a small studio: eight tracks, each its own instrument, a pattern sequencer with a drum grid
and a piano roll, a mixer with sends and master effects, audio tracks you record into – and it plays nicely with your DAW.
It began as "trap beats on the VIBE" and grew from there.

**Try it in the browser:** https://play.thefirmwarehub.com/?fw=boom

BOOM FM is a native firmware for [VIBE FM](https://thefirmwarehub.com/platform/): the VIBE FM Mk1, the plugin (VST3 / CLAP /
AU) and the browser run the same `.wasm`. Its controls, sequencer, mixer, effects and audio tracks are OberVave 3.9's.

## Eight tracks, one kind of sound each

| Kind | |
|---|---|
| **DRUMS** | eight synth drums (KICK SNARE CLAP CH OH RIM SNAP PERC, the closed hat chokes the open one) – or an **acoustic kit** and a **hip hop kit** from samples |
| **BASS** | a playable 808: mono, punch, click, sub, three drives, DECAY or GATE, SLIDE legato or always |
| **OBERVAVE** | the analog poly synth with its 64 sounds |
| **DX** | a DX7 voice (6 operators, 32 algorithms) under four macros and OP pages (each operator's ratio, level, envelope); loads `.syx` banks from the card. Its first 16 sounds are the former MEL kind's (4-op FM), rewritten as DX7 voices |
| **WAVETABLE** | two oscillators and a sub reading one of 8 tables (ANALOG, PULSE, VOCAL, ORGAN, SYNC, FOLD, BELL, COMB) of 8 frames each – POSITION moves through them, with its own envelope and the LFO; 8 voices |
| **STRINGS** | a string ensemble: 16' / 8' / 4' registers through a three-line chorus |
| **PIANO** | a sampled grand – **only in private builds**: its recordings are not part of this repository (they come from a digital piano whose sound belongs to its maker). Without them TYPE does not offer PIANO and a saved PIANO track stays silent |
| **E-PIANO** | an electric piano: MODEL **TINE** or **REED** (struck tines or reeds, a pickup that barks when played hard, preamp, mono or stereo tremolo) – or **CLAV**: a struck, damped string with two pickups (NECK, BRIDGE, BOTH, OUT of phase), modelled, with the felt's MUTE, the key's CLICK and an envelope filter (WAH, RESO, SPEED) |
| **SAMPLER** | record or load a sample, play it in tune or chop it into slices (even or on the hits), four quality steps from clean to lo-fi |
| **MIDI** | silent – the track's notes, program and four CCs go out to an external synth |
| **AUDIO** | takes per pattern from IN 1 / IN 2 – or up to 16 inputs of an interface, armed and recorded at once (multitrack) |

## Sequencer, mixer, effects

- 64 patterns, 8 tracks, up to 256 steps a track, its own length, meter, swing and quantise; tracks start and stop on the bar.
- **Drum grid** – a row for each drum, velocity and rolls per hit. **Piano roll** – 12 pitch rows, FOLD shows only the notes in use.
- **STEPS**: a strip shows the whole track above the bar being edited; DATA moves the cursor, a push sets or clears the step.
- **TRACK**: every track as a time line – notes where they play, how high and how long, the beats and bars behind them;
  an audio track's wave on the same grid.
- **TIME**: a track without bars – record notes or audio freely, REC closes the loop, START / END snap onto the beats.
- **Sustain pedal** (the VIBE's PEDAL jack or MIDI CC 64): holds the notes of the track it plays – REC records it as its own
  events beside the notes, where it went down and up, never quantised; a take's first pedal replaces the track's old one;
  a MIDI track sends it on as CC 64. Clearing a track clears its pedal.
- Mixer EQ per track (LOW / MID / HIGH). SAMPLER: TRIM cuts away what lies outside START–END, NORMALIZE, a recording threshold.
- Mixer rows LEVEL · FX · PAN · DELAY · REVERB · CHORUS · DRIVE · MASTER. Delay ANALOG / DIGITAL / TAPE, reverb HALL / ROOM / PLATE,
  drive OVERDRIVE / DIST / FUZZ / CRUSH or a **tube amp** as an insert on any track.
- Master FX on the sum: FILTER (low-pass / high-pass), CRUSH, DUB (a self-oscillating tape echo on one knob), REPEAT.
- Play without a keyboard: SHIFT + K1–K6 play the scale (or its chords); the pads play notes and drums.

## In the DAW

In the VIBE FM plugin each track gets its own MIDI channel (1–8) and seven tracks their own stereo outputs. Every sound,
mixer and master parameter can be automated, MIDI channel 16 switches patterns, a track can play the DAW's notes instead of
its steps, and F4 SAVE → MIDI exports a pattern as a MIDI file. Projects are versioned: a newer BOOM FM opens an older one.

**Sounds:** every kind has **128 program slots** (encoder B steps through them and stops at 1 and 128): its factory sounds
first – DRUMS 32, BASS 24, OBERVAVE 72, DX 48, WAVETABLE 24, STRINGS 20, E-PIANO 25 (with the CLAVs), SAMPLER 8,
MIDI 16 – then INIT slots ("WT INIT 25"). F5 stores the track's sound in any slot of its kind (DATA picks it); while "SAVE
HERE?" shows, ENTER gives that slot its factory sound back. A **MIDI program change 0–127 selects slot 1–128 of the
track's kind**. Older projects and stored sounds are carried over: each stored sound into its kind's bank at the place it had.

## Controls

`cheatsheet.html` lists every key and page. HOME on the device shows what every control does right now.

## Build

Built with the [VIBE FM SDK](https://github.com/Vibe-FM-SDK/vibe-fm-sdk). Put this repository at `firmware/boom/` in the SDK, then:

```
tools/build.sh boom                      # build/boom.wasm (+ boom.vibe.aot for the device)
node tools/vibe-check.mjs build/boom.wasm
tools/deploy-device.sh boom              # onto a VIBE
```

Files: `main.c` events and the device glue · `bm_engine.h` the voices of every kind · `bm_params.h` / `bm_patches.h` the
parameters and factory sounds · `bm_sampler.h` the sampler's pages · `bm_save.h` the versioned file format ·
`o3_*.h` / `obv_*.h` OberVave 3's frame: panel, sequencer, mixer, effects, audio tracks, files · `dx7_core.h` the DX7 engine.

## License

GPL-3.0 with an additional attribution term (section 7(b)) – see `COPYRIGHT` and `LICENSE`.
Copyright 2026 Sven Trogus (zvenson). If you use BOOM FM or any part of it, keep the credit
**"BOOM FM by Sven Trogus (zvenson)"** in the sources and name it in your README or documentation.

Third-party parts (see `NOTICE`): the DX7 engine is a port of Dexed's msfa (Copyright 2012 Google Inc., 2016–2025
Pascal Gauthier, Apache-2.0, `LICENSE-APACHE-2.0`); the acoustic and hip hop kits are made from Virtuosity Drums by
Versilian Studios (CC0-1.0); the recorded PIANO is not included (see `NOTICE`). The name BOOM FM and the logo are not part of the license; VIBE FM is a trademark of Sven Trogus.
