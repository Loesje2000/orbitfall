# Orbitfall — Plugin Build Handoff
## For Claude Code / Codex

**Project:** Rubblesonic — Orbitfall  
**Type:** VST3 + AU reverb plugin for macOS  
**Stack:** JUCE 8 / C++17  
**Author:** Andrew Vroomans (Rubblesonic)  
**Status:** Architecture and UI fully designed. Ready to build.

---

## 1. What this plugin is

Orbitfall is a modular studio reverb plugin inspired by the Meris Mercury X hardware pedal. It is **not** an emulation of that pedal — it is a new instrument that takes the Mercury X's key architectural ideas (predelay-as-delay-engine, processing elements at multiple routing locations, feedback loop inserts, dry blend) and implements them natively as a plugin in the Rubblesonic aesthetic.

The sonic identity: spatial, textural, ambient. Designed for guitars, vocals, and synthesisers. The kind of reverb that turns a sound into a place. Rubblesonic's working tagline: *"Some recordings feel like places that never let you go."*

---

## 2. Rubblesonic build conventions — read these first

These are non-negotiable and apply to every Rubblesonic plugin:

- **JUCE version:** JUCE 8, modules at `/Applications/JUCE/modules`
- **Language standard:** C++17
- **Targets:** VST3 + AU, macOS only
- **Never re-run Projucer** after the initial Xcode export. All changes go directly into the Xcode project or source files. Re-running Projucer strips microphone permissions and other custom settings.
- **Xcode sandbox settings:** `ENABLE_APP_SANDBOX=NO`, `CODE_SIGNING_REQUIRED=NO`, `CODE_SIGNING_ALLOWED=NO`
- **Always use** `juce::FontOptions` (not deprecated font constructors) and `juce::ParameterID` (not raw strings) for parameter registration
- **Always deliver complete replacement files**, never partial diffs or snippets
- **Bundle ID pattern:** `com.rubblesonic.orbitfall`

---

## 3. Visual identity

Every Rubblesonic plugin uses the same visual system. Implement it exactly:

| Element | Value |
|---|---|
| Background | `#2C1F0E` (tobacco brown) |
| Accent / knob indicators | `#C8A840` (amber/gold) |
| Primary text | `#E8D5A3` (cream) |
| Secondary text / inactive labels | `#a07840` |
| Inactive borders / section borders | `#5a3e1e` |
| Deep section fill | `#3d2a0f` |
| Very dark fill | `#1a1208` |
| Rubblesonic logo | Sage green, −6° rotation, top-left |
| Plugin name | Cream, 18px, weight 500, slight letter-spacing |
| Knob style | Bakelite-style with tactile ridges, amber indicator line |

The UI is dark, warm, and analogue-feeling. No bright whites. No blue tones. Everything should feel like a piece of studio equipment from the early 1980s that somehow also has a deep software brain.

---

## 4. Signal flow architecture

This is the complete processing chain, executed in order in `processBlock`:

```
Input (L/R stereo)
    │
    ├──────────────────────────────────────────────────────┐
    │                                                      │
    ▼                                                      │ dry blend path
[PRE+DRY processing stage]                                │ (bypasses delay lines)
    │                                                      │
    ▼                                                      │
[Predelay engine] ◄────────────────────────────────────── │
    │   (twin stereo delay lines, crossfeed,               │
    │    modulation, half speed, type)                     │
    │                                                      │
    ▼                                                      │
[PRE-TANK processing insert]                              │
    │   (optional: pitch, filter, mod, dynamics)           │
    │                                                      │
    ▼                                                      │
    [+] ◄──────────────────────────────────────────────────┘
    │   (dry blend merge point)
    │
    ▼
[Reverb algorithm]
    │   (cathedra / ultraplate / gravity /
    │    78 hall / prism / spring)
    │
    ├──────────────────┐
    │                  │
    ▼                  │ feedback loop
[Feedback insert]      │
    │   (pitch shift,  │
    │    filter, hazy, │
    │    diffusion)    │
    │                  │
    └──────────────────┘ (feedback amount controls
    │                     how much returns to reverb input)
    │
    ▼
[POST processing stage]
    │   (filter, compressor, limiter, hazy)
    │
    ▼
[Mix stage]
    │   (wet trim, dry trim, mix knob)
    │
    ▼
Output (L/R stereo)
```

### Processing order within each stage (fixed)

When multiple elements are active at the same stage location, they always run in this order:
`Dynamics → Preamp → Filter → Pitch → Modulation`

### Which elements are available at each location

| Element category | pre+dry | pre-tank | feedback | post |
|---|---|---|---|---|
| Dynamics (compressor, swell, diffusion, freeze) | ✓ | ✓ | — | ✓ |
| Filter (ladder, state variable, parametric) | ✓ | ✓ | ✓ | ✓ |
| Pitch (poly chroma, lo-fi, micro shift) | ✓ | ✓ | ✓ ← key | ✓ |
| Modulation (hazy, chorus, vibrato, tremolo, vowel mod) | ✓ | ✓ | ✓ ← key | ✓ |

The feedback location is where the most interesting sounds come from. A lo-fi pitch shifter at +1 semitone in the feedback loop means each repeat rises in pitch — this is the "spiral" sound. A ladder filter in the feedback loop means each repeat gets progressively darker. This is the defining character of Orbitfall over other reverb plugins.

---

## 5. The predelay engine — detailed design

This is the most architecturally unusual component. Implement it carefully.

### Structure

```cpp
class PredelayEngine {
    DelayLine leftLine;      // circular buffer, ~5.5s at 48kHz (for half-speed headroom)
    DelayLine rightLine;     // independent time and subdivision
    CrossfeedMatrix xfeed;   // mixes L↔R output
    PredelayLFO modL;        // per-line modulation
    PredelayLFO modR;
    // PreTankInsert lives outside this class, between engine output and reverb input
};
```

### DelayLine

Each delay line is an independent circular buffer with:
- Write pointer (advances every sample)
- Read pointer with **fractional delay** using Hermite interpolation (4-point, 3rd order) — this is important for audio quality, especially at short delay times
- An LFO attached to the read pointer for modulation (the modulation *wobbles the read pointer*, not a separate wet signal)
- A `halfSpeed` flag that changes the effective write/read rate:
  - Normal: 48kHz sample rate, max ~2.54s
  - Half speed: writes and reads at half rate (equivalent to 24kHz), max ~5.08s, **pitch drops one octave**
  - Implement half speed by writing every other sample and reading at half the normal pointer increment — do not resample, the aliasing artefacts at 24kHz effective rate are part of the character

### Delay types — character per type

The delay type changes the character of the LFO on the read pointer:

- **Digital:** Clean fractional delay, Hermite interpolation, LFO is a gentle sine at very low depth (subtle stereo movement only)
- **BBD (bucket brigade):** Add a small amount of irregular clock noise jitter to the read pointer. Model as a sine LFO with very slight random perturbation on each cycle. Also apply a mild 1-pole lowpass to the delay output (BBD circuits naturally roll off highs). Cutoff around 8kHz at full `damping`.
- **Tape:** Slower, deeper wow-and-flutter. Use two overlapping sine LFOs at slightly different rates (e.g. 0.3Hz and 0.7Hz) summed with slight non-linearity. Also apply `damping` as a lowpass that gets progressively darker — model as a 2-pole filter whose cutoff sweeps from 18kHz (no damping) to around 3kHz (full damping). Tape type is the signature character — most presets should default to this.

### Crossfeed

The crossfeed matrix runs after both delay lines have produced output, before the feedback sum:

```
xfeedL_out = (1 - crossfeed) * leftLine_out  + crossfeed * rightLine_out
xfeedR_out = crossfeed * leftLine_out + (1 - crossfeed) * rightLine_out
```

At `crossfeed = 0`: pure independent L and R  
At `crossfeed = 0.5`: fully summed mono  
At `crossfeed = 1.0`: fully swapped (L becomes R, R becomes L — creates maximum ping-pong)

The Cathedra preset had crossfeed at 100% — this is a significant part of that preset's stereo width.

### Feedback path

Feedback is external to the delay lines. After the crossfeed matrix, the output goes to:
1. The pre-tank insert (optional processing)
2. The dry blend merge point (summed with direct dry signal)
3. The reverb algorithm input

The feedback tap comes from **after the reverb algorithm output**, multiplied by the feedback amount, and is summed back into the delay line inputs. This means the feedback loop is: delay → insert → reverb tank → back to delay. Each cycle through the loop adds another reverb decay to the repeating echo.

```cpp
// Simplified feedback loop per sample:
float delayOutL = leftLine.read();
float delayOutR = rightLine.read();

// Crossfeed
float xL = lerp(delayOutL, delayOutR, crossfeed);
float xR = lerp(delayOutR, delayOutL, crossfeed);

// Write with feedback from previous reverb output
leftLine.write(inputL + feedbackAmount * prevReverbOutL);
rightLine.write(inputR + feedbackAmount * prevReverbOutR);
```

### Dry blend

The dry blend parameter feeds a portion of the **input signal** (before the delay lines) directly into the reverb tank input, in parallel with the delay output. At 0% dry blend, the reverb onset is entirely controlled by the predelay time. At 50%, half the signal hits the reverb immediately and half hits it after the delay — creating a layered, complex onset that sounds much larger. At 100%, the predelay is bypassed entirely (but the delay echoes still feed back through the loop).

```cpp
float reverbInput = (1.0f - dryBlend) * predelayOutput + dryBlend * directInput;
```

### Modulation

The `mod` knob controls LFO depth on both delay lines simultaneously. The LFO is applied by modulating the fractional read pointer position:

```cpp
float readPos = writePos - delayTimeSamples + lfoAmount * lfoValue;
```

The LFO range should be subtle: even at `mod = 1.0`, the maximum read pointer modulation should not exceed ±4ms (±192 samples at 48kHz). Larger values cause obvious pitch warble that sounds like a broken tape machine rather than a living reverb.

### Half speed — implementation note

When half speed is toggled, the delay time in samples doubles instantly. To avoid a click, crossfade over ~50ms when switching. The pitch shift downward by one octave is a feature, not a bug — it is exactly what the Mercury X does and what makes the long ambient tails sound so deep and cavernous.

---

## 6. Reverb algorithms

For v1, implement three algorithms. They share a common gate system (see Section 7).

### 6a. Cathedra

A massive, slow-building ethereal reverb. Inspired by Blade Runner — "C-beams glittering in the dark near the Tannhäuser gate." Slow build, very long tail, integrated shimmer (pitch content woven into the diffusion network rather than added as a post-process).

Parameters: Decay, Lo Freq, Hi Freq, Mod Speed, Mod Depth, Pitch, Pitch Mix, Diffusion

Implementation approach: Large Schroeder network (8+ allpass stages) with the allpass coefficients modulated by a slow LFO (this is what creates the slow build and shimmer — the allpass modulation creates subtle pitch content as a side effect of the time-varying delays). Apply Lo Freq and Hi Freq as shelf EQ within the feedback of the diffusion network, not post-reverb — this is how the tonal character gets baked into the decay rather than just rolled off at the output.

The Cathedra preset from the SYX file had these key values:
- Mix: 78%, Dry trim: 17%, Wet trim: 26%
- Feedback: 33%, Cross-feedback: 100%
- Predelay mod: 50%, Dry blend: 50%
- Gate: attack 39%, hold 100% (maxed), decay 41%
- Reverb param 1 (decay): 90%, param 2 (lo freq): 55%

### 6b. Gravity

The most unusual algorithm. Rather than simulating a room, Gravity takes windowed segments of the input signal and stretches them — "accelerating individual windows of signal and stretching them across the horizon." The result is pad-like textures that grow beneath the original sound. Not echo, not shimmer, but something closer to the signal being dissolved into a cloud that retains pitch content.

Parameters: Decay, Tilt EQ, Mod Speed, Mod Depth, Mod Feedback, Gain

Implementation approach: Granular time-stretching on the reverb input — take overlapping windows (grain size ~80-200ms), pitch-shift them slightly up or down per grain (controlled by Mod Depth), and overlap-add them back together with an exponential decay envelope. The Gain control is critical — granular stretching can explode in volume easily. The `Mod Feedback` parameter feeds the output back into the grain buffer, creating chaotic textures at high values.

This is the hardest algorithm to implement well. If it needs to be deferred to v2, that is acceptable — but it should be in the design from day one.

### 6c. 78 Hall

A large hall with a medium build-up of reflections and an EQ-network-controlled decay. The distinctive feature is the EQ network: decay time for frequencies above a crossover point (`Cross`) is set by `Mids`, and decay below the crossover is set by `Bass`. This means you can have a short, tight low-frequency decay with a long, airy mid/high decay — which is how real large halls behave acoustically.

Parameters: Mids, Bass, Treble, Cross, Tank Mod, Diffusion

Implementation approach: Feedback delay network (FDN) with 8 delay lines. The EQ network lives in the feedback path of the FDN — a crossover filter at `Cross` Hz sends low frequencies through one decay multiplier and highs through another. `Treble` sets a corner frequency above which the feedback coefficient rapidly decreases, adding high-frequency absorption. `Tank Mod` is a slow LFO on the FDN delay line lengths (keeps the tail alive and avoids metallic resonances).

---

## 7. Gate system

All three reverb algorithms share the same gate. The gate is applied to the reverb output after the algorithm but before the feedback insert.

The gate in Orbitfall is **not** the classic punchy '80s gate. It is a slow atmospheric gate:
- **Attack:** How long the gate takes to open fully after the reverb starts. At 39% (the Cathedra preset value) this creates a slow bloom.
- **Hold:** How long the gate stays open at maximum. At 100% (maxed in Cathedra), the gate holds open indefinitely — it's not gating at all in the traditional sense, just shaping the onset.
- **Decay:** How long the gate takes to close after hold expires. At 41%, a slow fade.

The gate in Cathedra at these settings essentially acts as a slow fade-in shaper. Short hold values with fast attack and decay give the classic gated snare sound. Very short hold with fast decay gives a reverse-reverb effect (the reverb appears and is cut off before it decays naturally).

Implement as an envelope follower on the reverb output amplitude, feeding a VCA:
```
State machine: CLOSED → (signal detected) → ATTACK → HOLD → DECAY → CLOSED
```

---

## 8. Processing elements

### Pitch elements

**Poly Chroma:** Polyphonic pitch shifter. Sums stereo to mono, shifts pitch, outputs to both channels. Pitch in semitone increments (±12 semitones, 20-cent steps within each semitone). Use a phase-vocoder approach for best quality on complex chords.

**Lo-Fi:** Dual (independent L/R) pitch shifter using an early grain-based technique that creates modulated, low-fidelity voices. The artefacts — the slight metallic grain, the unstable tuning — are intentional and desirable. Pitch L and Pitch R can be set independently.

**Micro Shift:** Fine detuning only (±50 cents max), independent L/R. Used for stereo widening and subtle doubling. Much simpler to implement than Poly Chroma — a simple circular buffer with a slowly drifting read pointer per channel is sufficient.

### Filter elements

**Ladder Filter:** Moog-style ladder filter, stereo. Frequency (20Hz–20kHz), Resonance (0–self-oscillation), Topology (lowpass/bandpass/highpass), Spread (offsets right channel frequency from left). This is a classic 4-pole ladder — implement carefully for stability at high resonance.

**State Variable Filter:** Two-integrator-loop SVF, stereo. Same parameters as Ladder. Sounds different — more transparent, less "organic" than the ladder. Good for precise EQ-style filtering.

**Parametric:** Single-band parametric EQ. Frequency, Q (expressed as Resonance), Topology (shelf or peak), Gain (±10dB). Useful in the POST location for final tonal balancing.

### Dynamics elements

**Compressor:** Stereo compressor. Threshold, Ratio, Gain (makeup), Attack, Release, Mix (parallel compression). Use standard RMS detection.

**Swell:** Auto-volume swell. Detects pick attacks and applies an exponential fade-in over `Attack Time`. Removes the attack transient entirely at fast times, creates violin-bow-style swells. Best in PRE+DRY location.

**Diffusion:** Short multi-tap stereo delay network for smoothing hard-edged transients. Density (number of taps), LPF (post-diffusion lowpass). Very useful in the feedback loop location to progressively smooth repeats.

### Modulation elements

**Hazy:** Lo-fi texture engine. The Rubblesonic aesthetic is deeply connected to this one.
- **Age:** Tape degradation — adds gentle flutter and hiss character. Implement as a slow irregular LFO on pitch (max depth ±8 cents) plus white noise at very low level (−60dBFS at full Age).
- **Warble:** Slow tape pitch modulation. A sine LFO at 0.1–2Hz, depth up to ±30 cents.
- **Decimate:** Sample rate reduction (bit-crush style, but via sample rate, not bit depth). At minimum, full 48kHz. At maximum, ~4kHz effective. Adds overtones similar to ring modulation.
- **Lows / Highs:** Shelf EQ trim at low and high end. Unity at 100%.
- **Mix:** Parallel blend of hazy processing with dry signal through this element.

**79 Chorus:** Classic one-knob chorus with expanded depth control. Speed (0.1–10Hz), Depth (0–25ms modulation range), Note Division (sync to predelay tempo). Based on the BBD chorus sound of 1979.

**Vibrato:** Pitch-only modulation (no dry signal mixed in, unlike chorus). Speed, Depth, Note Division.

**Tremolo:** Amplitude modulation. Speed, Waveshape (sine/triangle/square/ramp), Mix, Spread (L/R phase offset for stereo tremolo), Voice (Opto = classic volume pulse / Harmonic = alternating filter).

---

## 9. Modifiers system

The modifiers system allows any parameter to be modulated automatically. This is what makes Orbitfall feel alive rather than static.

Four modifier sources, two of which are active simultaneously (Modifier A and Modifier B):

**LFO:** Periodic oscillator. Parameters: Rate (0.01–20Hz), Depth (0–100% of target parameter range), Shape (sine/triangle/ramp-up/ramp-down/square/3-step/4-step), Note Division (sync to predelay time), Target (any assignable parameter).

**Envelope:** Note-triggered envelope generator. Detects input transients. Parameters: Attack Time, Decay Time, Shape (linear/exponential/clipped), Target.

**Sample & Hold (S+H):** Generates a new random value at each cycle. Parameters: Rate, Note Division, Target. Creates random stepped parameter automation — very effective assigned to pitch for glitchy sounds.

**Sequencer:** 7-step pattern playback (note: the Cathedra and tpc.GL MKII presets both contained an identical 7-step ascending ramp pattern `[14%, 28%, 43%, 57%, 72%, 86%, 100%]` in the modifier region — this is the factory sequencer default and creates the signature slow upward pitch drift in the reverb tail). Steps can be set individually from 0–100%. Rate and Note Division as above.

### Assignable targets

Any of these parameters can be a modifier target:
- Reverb: decay, pitch, pitch mix, diffusion, lo freq, hi freq
- Predelay: time, feedback, crossfeed, mod
- Gate: attack, hold, decay
- Feedback insert: amount, param
- Hazy: age, warble, decimate
- Filter: frequency, resonance
- Post: mix

The modifier assigns to a *percentage of the current parameter value*, not an absolute range. This means you can turn the underlying parameter and the modulation range follows — exactly as the Mercury X behaves.

---

## 10. Parameters — full list for AudioProcessorValueTreeState

```cpp
// REVERB
"reverb_algorithm"   // int 0-5: cathedra/ultraplate/gravity/78hall/prism/spring
"reverb_decay"       // float 0-1
"reverb_size"        // float 0-1
"reverb_diffusion"   // float 0-1
"reverb_lo_freq"     // float 0-1
"reverb_hi_freq"     // float 0-1
"reverb_pitch"       // float -12 to +12 semitones
"reverb_pitch_mix"   // float 0-1

// PREDELAY ENGINE
"pre_type"           // int 0-2: tape/BBD/digital
"pre_time"           // float 0-2500ms (log taper)
"pre_feedback"       // float 0-1
"pre_crossfeed"      // float 0-1
"pre_mod"            // float 0-1
"pre_half_speed"     // bool
"pre_dry_blend"      // float 0-1

// GATE
"gate_attack"        // float 0-1
"gate_hold"          // float 0-1
"gate_decay"         // float 0-1

// FEEDBACK INSERT
"insert_type"        // int 0-4: none/lo-fi/ladder/hazy/diffusion
"insert_amount"      // float 0-1
"insert_mix"         // float 0-1
"insert_param"       // float 0-1 (meaning depends on insert_type)

// MODIFIERS
"mod_a_type"         // int 0-3: lfo/envelope/sh/sequencer
"mod_a_rate"         // float 0.01-20 Hz
"mod_a_depth"        // float 0-1
"mod_a_shape"        // int 0-6: sine/tri/rampup/rampdown/square/3step/4step
"mod_a_target"       // int (enum of assignable targets)
"mod_b_type"         // int 0-3
"mod_b_rate"         // float
"mod_b_depth"        // float
"mod_b_shape"        // int
"mod_b_target"       // int

// HAZY
"hazy_age"           // float 0-1
"hazy_warble"        // float 0-1
"hazy_decimate"      // float 0-1
"hazy_mix"           // float 0-1

// OUTPUT
"mix"                // float 0-1
"dry_trim"           // float 0-1
"wet_trim"           // float 0-1
"spillover"          // bool
"trails"             // bool
```

Use `juce::ParameterID{"parameter_name", 1}` for all parameter registrations.

---

## 11. UI layout — complete specification

The UI is a single panel, no tabs. All controls visible at once. Laid out top-to-bottom in five horizontal sections. The sections and their content, in order:

### Header row
- Rubblesonic logo (sage green, −6° rotation, left)
- "orbitfall" plugin name (cream, 18px, centre or right of logo)
- Preset navigation: left arrow | preset name field | right arrow (right side)

### Section 1: Reverb (top, full width)
Algorithm selector strip (six buttons, left-to-right): `cathedra` / `ultraplate` / `gravity` / `78 hall` / `prism` / `spring`  
Active algorithm button is highlighted (amber border, slightly lighter background).

**Primary knob row** (large knobs, 48px):
`decay` — `size` — `diffusion`

**Secondary knob row** (mini knobs, 26px, immediately below primary row):
`lo freq` — `hi freq` — `pitch` — `pitch mix`

The secondary row is visually subordinate — smaller knobs, slightly more muted labels — but still accessible and clearly labelled. This solves the "seven knobs in a row" problem from the earlier mockup.

### Section 2: Two-column row

**Left column — Predelay section:**
- Type selector: `tape` / `BBD` / `digital` (three small buttons)
- Four knobs (medium, 36px): `time` — `feedback` — `crossfeed` — `mod`
- Divider
- Toggle: `half speed`
- Inline control: `dry blend` label + mini knob + value display

**Right column — Gate + Feedback Insert:**
- Three knobs (medium): `attack` — `hold` — `decay`
- Divider
- Label: "feedback insert"
- Type selector slot (single clickable field showing current type, e.g. "lo-fi pitch"): clicking opens a small popover with options: `none` / `lo-fi pitch` / `ladder filter` / `hazy` / `diffusion`
- Three mini knobs: `amount` — `mix` — `param` (third knob label changes with type)

### Section 3: Modulation (full width, three sub-columns)

**Column 1 — Modifier A:**
- Type selector: `LFO` / `env` / `S+H` / `seq`
- Three mini knobs: `rate` — `depth` — `→ target` (target knob is a discrete selector, not continuous)

**Column 2 — Modifier B:**
- Same structure as Modifier A

**Column 3 — Hazy:**
- Label: "hazy"
- Four mini knobs: `age` — `warble` — `decimate` — `mix`

### Section 4: Output (full width)
- Three large knobs: `mix` — `dry trim` — `wet trim`
- Right side: two toggles stacked vertically: `spillover` / `trails`

### Depth hint (below all sections)
Small muted text: `"click feedback insert type to expand routing"`

---

## 12. Reference presets — implement these as factory presets

These values come from parsed SYX data from real Mercury X presets. Use them as the starting point for factory presets. Values are approximate percentages of the 0–127 MIDI range unless noted.

### Preset: "Cathedra dark" (from MercX-Cathedra-Reverb SYX)
- Algorithm: Cathedra
- Mix: 78%, Dry trim: 17%, Wet trim: 26%
- Predelay type: digital, Time: short (~15ms), Feedback: 33%, Crossfeed: 100%, Mod: 50%
- Half speed: off, Dry blend: 50%
- Reverb decay: 90%, Reverb lo freq: 55%, Reverb hi freq: 48%
- Reverb pitch: ~19% (gentle shimmer), Pitch mix: 19%
- Gate: Attack 39%, Hold 100%, Decay 41%
- Feedback insert: none (or compressor with heavy ratio ~69% threshold, 100% ratio)
- Spillover: on

**Why it sounds the way it does:** The 100% crossfeed creates a wide stereo spiral in the predelay before the signal hits the reverb tank. The 50% dry blend means the reverb onset is immediate (some dry signal) and also delayed (the predelay output). The nearly-maxed gate hold means the gate is essentially open the whole time — it's just shaping the onset bloom. The slow shimmer from the pitch parameter woven into the Cathedra diffusion network makes the tail feel alive without sounding like a cheap shimmer pedal.

### Preset: "Spiral hall" (from tpc.GL MKII MercX SYX)
- Algorithm: Ultraplate (or 78 Hall)
- Mix: 24%
- Predelay type: digital, Half speed: ON
- Feedback: 17% crossfeed, very low direct feedback
- Reverb decay: ~97%, Reverb mod speed: ~91%, Reverb pitch: ~64%, Pitch mix: 100%
- Gate: Hold 11% (subtle)
- Feedback insert: lo-fi pitch (the defining feature — creates the ascending spiral)
- Modifier A: sequencer, 7-step ascending ramp [14→28→43→57→72→86→100%], target: reverb pitch

**Why it sounds the way it does:** Half speed drops everything an octave and doubles the tail length. The sequencer-driven pitch ramp creates stepped upward movement through the tail — not a continuous shimmer but a rising staircase of pitch that feels like the sound is ascending into the upper atmosphere. The lo-fi pitch in the feedback insert means every echo through the feedback loop adds another semitone up. With low direct feedback this is subtle; turning up feedback makes it an infinite ascending spiral.

### Preset: "Empty room" (simple starting preset)
- Algorithm: 78 Hall
- Mix: 30%, all trims at 100%
- Predelay: tape type, 20ms, no feedback, no crossfeed, mod 20%
- Half speed: off, Dry blend: 0%
- Reverb: Decay 40%, Size 50%, Diffusion 60%
- No gate, no insert, no modifiers

---

## 13. Class structure recommendation

```
OrbitfallAudioProcessor
├── PredelayEngine
│   ├── DelayLine leftLine
│   ├── DelayLine rightLine
│   ├── PredelayLFO modLfoL
│   ├── PredelayLFO modLfoR
│   └── (crossfeed is inline computation, no separate class needed)
├── ProcessingStage preTankStage    // pre-tank location
├── ReverbEngine
│   ├── CathedraReverb
│   ├── GravityReverb
│   ├── HallReverb78
│   └── ReverbGate
├── FeedbackInsert                  // processes signal in feedback loop
├── ProcessingStage postStage       // post location
├── ModifierSystem
│   ├── ModifierA
│   └── ModifierB
├── HazyProcessor                   // always available, in modulation section
├── MixStage                        // wet/dry/trims
└── OrbitfallEditor (JUCE component)
    ├── AlgorithmSelector
    ├── ReverbSection (primary + secondary knob rows)
    ├── PredelaySection
    ├── GateSection
    ├── FeedbackInsertSlot
    ├── ModifierSection
    ├── HazySection
    └── OutputSection
```

---

## 14. Files to reference

The following files should be uploaded alongside this handoff:

- `MercX-Cathedra-Reverb.syx` — Cathedra preset, SysEx binary. The key preset that defines Orbitfall's primary character. See Section 12 for decoded values.
- `tpc_GL_MKII_MercX.syx` — Spiral hall preset (MercuryX version). See Section 12.
- `tpc_GL_MKII_LVX.syx` — Same preset on the LVX delay pedal — useful as reference for the predelay engine character in isolation.
- `MercuryX_Manual_v1_2_2.pdf` — Full Mercury X manual. Reference for: reverb structure parameters (Section 9), processing element details (Section 10), modifier system (Section 6), predelay parameters (Section 8). The MIDI CC table (Section 11) was used to decode the SYX files.
- `[English] Meris Mercury X - Is this the Big Sky Killer.txt` — Video transcript. Contains subjective descriptions of how the reverb feels ("the shimmer feels like part of the sound rather than an addition") that inform the tuning goals.
- `[English] Meris MercuryX Make Anything Sound Cinematic and Beautiful.txt` — Video transcript. More detailed walkthrough of sound design workflow — useful for understanding how parameters interact.

---

## 15. Build sequence recommendation

Start in this order to build up complexity gradually:

1. **Scaffold:** Create the Projucer project, set up `AudioProcessorValueTreeState` with all parameters from Section 10, verify it loads in a DAW with no audio processing yet.

2. **Predelay engine only:** Implement `DelayLine` with Hermite interpolation, the two-line stereo engine, crossfeed, and the `mod` LFO. No reverb yet. Test: dry input should produce a clear stereo delay with modulation. This validates the most unusual architectural component before adding complexity.

3. **Cathedra reverb algorithm:** Add the reverb tank. At this point you have the core Orbitfall sound: predelay feeding a reverb. Test against the Cathedra preset values from Section 12.

4. **Gate system:** Add gate to the reverb output. Verify the Cathedra preset's slow bloom behaviour.

5. **Feedback insert:** Add the `FeedbackInsert` between reverb output and feedback sum. Test with lo-fi pitch: turning feedback up with a pitch shift should create the ascending spiral.

6. **Hazy:** Add hazy processor. Test in post location.

7. **Modifier system:** Add LFO and sequencer. Test sequencer with the 7-step ascending ramp from the Spiral hall preset.

8. **78 Hall and Gravity algorithms:** Add remaining reverb algorithms.

9. **Remaining processing elements:** Filter, compressor, swell, etc.

10. **UI:** Build the editor component last, using the layout from Section 11. The visual identity specifications from Section 3 apply throughout.

---

## 16. Known edge cases and implementation notes

- **Feedback stability:** When pitch shift is in the feedback loop with `feedback > 0`, the signal level can escalate quickly. Add a soft limiter (−6dBFS ceiling) on the signal entering the feedback sum. This is not a creative limiter — it is a safety valve.

- **Half speed switching:** Crossfade over 50ms when toggling `half_speed` to avoid clicks. The pitch shift downward is intentional and desirable.

- **Spillover vs. trails:** `Spillover` means the reverb tail from the previous preset continues while the new preset fades in. `Trails` means the reverb tail decays naturally when the plugin is bypassed (rather than cutting). These are independent features. Both require the audio engine to continue processing after a preset change or bypass event.

- **Projucer:** Do NOT re-run Projucer after initial setup. See Section 2. If you accidentally re-run it, patch `Info-App.plist` directly via Python `plistlib`.

- **Xcode sandbox:** Set `ENABLE_APP_SANDBOX=NO`, `CODE_SIGNING_REQUIRED=NO`, `CODE_SIGNING_ALLOWED=NO` in Xcode build settings. Without these the plugin will not load in most DAW hosts on macOS.

- **Sample rate changes:** The delay line buffer sizes and all time-based parameters must recalculate when `prepareToPlay` is called with a new sample rate. Allocate buffers at 48kHz length even if the session is at 44.1kHz — this avoids reallocating on sample rate switches.

- **The Gravity algorithm:** This is the hardest to implement. If it causes delays, implement a placeholder that outputs processed noise with a slow envelope (it will still sound interesting) and mark it for replacement. Do not let it block the rest of the build.

---

*Handoff prepared: June 2026. All architectural decisions and sonic reasoning documented above are final and agreed with the project owner.*
