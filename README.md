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
| **MEL** | 4-operator FM: 8 algorithms, feedback, 8 voices, filter, envelopes, LFO, arp – bells, plucks, keys, pads |
| **OBERVAVE** | the analog poly synth with its 64 sounds |
| **DX** | a DX7 voice (6 operators, 32 algorithms) under four macros; loads `.syx` banks from the card |
| **STRINGS** | a string ensemble: 16' / 8' / 4' registers through a three-line chorus |
| **SAMPLER** | record or load a sample, play it in tune or chop it into slices (even or on the hits), four quality steps from clean to lo-fi |
| **MIDI** | silent – the track's notes, program and four CCs go out to an external synth |
| **AUDIO** | takes per pattern from IN 1 / IN 2 – or up to 16 inputs of an interface, armed and recorded at once (multitrack) |

## Sequencer, mixer, effects

- 64 patterns, 8 tracks, up to 256 steps a track, its own length, meter, swing and quantise; tracks start and stop on the bar.
- **Drum grid** – a row for each drum, velocity and rolls per hit. **Piano roll** – 12 pitch rows, FOLD shows only the notes in use.
- Mixer rows LEVEL · FX · PAN · DELAY · REVERB · CHORUS · DRIVE · MASTER. Delay ANALOG / DIGITAL / TAPE, reverb HALL / ROOM / PLATE,
  drive OVERDRIVE / DIST / FUZZ / CRUSH or a **tube amp** as an insert on any track.
- Master FX on the sum: FILTER (low-pass / high-pass), CRUSH, DUB (a self-oscillating tape echo on one knob), REPEAT.
- Play without a keyboard: SHIFT + K1–K6 play the scale (or its chords); the pads play notes and drums.

## In the DAW

In the VIBE FM plugin each track gets its own MIDI channel (1–8) and seven tracks their own stereo outputs. Every sound,
mixer and master parameter can be automated, MIDI channel 16 switches patterns, a track can play the DAW's notes instead of
its steps, and F4 SAVE → MIDI exports a pattern as a MIDI file. Projects are versioned: a newer BOOM FM opens an older one.

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
Versilian Studios (CC0-1.0). The name BOOM FM and the logo are not part of the license; VIBE FM is a trademark of Sven Trogus.
