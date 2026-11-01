// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "FxParameters.h"

namespace fx {
namespace {

NormalisableRange<float> linearRange(float min, float max) {
    return { min, max };
}

// Equal ratios for equal knob moves (frequencies, times, ratios...)
NormalisableRange<float> logRange(float min, float max) {
    return { min, max,
             [](float start, float end, float normalised) { return start * std::pow(end / start, normalised); },
             [](float start, float end, float value) { return std::log(value / start) / std::log(end / start); } };
}

String percentText(float value) {
    return String(roundToInt(value * 100.0f)) + "%";
}

String decibelText(float value) {
    return String(value, 1) + " dB";
}

String frequencyText(float hz) {
    return hz < 1000.0f ? String(roundToInt(hz)) + " Hz" : String(hz / 1000.0f, 2) + " kHz";
}

String timeText(float ms) {
    if (ms < 10.0f)
        return String(ms, 1) + " ms";
    return ms < 1000.0f ? String(roundToInt(ms)) + " ms" : String(ms / 1000.0f, 2) + " s";
}

// For the knobs that morph between 3 characters: 0 = first, 0.5 = second, 1 = third
std::function<String(float)> blendText(StringArray names) {
    return [names](float value) {
        const float position = jlimit(0.0f, 2.0f, value * 2.0f);
        const int first = jmin(1, (int) position);
        const int secondPercent = roundToInt((position - (float) first) * 100.0f);
        if (secondPercent == 0)
            return names[first];
        if (secondPercent == 100)
            return names[first + 1];
        return String(100 - secondPercent) + "% " + names[first] + " / " + String(secondPercent) + "% " + names[first + 1];
    };
}

std::vector<ParamInfo> createParamInfos() {
    std::vector<ParamInfo> infos(numParams);
    auto set = [&infos](Param param, const char* id, const char* label, NormalisableRange<float> range,
                        float defaultValue, std::function<String(float)> toText) {
        infos[param] = { Identifier(id), label, range, defaultValue, std::move(toText) };
    };

    set(distortionDrive, "distortionDrive", "Drive", linearRange(0.0f, 36.0f), 12.0f, decibelText);
    set(distortionShape, "distortionShape", "Shape", linearRange(0.0f, 1.0f), 0.0f, blendText({ "Soft", "Hard", "Fold" }));
    set(distortionTone, "distortionTone", "Tone", logRange(1000.0f, 20000.0f), 8000.0f, frequencyText);
    set(distortionMix, "distortionMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(filterCutoff, "filterCutoff", "Cutoff", logRange(20.0f, 20000.0f), 2000.0f, frequencyText);
    set(filterResonance, "filterResonance", "Reso", logRange(0.5f, 12.0f), 0.707f,
        [](float q) { return "Q " + String(q, 2); });
    set(filterType, "filterType", "Type", linearRange(0.0f, 1.0f), 0.0f, blendText({ "Low-pass", "Band-pass", "High-pass" }));
    set(filterMix, "filterMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(compressorThreshold, "compressorThreshold", "Thresh", linearRange(-48.0f, 0.0f), -18.0f, decibelText);
    set(compressorRatio, "compressorRatio", "Ratio", logRange(1.0f, 20.0f), 4.0f,
        [](float ratio) { return String(ratio, 1) + ":1"; });
    set(compressorAttack, "compressorAttack", "Attack", logRange(0.1f, 100.0f), 10.0f, timeText);
    set(compressorRelease, "compressorRelease", "Release", logRange(10.0f, 1000.0f), 150.0f, timeText);
    set(compressorMix, "compressorMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(modulationRate, "modulationRate", "Rate", logRange(0.05f, 10.0f), 0.5f,
        [](float hz) { return String(hz, hz < 1.0f ? 2 : 1) + " Hz"; });
    set(modulationDepth, "modulationDepth", "Depth", linearRange(0.0f, 1.0f), 0.5f, percentText);
    set(modulationFeedback, "modulationFeedback", "Feedback", linearRange(-0.9f, 0.9f), 0.0f, percentText);
    set(modulationCharacter, "modulationCharacter", "Character", linearRange(0.0f, 1.0f), 0.0f,
        blendText({ "Chorus", "Flanger", "Phaser" }));
    set(modulationMix, "modulationMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(delayTime, "delayTime", "Time", logRange(10.0f, 2000.0f), 350.0f, timeText);
    set(delayFeedback, "delayFeedback", "Feedback", linearRange(0.0f, 0.95f), 0.35f, percentText);
    set(delayTone, "delayTone", "Tone", logRange(500.0f, 20000.0f), 6000.0f, frequencyText);
    set(delaySpread, "delaySpread", "Spread", linearRange(0.0f, 1.0f), 0.0f, percentText);
    set(delayMix, "delayMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(reverbSize, "reverbSize", "Size", linearRange(0.0f, 1.0f), 0.6f, percentText);
    set(reverbDamping, "reverbDamping", "Damping", linearRange(0.0f, 1.0f), 0.5f, percentText);
    set(reverbWidth, "reverbWidth", "Width", linearRange(0.0f, 1.0f), 1.0f, percentText);
    set(reverbMix, "reverbMix", "Mix", linearRange(0.0f, 1.0f), 0.0f, percentText);

    set(outputGain, "outputGain", "Output", linearRange(-24.0f, 12.0f), 0.0f, decibelText);

    return infos;
}

} // namespace

const ParamInfo& getParamInfo(int param) {
    static const std::vector<ParamInfo> infos = createParamInfos();
    return infos[(size_t) param];
}

const ModuleInfo& getModuleInfo(int module) {
    static const ModuleInfo infos[numModules] = {
        { "DISTORTION", distortionDrive, 4, distortionMix },
        { "FILTER", filterCutoff, 4, filterMix },
        { "COMPRESSOR", compressorThreshold, 5, compressorMix },
        { "MODULATION", modulationRate, 5, modulationMix },
        { "DELAY", delayTime, 5, delayMix },
        { "REVERB", reverbSize, 4, reverbMix },
    };
    return infos[module];
}

int getModuleOf(int param) {
    if (param == outputGain)
        return -1;
    for (int module = numModules - 1; module > 0; --module)
        if (param >= getModuleInfo(module).firstParam)
            return module;
    return 0;
}

} // namespace fx
