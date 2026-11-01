// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "FxChain.h"

using namespace fx;

namespace {
// Glide time of the parameters, and of the delay time
constexpr double smoothingSeconds = 0.05;
constexpr float maxDelayMs = 2000.0f;

// Modulation: the first half of the "character" knob shortens the delay from
// chorus to flanger, the second half crossfades the flanger to the phaser
constexpr float chorusDelayMs = 20.0f;
constexpr float flangerDelayMs = 1.5f;
constexpr float phaserCentreHz = 1000.0f;

// The output soft clipper is transparent up to this level (about -3 dBFS)
constexpr float softClipThreshold = 0.7f;

// Reverb's own wet and dry gains (see juce::Reverb: wet is scaled by 3, dry by 2)
constexpr float reverbWetLevel = 0.25f;
constexpr float reverbDryLevel = 0.5f;

// Gain of a one-pole low-pass filter (topology-preserving transform), for tan(pi * fc / fs)
float onePoleGain(float cutoffHz, double sampleRate) {
    const float g = std::tan(MathConstants<float>::pi * jmin(cutoffHz, 0.45f * (float) sampleRate) / (float) sampleRate);
    return g / (1.0f + g);
}

float onePoleLowPass(float input, float gain, float& state) {
    const float v = (input - state) * gain;
    const float output = v + state;
    state = output + v;
    return output;
}

// Identity up to the threshold, then a tanh curve towards 1 (same value and
// slope at the threshold, so it doesn't add a kink)
float softClip(float x) {
    const float magnitude = std::abs(x);
    if (magnitude <= softClipThreshold)
        return x;
    constexpr float headroom = 1.0f - softClipThreshold;
    return std::copysign(softClipThreshold + headroom * std::tanh((magnitude - softClipThreshold) / headroom), x);
}

// Soft (0) -> hard (0.5) -> fold (1) waveshaper
float shape(float x, float shapeValue) {
    const float hard = jlimit(-1.0f, 1.0f, x);
    if (shapeValue <= 0.5f) {
        const float soft = std::tanh(x);
        return soft + (hard - soft) * (shapeValue * 2.0f);
    }
    const float fold = std::sin(x * MathConstants<float>::halfPi);
    return hard + (fold - hard) * (shapeValue * 2.0f - 1.0f);
}
} // namespace

//==============================================================================
void FxChain::prepare(double newSampleRate) {
    sampleRate = newSampleRate;

    for (auto& value : smoothedValues)
        value.reset(sampleRate, smoothingSeconds);
    valuesInitialised = false;

    const dsp::ProcessSpec spec { sampleRate, (uint32) subBlockSize, (uint32) numChannels };
    compressorDsp.prepare(spec);
    chorus.prepare(spec);
    chorus.setMix(1.0f);
    phaser.prepare(spec);
    phaser.setMix(1.0f);
    phaser.setCentreFrequency(phaserCentreHz);

    wetBuffer.setSize(numChannels, subBlockSize);
    phaserBuffer.setSize(numChannels, subBlockSize);

    for (auto& line : delayLines)
        line.assign((size_t) std::ceil(maxDelayMs * 0.001 * sampleRate) + 2, 0.0f);
    delayGlide = 1.0f - (float) std::exp(-1.0 / (smoothingSeconds * sampleRate));

    reverbDsp.setSampleRate(sampleRate);

    reset();
}

void FxChain::reset() {
    for (int module = 0; module < numModules; ++module) {
        resetModule(module);
        active[(size_t) module] = false;
    }
}

void FxChain::resetModule(int module) {
    previousMixes[(size_t) module] = {};

    switch (module) {
        case distortion:
            std::fill(std::begin(distortionToneState), std::end(distortionToneState), 0.0f);
            break;
        case filter:
            std::fill(std::begin(filterState1), std::end(filterState1), 0.0f);
            std::fill(std::begin(filterState2), std::end(filterState2), 0.0f);
            break;
        case compressor:
            compressorDsp.reset();
            break;
        case modulation:
            chorus.reset();
            phaser.reset();
            chorusActive = phaserActive = false;
            break;
        case delay:
            for (auto& line : delayLines)
                std::fill(line.begin(), line.end(), 0.0f);
            std::fill(std::begin(delayToneState), std::end(delayToneState), 0.0f);
            delaySamples = get(delayTime) * 0.001f * (float) sampleRate;
            break;
        case reverb:
            reverbDsp.reset();
            break;
        default:
            break;
    }
}

//==============================================================================
void FxChain::process(AudioBuffer<float>& buffer, const Values& normalisedValues) {
    jassert(buffer.getNumChannels() == numChannels);

    for (int param = 0; param < numParams; ++param) {
        if (valuesInitialised)
            smoothedValues[(size_t) param].setTargetValue(normalisedValues[(size_t) param]);
        else
            smoothedValues[(size_t) param].setCurrentAndTargetValue(normalisedValues[(size_t) param]);
    }
    valuesInitialised = true;

    const int numSamples = buffer.getNumSamples();
    for (int start = 0; start < numSamples; start += subBlockSize) {
        const int subBlockSamples = jmin(subBlockSize, numSamples - start);

        for (int param = 0; param < numParams; ++param)
            values[(size_t) param] = fromNormalised(param, smoothedValues[(size_t) param].skip(subBlockSamples));

        float* channels[numChannels] = { buffer.getWritePointer(0, start), buffer.getWritePointer(1, start) };
        processSubBlock(channels, subBlockSamples);
    }

    // Output safety: rounds off the peaks that would clip (no latency, unlike a limiter)
    for (int ch = 0; ch < numChannels; ++ch) {
        float* samples = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            samples[i] = softClip(samples[i]);
    }
}

void FxChain::processSubBlock(float* const* channels, int numSamples) {
    for (int module = 0; module < numModules; ++module) {
        const float mixValue = get(getModuleInfo(module).mixParam);

        // Bypassed, once the wet signal has been faded out
        if (mixValue <= 0.0f && previousMixes[(size_t) module].wet <= 0.0f) {
            active[(size_t) module] = false;
            continue;
        }
        if (!active[(size_t) module]) {
            resetModule(module);
            active[(size_t) module] = true;
        }

        const Mix mix = getMix(module, mixValue);

        switch (module) {
            case distortion: processDistortion(channels, numSamples); break;
            case filter: processFilter(channels, numSamples); break;
            case compressor: processCompressor(channels, numSamples); break;
            case modulation: processModulation(channels, numSamples); break;
            case delay: processDelay(channels, numSamples); break;
            case reverb: processReverb(channels, numSamples, mix); break;
            default: break;
        }

        if (module == reverb)
            previousMixes[(size_t) module] = mix; // (the reverb ramps its own gains)
        else
            mixWet(channels, numSamples, module, mix);
    }
}

FxChain::Mix FxChain::getMix(int module, float mixValue) {
    switch (module) {
        case modulation: {
            // Partly correlated wet signal: equal-power crossfade
            const float angle = mixValue * MathConstants<float>::halfPi;
            return { std::cos(angle), std::sin(angle) };
        }
        case delay:
        case reverb:
            // Echoes and tails, added to the dry signal: the dry signal stays at
            // full level up to 50%, where the wet signal reaches full level
            return { jmin(1.0f, 2.0f * (1.0f - mixValue)), jmin(1.0f, 2.0f * mixValue) };
        default:
            // Wet signal correlated with the dry one: linear crossfade
            return { 1.0f - mixValue, mixValue };
    }
}

void FxChain::mixWet(float* const* channels, int numSamples, int module, Mix mix) {
    const Mix from = previousMixes[(size_t) module];
    const float dryStep = (mix.dry - from.dry) / (float) numSamples;
    const float wetStep = (mix.wet - from.wet) / (float) numSamples;

    for (int ch = 0; ch < numChannels; ++ch) {
        float* out = channels[ch];
        const float* wet = wetBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            const float t = (float) (i + 1);
            out[i] = out[i] * (from.dry + dryStep * t) + wet[i] * (from.wet + wetStep * t);
        }
    }

    previousMixes[(size_t) module] = mix;
}

//==============================================================================
void FxChain::processDistortion(const float* const* input, int numSamples) {
    const float driveGain = Decibels::decibelsToGain(get(distortionDrive));
    // (rough level compensation: louder than 1 / drive, as the peaks get clipped)
    const float outputGain = 1.0f / std::sqrt(driveGain);
    const float shapeValue = get(distortionShape);
    const float toneGain = onePoleGain(get(distortionTone), sampleRate);

    for (int ch = 0; ch < numChannels; ++ch) {
        float* wet = wetBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            const float shaped = shape(input[ch][i] * driveGain, shapeValue);
            wet[i] = onePoleLowPass(shaped, toneGain, distortionToneState[ch]) * outputGain;
        }
    }
}

void FxChain::processFilter(const float* const* input, int numSamples) {
    // State variable filter, see Andrew Simper's "Linear Trapezoidal
    // Integrated State Variable Filter" (Cytomic, 2013)
    const float cutoff = jmin(get(filterCutoff), 0.45f * (float) sampleRate);
    const float g = std::tan(MathConstants<float>::pi * cutoff / (float) sampleRate);
    const float k = 1.0f / get(filterResonance);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;

    // Low-pass (0) -> band-pass (0.5) -> high-pass (1)
    const float type = get(filterType);
    const float lowPassGain = jmax(0.0f, 1.0f - 2.0f * type);
    const float highPassGain = jmax(0.0f, 2.0f * type - 1.0f);
    const float bandPassGain = 1.0f - lowPassGain - highPassGain;

    for (int ch = 0; ch < numChannels; ++ch) {
        float* wet = wetBuffer.getWritePointer(ch);
        float& ic1 = filterState1[ch];
        float& ic2 = filterState2[ch];
        for (int i = 0; i < numSamples; ++i) {
            const float x = input[ch][i];
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            const float lowPass = v2;
            const float bandPass = k * v1; // (unity gain at the cutoff)
            const float highPass = x - k * v1 - v2;
            wet[i] = lowPassGain * lowPass + bandPassGain * bandPass + highPassGain * highPass;
        }
    }
}

void FxChain::processCompressor(const float* const* input, int numSamples) {
    const float threshold = get(compressorThreshold);
    const float ratio = get(compressorRatio);
    compressorDsp.setThreshold(threshold);
    compressorDsp.setRatio(ratio);
    compressorDsp.setAttack(get(compressorAttack));
    compressorDsp.setRelease(get(compressorRelease));

    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.copyFrom(ch, 0, input[ch], numSamples);

    dsp::AudioBlock<float> block(wetBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    compressorDsp.process(dsp::ProcessContextReplacing<float>(block));

    // Automatic make-up gain: half of the gain reduction of a 0 dB signal
    const float makeUpGain = Decibels::decibelsToGain(-threshold * (1.0f - 1.0f / ratio) * 0.5f);
    block.multiplyBy(makeUpGain);
}

void FxChain::processModulation(const float* const* input, int numSamples) {
    const float rate = get(modulationRate);
    const float depth = get(modulationDepth);
    const float feedback = get(modulationFeedback);
    const float character = get(modulationCharacter);

    const float phaserWeight = jmax(0.0f, 2.0f * character - 1.0f);
    const bool needsChorus = phaserWeight < 1.0f;
    const bool needsPhaser = phaserWeight > 0.0f;

    if (needsChorus) {
        if (!chorusActive)
            chorus.reset();
        chorusActive = true;

        const float centreDelay = chorusDelayMs * std::pow(flangerDelayMs / chorusDelayMs, jmin(1.0f, 2.0f * character));
        chorus.setRate(rate);
        // (JUCE's chorus modulates the delay by up to 20 ms: keep it in proportion to the centre delay)
        chorus.setDepth(depth * centreDelay / chorusDelayMs);
        chorus.setCentreDelay(centreDelay);
        chorus.setFeedback(feedback);

        for (int ch = 0; ch < numChannels; ++ch)
            wetBuffer.copyFrom(ch, 0, input[ch], numSamples);
        dsp::AudioBlock<float> block(wetBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        chorus.process(dsp::ProcessContextReplacing<float>(block));
    } else {
        chorusActive = false;
    }

    if (needsPhaser) {
        if (!phaserActive)
            phaser.reset();
        phaserActive = true;

        phaser.setRate(rate);
        phaser.setDepth(depth);
        phaser.setFeedback(feedback);

        for (int ch = 0; ch < numChannels; ++ch)
            phaserBuffer.copyFrom(ch, 0, input[ch], numSamples);
        dsp::AudioBlock<float> block(phaserBuffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        phaser.process(dsp::ProcessContextReplacing<float>(block));

        for (int ch = 0; ch < numChannels; ++ch) {
            if (needsChorus) {
                wetBuffer.applyGain(ch, 0, numSamples, 1.0f - phaserWeight);
                wetBuffer.addFrom(ch, 0, phaserBuffer, ch, 0, numSamples, phaserWeight);
            } else {
                wetBuffer.copyFrom(ch, 0, phaserBuffer, ch, 0, numSamples);
            }
        }
    } else {
        phaserActive = false;
    }
}

void FxChain::processDelay(const float* const* input, int numSamples) {
    const float targetDelaySamples = get(delayTime) * 0.001f * (float) sampleRate;
    const float feedback = get(delayFeedback);
    const float toneGain = onePoleGain(get(delayTone), sampleRate);
    // 0 = stereo delay, 1 = ping-pong (the mono input goes to the left line,
    // and each line feeds the other one)
    const float spread = get(delaySpread);

    const int size = (int) delayLines[0].size();
    float* lineL = delayLines[0].data();
    float* lineR = delayLines[1].data();
    float* wetL = wetBuffer.getWritePointer(0);
    float* wetR = wetBuffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i) {
        // (the delay time glides, like a tape delay)
        delaySamples += (targetDelaySamples - delaySamples) * delayGlide;

        float readPosition = (float) delayWritePosition - delaySamples;
        if (readPosition < 0.0f)
            readPosition += (float) size;
        const int index0 = (int) readPosition;
        const int index1 = index0 + 1 < size ? index0 + 1 : 0;
        const float fraction = readPosition - (float) index0;
        const float delayedL = lineL[index0] + (lineL[index1] - lineL[index0]) * fraction;
        const float delayedR = lineR[index0] + (lineR[index1] - lineR[index0]) * fraction;

        const float inL = input[0][i];
        const float inR = input[1][i];
        const float mono = 0.5f * (inL + inR);

        const float feedbackL = onePoleLowPass(delayedL + (delayedR - delayedL) * spread, toneGain, delayToneState[0]);
        const float feedbackR = onePoleLowPass(delayedR + (delayedL - delayedR) * spread, toneGain, delayToneState[1]);
        lineL[delayWritePosition] = inL + (mono - inL) * spread + feedback * feedbackL;
        lineR[delayWritePosition] = inR * (1.0f - spread) + feedback * feedbackR;

        wetL[i] = delayedL;
        wetR[i] = delayedR;

        if (++delayWritePosition == size)
            delayWritePosition = 0;
    }
}

void FxChain::processReverb(float* const* channels, int numSamples, Mix mix) {
    Reverb::Parameters parameters;
    parameters.roomSize = get(reverbSize);
    parameters.damping = get(reverbDamping);
    parameters.width = get(reverbWidth);
    parameters.wetLevel = mix.wet * reverbWetLevel;
    parameters.dryLevel = mix.dry * reverbDryLevel;
    parameters.freezeMode = 0.0f;
    reverbDsp.setParameters(parameters);

    reverbDsp.processStereo(channels[0], channels[1], numSamples);
}
