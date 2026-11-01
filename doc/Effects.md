Effects
=======

Ambidex adds a global FX chain, which Dexed doesn't have. It processes the mix of both engines
(after the A/B crossfade), and is shown on the right of the Dexed editor.

The chain has six modules, always in this order:

**Distortion → Filter → Compressor → Modulation → Delay → Reverb**

followed by the output gain and a soft clipper.

Morphing
--------

Each knob has an A value and a B value. Fully on A (or B), the knobs edit the values of that
preset. In between, the chain uses values interpolated with the morph slider position: the FX panel
is then greyed (it can't be edited), and its knobs show the values heard, moving with the slider.

Values are interpolated on the knob's scale, which is perceptual: frequencies, times and ratios on
a log scale, gains in dB. Halfway between 100 Hz and 10 kHz is therefore 1 kHz, not 5 kHz. The
interpolated values glide over 50 ms, so that jumps (e.g. clicking A while morphing) don't click.

A module that only one of the presets uses (its mix is at 0% in the other preset) isn't
interpolated: its knobs keep the values of the preset that uses it, and only its mix is morphed
(from 0% to that preset's mix). For example, if only preset B has a delay, the echoes keep B's time
and feedback during the whole morph, and simply fade in as the slider moves towards B, instead of
also morphing from the (unheard) delay settings stored in preset A.

As a consequence, **all controls are continuous**: a switch (on/off, filter type...) can't be
interpolated. Choices that are usually switches are knobs that blend from one character to the
next (see Shape, Type and Character below), and each module's **Mix** knob is also its bypass.
A module whose mix is at 0% is bypassed (it uses no CPU), and is dimmed on the panel.

FX switch
---------

The **ON / OFF** switch at the top of the FX panel bypasses the whole chain (the mixes fade out
over 50 ms), to listen to the presets dry, e.g. while editing their FM patch. It is a listening
tool: it isn't saved in the presets, isn't morphed, and is always on when Ambidex starts. The FX
knobs can still be edited while the chain is off. The output soft clipper (see below) stays on.

Why these modules
-----------------

Six modules at most, to keep the FX panel as tall as the Dexed editor. They cover the effects most
used on FM sounds, which tend to be bright, dry and static:

* **Distortion** adds grit and harmonics; DX7 sounds have none.
* **Filter** is the subtractive synthesis tool the DX7 lacks: it tames the brightness of FM, or
  sweeps it (morphing the cutoff between A and B is a filter sweep).
* **Compressor** evens out the dynamics, e.g. the resonant peaks of the filter, or the attack of
  percussive sounds.
* **Modulation** (chorus, flanger, phaser) gives movement and width to static sounds; the classic
  DX7 electric piano is almost always chorused. Chorus, flanger and phaser are merged into one
  module, which is how all seven candidate effects fit in six modules.
* **Delay** and **Reverb** give space.

Why this order
--------------

The order can't be changed, so it follows the most common signal chain, where each module helps
the next ones:

1. **Distortion first**: it is the only module that reacts strongly to the input level, so it gets
   the raw signal of the engines. Its harmonics then go through the filter.
2. **Filter after the distortion**, as in a guitar amp or a synth voice: the filter shapes (or
   sweeps) the harmonics created by the distortion. The reverse order would only filter what goes
   into the distortion, which then brings back bright harmonics.
3. **Compressor after the filter**: it controls the resonant peaks of the filter, and sends an even
   level to the time-based effects.
4. **Modulation before the time-based effects**, as on a pedalboard: the echoes and the reverb
   tail of a chorused sound are chorused too, which sounds wider than chorusing the reverb.
5. **Delay before the reverb**, so that the echoes sit in the same room as the dry sound.
6. **Reverb last**: it is the "room" of the whole sound.

Modules and controls
--------------------

| Module | Control | Range | Scale |
|---|---|---|---|
| Distortion | Drive | 0 to 36 dB | dB |
| | Shape | soft → hard → fold | blend |
| | Tone | 1 to 20 kHz (low-pass) | log |
| | Mix | 0 to 100% | linear crossfade |
| Filter | Cutoff | 20 Hz to 20 kHz | log |
| | Reso | Q 0.5 to 12 | log |
| | Type | low-pass → band-pass → high-pass | blend |
| | Mix | 0 to 100% | linear crossfade |
| Compressor | Thresh | -48 to 0 dB | dB |
| | Ratio | 1:1 to 20:1 | log |
| | Attack | 0.1 to 100 ms | log |
| | Release | 10 to 1000 ms | log |
| | Mix | 0 to 100% (parallel compression) | linear crossfade |
| Modulation | Rate | 0.05 to 10 Hz | log |
| | Depth | 0 to 100% | linear |
| | Feedback | -90 to +90% | linear |
| | Character | chorus → flanger → phaser | blend |
| | Mix | 0 to 100% | equal-power crossfade |
| Delay | Time | 10 ms to 2 s | log |
| | Feedback | 0 to 95% | linear |
| | Tone | 0.5 to 20 kHz (low-pass in the feedback) | log |
| | Spread | stereo → ping-pong | linear |
| | Mix | 0 to 100% | send-like (see below) |
| Reverb | Size | 0 to 100% | linear |
| | Damping | 0 to 100% | linear |
| | Width | 0 to 100% | linear |
| | Mix | 0 to 100% | send-like (see below) |

Notes:

* **Mix laws.** The wet signal of the distortion, filter and compressor is correlated with the dry
  signal: a linear crossfade keeps the level. The modulation's wet signal is partly correlated: an
  equal-power (sin/cos) crossfade avoids a level dip in the middle. Echoes and reverb tails are
  added to the sound: the dry signal stays at full level up to 50%, where the wet signal reaches
  full level, then the dry signal fades out (100% = wet only).
* **Distortion.** The shape blends a soft curve (tanh), a hard clipper and a sine wavefolder. The
  output level is roughly compensated (by the square root of the drive gain). There is no
  oversampling, to save CPU, so high drives alias.
* **Filter.** A 12 dB/octave state variable filter, whose low-pass, band-pass and high-pass outputs
  are blended by the Type knob. The band-pass has unity gain at the cutoff.
* **Compressor.** The make-up gain is automatic (half of the gain reduction of a 0 dB signal).
* **Modulation.** The first half of the Character knob shortens the delay of the chorus (20 ms)
  into a flanger (1.5 ms); the second half crossfades the flanger into a 6-stage phaser (centred
  on 1 kHz). Rate, depth and feedback apply to both.
* **Delay.** The time is set in milliseconds (the standalone app has no tempo to sync to). When it
  changes (knob or morph), the delay starts reading at the new time as well, and crossfades to it
  over 30 ms: the echoes move to the new time without any pitch change (unlike a tape delay, whose
  pitch bends while its time changes, which sounds odd when morphing). While the time keeps
  changing, the crossfades follow each other. With Spread at 100%, the input goes to the left line
  only, and each line feeds the other one (ping-pong).
* **Reverb.** Freeverb, without its freeze mode (a switch).

Output gain
-----------

The **Output** knob, at the top right of the FX panel, sets the level of the preset after the FX
(-24 to +12 dB, 0 dB by default), e.g. to balance a preset with heavy FX against a dry one. Like the
other knobs, it is saved in the preset, and morphed (in dB) between A and B. The ON/OFF switch
doesn't bypass it.

Output soft clipper
-------------------

After the output gain, the signal goes through a fixed soft clipper (no controls, always on, even
when all the modules are bypassed), so that loud settings don't clip harshly:

* up to 0.7 (about -3 dBFS), the signal is untouched;
* above, peaks are rounded off by a tanh curve that tends to 1 (0 dBFS) without reaching it. The
  curve has the same slope as the untouched signal at 0.7, so the transition is smooth.

Unlike a limiter, it has no lookahead, hence no latency, and costs almost no CPU. The price is a
little harmonic distortion on the peaks above -3 dBFS, which is usually inaudible on short peaks
and becomes a gentle saturation when pushed. To avoid it, lower the levels (e.g. Dexed's output
level, or the mix of the FX).

Implementation
--------------

The DSP is in `ambidex/FxChain.cpp`, the parameters (ranges, scales and defaults) in
`ambidex/FxParameters.cpp`, and the panel in `ambidex/FxPanel.cpp`. The compressor, chorus,
phaser and reverb are JUCE's (`juce::dsp` and `juce::Reverb`); the distortion, the delay and the
filter (Andrew Simper's linear trapezoidal state variable filter) are written for Ambidex. The
parameters are updated every 32 samples.

The FX values are saved in the presets (see `Presets.md`).
