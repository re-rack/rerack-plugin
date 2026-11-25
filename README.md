# GMajor2

A JUCE 9 VST3 audio-effect plugin that wraps the C17 [libgmajor2](../libgmajor2)
library. It exposes the library's 36 bit-exact re-creations of the TC Electronic
G-Major 2 effects through a single effect dropdown and per-effect knob
automation.

## Requirements

- CMake 3.22 or newer
- Ninja and a C++17 compiler
- The `libgmajor2` source tree (defaults to `../libgmajor2`, override with
  `-DGM2_DIR=/path/to/libgmajor2`)
- JUCE 9.0.6, fetched automatically by CMake

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The VST3 is written to
`build/GMajor2_artefacts/Release/VST3/GMajor2.vst3`. Pass
`-DGM2_DIR=/path/to/libgmajor2` if the library is not next to this project.

## Sample rates

Effects run directly at host rates up to 48 kHz, including 44.1 and 48 kHz, with
zero latency. Above 48 kHz the plugin divides the rate by the first power of two
(1, 2, 4, ... 64) that the library accepts, runs the effect at that base rate,
and converts back to the host rate. The resampling filters' delay is reported to
the host with `setLatencySamples`. If no power of two produces a valid rate, the
plugin passes audio through untouched.

Tempo-synced knobs follow the host BPM; LFO effects are aligned to the song
position when playback starts, after an effect switch, and after a position
jump.

## Layout

- `Source/PluginProcessor.*` - audio processing, APVTS parameters, state
- `Source/PluginEditor.*` - fixed-size effect/knob editor
- `Source/EffectCatalog.*` - effect and knob tables, raw-to-parameter mapping
- `Source/EffectEngine.h` - templated engine wrapper around each library module
- `Source/Resampler.*` - host-rate to base-rate conversion
- `tests/resampler_test.cpp` - standalone resampler test
- `CMakeLists.txt` - JUCE fetch, plugin target, test target
