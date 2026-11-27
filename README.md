# duddex's VCV Rack Modules

A collection of modules for [VCV Rack](https://vcvrack.com/) by duddex.

**Note:** These modules were created as an experiment in how GitHub Copilot and LLMs can assist in software development. The entire workflow — from setting up the build environment (downloading and configuring MSYS2/MinGW and the Rack SDK), to writing module code, designing panel layouts, and authoring this very README — was done with significant LLM support using GitHub Copilot with Claude Opus 4.8.

## Table of Contents

- [Modules](#modules)
  - [Tropical Oscillator](#tropical-oscillator)
  - [606 Drums](#606-drums)
  - [Shimmer Reverb](#shimmer-reverb)
  - [Blinkenlights](#blinkenlights)
  - [Blinkenlights Plus](#blinkenlights-plus)

## Modules

### Tropical Oscillator

A polyphonic oscillator based on **Tropical Additive Synthesis**, combining five cosine oscillators using the **minimum** operator (tropical addition in min-plus algebra) instead of summation. This produces complex, angular waveforms rich in harmonics, with controls for per-oscillator frequency multipliers, detuning, and amplitude offsets (tropical VCAs), plus polyphonic V/OCT input and a DC offset knob.

For full documentation, see [TropicalOscillator.md](TropicalOscillator.md).

### 606 Drums

A seven-voice analog-style drum module inspired by the classic Roland TR-606. Each voice — **Kick**, **Snare**, **Clap**, **Closed Hat**, **Open Hat**, **Low Tom**, and **High Tom** — has its own trigger input, tuning and decay knobs (plus a character knob on the voices that support it), and a level control. Every knob has an accompanying CV input, so the parameters can be modulated from sequencers, LFOs, or envelopes. All voices are summed to a single mono mix output on the right. Triggering the closed hat chokes the open hat, just like the hardware.

![606 Drums module screenshot](images/606-drums.png)

The drum DSP comes from Matthew Fecher's [606 Inspired Synth Drums](https://github.com/analogcode/606-Inspired-Synth-Drums) (MIT licensed), which is included in this repository as a **git submodule** under `lib/606-Inspired-Synth-Drums`. Because of this, after cloning the repository you must initialize the submodule before building:

```sh
git submodule update --init
```

The submodule provides header-only C++14 DSP, so this module is compiled with `-std=c++14` (see the plugin `Makefile`), and its `Source` directory is added to the include path.

| Control | Type | Range | Description |
|---------|------|-------|-------------|
| **TRIG** | Trigger input | — | Fires the voice on a rising edge (per drum) |
| **TUNE** | Knob + CV | -12 – +12 semitones | Pitch of the voice (CV is 1V/oct) |
| **DECAY** | Knob + CV | 0 – 100% | Length of the voice's tail |
| **CHAR** | Knob + CV | 0 – 100% | Character: Kick snap, Snare snappy, Clap noise/air (Kick, Snare, Clap only) |
| **LEVEL** | Knob | 0 – 100% | Voice level into the mix |
| **MIX OUT** | Audio output | — | Summed mono output of all voices |

### Shimmer Reverb

A stereo **shimmer reverb**. A plate-style reverb tank has a pitch shifter in its feedback loop. Each trip around the loop adds another pitch-shifted copy, so the tail slowly climbs (by default an octave at a time) and fades out at the top. This is the "swelling octaves" sound Brian Eno and Daniel Lanois created in the early 1980s with a pitch shifter feeding a hall reverb.

![Shimmer Reverb module screenshot](images/shimmer-reverb.png)

The controls are designed for the characteristic swelling-octave shimmer effect, while the algorithm itself is original.

How it works:

- The input is summed to mono, pre-delayed, and smeared by four input allpass diffusers.
- The reverb tank is the "figure-eight" topology from Jon Dattorro's *Effect Design Part 1* (J. Audio Eng. Soc., 1997): two cross-coupled chains of modulated allpasses, delays, and damping filters, read through Dattorro's 7 output taps per side.
- A two-voice delay-line pitch shifter sits in one of the tank's cross-feed paths. Its output is filtered by the Tone control and blended back into the loop with constant power, so the shimmer amount doesn't change the decay time.
- **Hold** makes the tank lossless: decay goes to 100%, the input is muted, and damping, modulation and shimmer fade out. Whatever is in the reverb then rings indefinitely.

| Control | Type | Range | Description |
|---------|------|-------|-------------|
| **DECAY** | Knob + CV | 0.4 – 40 s | Reverb time (RT60) |
| **AMOUNT** | Knob + CV | 0 – 100% | How much pitch-shifted signal is fed back into the tank |
| **MIX** | Knob + CV | 0 – 100% | Equal-power dry/wet balance |
| **PRE-DLY** | Knob + CV | 0 – 500 ms | Delay before the reverb starts |
| **TONE** | Knob + CV | 500 Hz – 18 kHz | Damping cutoff of the tank and the shimmer path (darker tone = fewer high layers) |
| **MOD** | Knob + CV | 0 – 100% | Depth of the slow modulation of the tank allpasses (adds chorus and smooths metallic ringing) |
| **PITCH** | Selector | 6 modes | Shimmer interval: +1 oct, +5th, +oct +5th, ±1 oct, +1 & +2 oct, −1 oct |
| **HOLD** | Latch button + gate | — | Infinite hold, while the button is on or the gate is high |
| **IN L / R** | Audio inputs | — | Stereo input; R is normalled to L for mono sources |
| **OUT L / R** | Audio outputs | — | Stereo output |

CV inputs add to their knob: 10 V covers the full knob range.

### Blinkenlights

A simple utility module with a blinking LED. A knob controls the blink frequency from 0.1 Hz to 5 Hz (default 1 Hz), and a button cycles the LED color through red, yellow, and green. Useful for visual tempo reference or just for fun.

![Blinkenlights module screenshot](images/blinkenlights.png)

| Control | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| **FREQ** | Knob | 0.1 – 5 Hz | 1 Hz | Blink frequency |
| **COLOR** | Button | — | Red | Cycles LED color: red → yellow → green |


### Blinkenlights Plus

An extended visual utility module that demonstrates illuminated Rack UI components. It blinks a **light bezel** and a **light slider** in sync. The slider controls blink frequency, and a color knob sweeps through a continuous RGB palette.

![Blinkenlights Plus module screenshot](images/blinkenlights-plus.png)

| Control | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| **COLOR** | Knob | 0 – 255 | 36.5 | Continuous RGB color control (left = black, right = white). Default maps to red. |
| **BEZEL** | Light Bezel | — | — | Illuminated bezel element, blinking with the selected color. |
| **FREQ** | Light Slider | 0.1 – 5 Hz | 2.55 Hz | Blink frequency control for both illuminated elements. |

Color transitions follow this path:

black → red → yellow → green → cyan → blue → magenta → white

## License

- **Source code**: [GPL-3.0-or-later](LICENSE), © duddex.
- **Panel graphics** (all SVG files in `res/`): [GPL-3.0-or-later](LICENSE), © duddex. You may reuse and modify them under the terms of that license.
- **606 drum DSP** (`lib/606-Inspired-Synth-Drums`): MIT, © Matthew Fecher. See [LICENSE-606-Inspired-Synth-Drums.txt](LICENSE-606-Inspired-Synth-Drums.txt).

The "duddex" name is not covered by these licenses; please don't use it to brand derivative plugins.
