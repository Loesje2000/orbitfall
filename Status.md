# Orbitfall — Current Status

Last updated: 20 June 2026

## Working build

- The JUCE 8 / C++17 project configures and builds successfully with CMake and Ninja.
- Fresh macOS builds were installed as AU and VST3 plug-ins.
- The standalone target also links successfully.

## Current implementation

- The core stereo predelay engine, Cathedra / 78 Hall / Gravity placeholder algorithms, gate, feedback insert, Hazy processor, modifiers, factory presets, and complete one-panel editor are implemented.
- Half-speed predelay changes now crossfade normal- and half-rate delay taps over 50 ms, preventing clicks when the switch is toggled.

## Known next work

- Replace the Gravity placeholder with the planned granular engine.
- Broaden the modifier target routing beyond decay and pitch.
- Add the remaining processing elements and refine the reverb algorithms through listening tests in a DAW.

## Verification

Validated locally with:

```sh
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja --config Release -j 4
```

The original `ORBITFALL_HANDOFF.md` remains the authoritative design handoff and is intentionally unchanged.
