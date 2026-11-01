// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

/**
 * The parameters of the global FX chain (see doc/Effects.md). All of them are
 * continuous, so that they can be morphed: their normalised (0..1) values are
 * interpolated between preset A and preset B, and the normalisable ranges give
 * them perceptual scales (dB, log frequencies and times...).
 */
namespace fx {

enum Param {
    distortionDrive, distortionShape, distortionTone, distortionMix,
    filterCutoff, filterResonance, filterType, filterMix,
    compressorThreshold, compressorRatio, compressorAttack, compressorRelease, compressorMix,
    modulationRate, modulationDepth, modulationFeedback, modulationCharacter, modulationMix,
    delayTime, delayFeedback, delayTone, delaySpread, delayMix,
    reverbSize, reverbDamping, reverbWidth, reverbMix,
    numParams
};

struct ParamInfo {
    Identifier id; // in the saved state
    String label;  // under the knob
    NormalisableRange<float> range;
    float defaultValue;
    std::function<String(float)> toText;
};

const ParamInfo& getParamInfo(int param);

/** Converts between a parameter's value and its normalised (0..1) value. */
inline float toNormalised(int param, float value) { return getParamInfo(param).range.convertTo0to1(value); }
inline float fromNormalised(int param, float normalised) { return getParamInfo(param).range.convertFrom0to1(normalised); }

enum Module { distortion, filter, compressor, modulation, delay, reverb, numModules };

struct ModuleInfo {
    String name;
    int firstParam;
    int numParams;
    int mixParam; // 0 = bypassed
};

const ModuleInfo& getModuleInfo(int module);
/** The module a parameter belongs to. */
int getModuleOf(int param);

} // namespace fx
