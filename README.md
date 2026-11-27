# ReRack Vintage Digital

An iPlug2 VST3 audio-effect plugin that runs bit-exact re-creations of vintage digital rack units:

- **G Major 2** - the C17 `libgmajor2` library's 36 TC Electronic G-Major 2 effects
- **Intellifex LTD** - the C17 `libifx` library's six Rocktron Intellifex LTD configurations

The editor's Unit menu picks the rack unit, the Effect menu one of its effects; each unit keeps
its own effect. Every knob of every effect is an automatable parameter. The editor is drawn by
IGraphics with the NanoVG backend on OpenGL 3.

## Requirements

- Linux with the X11, xcb and OpenGL development headers
- GNU Make 4.3 or newer, a C17/C++17 compiler, Git and pkg-config

## Build

```sh
git clone --recurse-submodules git@github.com:re-rack/rerack-plugin.git
cd rerack-plugin
make            # the VST3 bundle, build/out/ReRackVintageDigital.vst3
make test       # build and run the tests
make install    # copy the bundle into ~/.vst3, replacing an older copy
make clean      # remove build/
```

`make` checks out the submodules the build needs if a clone left them empty.

Variables can be overridden on the command line, for example `make -j16`,
`make VST3_DIR=/usr/local/lib/vst3 install`, `make BUILD=/tmp/vintage-digital`,
`make IPLUG2=../iPlug2`, `make GM2_DIR=../libgmajor2` or `make IFX_DIR=../libifx` to build
against checkouts outside the submodules.

## G Major 2

### Mix and Out Level

Where the device has them, an effect exposes Mix (0 to 100 %) and Out Level (-100
to 0 dB) with the device's factory defaults. Both are passed to the library raw:
the effect does the device's equal-power dry/wet blend and folds Out Level into
its level words, so the plugin adds no gain or smoothing of its own. The EQ, Noise
Gate and Compressor have neither knob, as on the device.

### De-pop

Some knobs click when turned fast, because the device does. The De-pop switch
(on by default) runs the library's de-popped variant of the effects that have one:
the Three-Band EQ, the Resonance Filter, both Pitch Shifters, the Reverse Delay
and the four Reverbs. A variant gives the effect's own output, sample for sample,
once its knobs have settled, and costs about twice the effect while a knob moves.
For the other effects the switch is greyed out. Flipping it restarts the effect,
as switching effects does.

### Sample rates

Effects run directly at host rates up to 48 kHz, including 44.1 and 48 kHz, with
zero latency. Above 48 kHz the plugin divides the rate by the first power of two
(1, 2, 4, ... 64) that the library accepts, runs the effect at that base rate,
and converts down and back up through equiripple linear-phase half-band FIR
stages, flat to 20 kHz and 100 dB down in the stopband. The filters' delay,
74 samples at 96 kHz and 166 at 192 kHz, is reported to the host. If no power
of two produces a valid rate, the plugin passes audio through untouched.

Tempo-synced knobs follow the host BPM; LFO effects are aligned to the song
position when playback starts, after an effect switch, and after a position
jump.

## Intellifex LTD

The six configurations are Reverb; Delay & Ducker; 8 Voice Chorus; Pitch Shift & Delay;
Chorus, Delay & Reverb; and Pitch Shift, Delay & Reverb, each with the device's Hush. Knob names,
ranges, value text and the Page menu's pages are the firmware's (its parameter names, MCU windows,
display routines and FUNCTION SELECT pages), and every knob starts at the configuration's factory
preset.

- **Mono in, stereo out.** The device's single A/D sees its two inputs summed; the plugin feeds
  the effect (L + R) / 2.
- **39 kHz.** The firmware has no rate maths, so the effect runs at the device's own 39 kHz
  through a rational polyphase resampler (Kaiser-windowed, flat to 0.45 of the lower rate,
  100 dB down from its half) and back. The round trip plus the device's 32-frame block is an
  exact whole number of host samples, reported to the host: 182 at 44.1 kHz, 198 at 48 kHz,
  395 at 96 kHz. At 39 kHz nothing is resampled.
- **Analog Direct** (on by default) models the device's analog dry path: with Direct Hush at
  Pre, the input is added at L/R Dir Level, as on the hardware. Off, the effect outputs only
  what the DSP does. **Output Level** models the output pot, 0 to 100 %. Both are modelled
  stages, not bit-exact, and glide over 26 ms when they or the Dir Levels change; with Analog
  Direct off and Output Level at 100 % the output is the DSP's, bit-exact.
- **De-pop** (on by default) runs libifx's de-popped variant of the effect. A turn crossfades
  over 26 ms from the old settings to the new once the click it makes has passed, and from there
  the output is the effect's, sample for sample. A voice's Delay, Level and Pan, the Delay switch
  and the type changes are heard only once their knob stops, after the voice's delay or the
  memory wipe. A click that regenerates through a delay line or enters the reverb still comes
  back. Flipping the switch restarts the effect.
- The plugin's latency follows the unit, so switching units makes the host re-read it.

## Layout

- `src/VintageDigital.*` - parameters, audio processing, VST3 parameter titles and text input
- `src/VintageDigitalEditor.cpp` - fixed-size IGraphics editor: Unit, Effect and Page menus
- `src/config.h` - iPlug2 plugin configuration
- `src/EffectCatalog.*` - shared knob and effect types, the units, the G Major 2 effect and knob
  tables, raw-to-parameter mapping, knob value text
- `src/IfxCatalog.*` - the Intellifex LTD effect, knob and page tables
- `src/EffectEngine.h` - templated engine wrapper around each library module
- `src/Resampler.*` - G Major 2 host-rate to base-rate conversion, Remez half-band filter design
- `src/RationalResampler.*` - Intellifex host-rate to 39 kHz conversion, rational polyphase FIR
- `tests/resampler_test.cpp` - half-band resampler latency, passband, aliasing and block-size checks
- `tests/rational_resampler_test.cpp` - rational resampler latency, passband, aliasing, imaging and
  block-size checks
- `tests/depop_test.cpp` - the De-pop engines against their effects
- `tests/knob_format_test.cpp` - knob value text and its parsing
- `tests/ifx_catalog_test.cpp`, `tests/ifx_fixture_knobs.c` - the Intellifex catalog against
  libifx's fixture knob tables, its pages, and each effect at its defaults
- `lib/libgmajor2`, `lib/libifx` - the effect libraries, built by their own Makefiles
- `lib/iplug2` - iPlug2 at the pinned revision
- `lib/vst3sdk` - the VST3 SDK as three component submodules (`base`,
  `pluginterfaces`, `public.sdk`), the directory itself is the SDK root
- `lib/clap`, `lib/clap-helpers` - the CLAP SDK, not built yet
- `lib/aaxsdk` - extract Avid's AAX SDK here (developer.avid.com), not built yet
- `Makefile` - submodule checkout, plugin and test targets
