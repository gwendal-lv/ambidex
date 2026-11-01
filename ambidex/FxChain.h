// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "FxParameters.h"

/**
 * The global FX chain, applied to the mix of both engines: distortion, filter,
 * compressor, modulation, delay and reverb, in this fixed order (see
 * doc/Effects.md). A module whose mix is at 0 is bypassed, and uses no CPU.
 * The output then goes through a gain and a soft clipper, which keeps it below
 * 0 dBFS.
 */
class FxChain {
public:
    using Values = std::array<float, fx::numParams>;

    void prepare(double sampleRate);
    void reset();

    /**
     * Processes a stereo buffer in place. The FX parameters glide to the given
     * normalised (0..1) values, so that a morph or a knob move doesn't click.
     */
    void process(AudioBuffer<float>& buffer, const Values& normalisedValues);

private:
    // Parameters are updated (and mixes ramped) every subBlockSize samples
    static constexpr int subBlockSize = 32;
    static constexpr int numChannels = 2;

    struct Mix {
        float dry = 1.0f, wet = 0.0f;
    };

    // Dry and wet gains of a module, for its mix parameter (see doc/Effects.md)
    static Mix getMix(int module, float mixValue);
    void processSubBlock(float* const* channels, int numSamples);
    // (each returns the wet signal in the wet buffer)
    void processDistortion(const float* const* input, int numSamples);
    void processFilter(const float* const* input, int numSamples);
    void processCompressor(const float* const* input, int numSamples);
    void processModulation(const float* const* input, int numSamples);
    void processDelay(const float* const* input, int numSamples);
    void processReverb(float* const* channels, int numSamples, Mix mix);

    float get(int param) const { return values[(size_t) param]; }
    void resetModule(int module);
    // Mixes the wet buffer into the channels, ramping from the previous mix
    void mixWet(float* const* channels, int numSamples, int module, Mix mix);

    double sampleRate = 44100.0;

    std::array<SmoothedValue<float>, fx::numParams> smoothedValues;
    bool valuesInitialised = false;
    Values values {}; // current values (not normalised)

    std::array<bool, fx::numModules> active {};
    std::array<Mix, fx::numModules> previousMixes {};

    AudioBuffer<float> wetBuffer, phaserBuffer;

    // Distortion
    float distortionToneState[numChannels] {};

    // Filter (state variable filter, in topology-preserving transform form)
    float filterState1[numChannels] {}, filterState2[numChannels] {};

    dsp::Compressor<float> compressorDsp;

    dsp::Chorus<float> chorus;
    dsp::Phaser<float> phaser;
    bool chorusActive = false, phaserActive = false;

    // Delay
    std::vector<float> delayLines[numChannels];
    int delayWritePosition = 0;
    // When the delay time changes, the delay reads at the new time too, and
    // crossfades to it (a tape-like glide would bend the pitch)
    float delaySamples = 0.0f, nextDelaySamples = 0.0f;
    float delayCrossfade = 0.0f; // 0 = not crossfading
    float delayCrossfadeStep = 1.0f; // per sample
    float delayToneState[numChannels] {};

    Reverb reverbDsp;

    float previousOutputGain = 1.0f;
};
