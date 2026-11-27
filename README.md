# IMajor2

An iPlug2 VST3 audio-effect plugin that wraps the C17 `libgmajor2` library. It
exposes the library's 36 bit-exact re-creations of the TC Electronic G-Major 2
effects through a single effect menu and per-effect knob automation. The editor
is drawn by IGraphics with the Skia backend on OpenGL 3.

## Requirements

- Linux with the X11, OpenGL, Fontconfig, Expat and FreeType development headers
  (see iPlug2's Linux prerequisites), plus Python 3, pkg-config and Ninja for
  the Skia build
- GNU Make 4.3 or newer, a C17/C++17 compiler, Git, curl and sha256sum
- The `libgmajor2` sources, a submodule at `lib/libgmajor2`; `make` initialises
  it when it is missing

## Build

```sh
git clone --recurse-submodules git@github.com:re-rack/rerack-plugin.git
cd rerack-plugin
make            # the VST3 bundle, build/out/IMajor2.vst3
make test       # build and run the tests
make install    # copy the bundle into ~/.vst3, replacing an older copy
make deps       # fetch and build the dependencies, without the plugin
make clean      # remove build/ and the Skia build leftovers
```

The first `make` fetches pinned iPlug2 and VST3 SDK versions into `deps/` and
builds iPlug2's pinned Skia into `deps/skia`; that build is long and leaves about
8.5 GB of sources behind. Later runs reuse it, and `make clean` keeps
`deps/skia/lib` so they still do. Every fetch and build is skipped once its
result is in place.

Variables can be overridden on the command line, for example `make -j16`,
`make VST3_DIR=/usr/local/lib/vst3 install`, `make BUILD=/tmp/imajor2` or
`make GM2_DIR=../libgmajor2` to use a libgmajor2 checkout outside the submodule.

## Mix and Out Level

Where the device has them, an effect exposes Mix (0 to 100 %) and Out Level (-100
to 0 dB) with the device's factory defaults. Both are passed to the library raw:
the effect does the device's equal-power dry/wet blend and folds Out Level into
its level words, so the plugin adds no gain or smoothing of its own. The EQ, Noise
Gate and Compressor have neither knob, as on the device.

## De-pop

Some knobs click when turned fast, because the device does. The De-pop switch
(on by default) runs the library's de-popped variant of the effects that have one:
the Three-Band EQ, the Resonance Filter, both Pitch Shifters, the Reverse Delay
and the four Reverbs. A variant gives the effect's own output, sample for sample,
once its knobs have settled, and costs about twice the effect while a knob moves.
For the other effects the switch is greyed out. Flipping it restarts the effect,
as switching effects does.

## Sample rates

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

## Layout

- `Source/IMajor2.*` - parameters, audio processing, VST3 parameter titles and text input
- `Source/IMajor2Editor.cpp` - fixed-size IGraphics editor
- `Source/config.h` - iPlug2 plugin configuration
- `Source/EffectCatalog.*` - effect and knob tables, raw-to-parameter mapping, knob value text
- `Source/EffectEngine.h` - templated engine wrapper around each library module
- `Source/Resampler.*` - host-rate to base-rate conversion, Remez half-band filter design
- `tests/resampler_test.cpp` - resampler latency, passband, aliasing and block-size checks
- `tests/depop_test.cpp` - the De-pop engines against their effects
- `tests/knob_format_test.cpp` - knob value text and its parsing
- `lib/libgmajor2` - the effect library, built by its own Makefile
- `Makefile` - dependency fetch, Skia build, plugin and test targets
