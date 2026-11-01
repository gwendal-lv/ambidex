// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "AmbidexProcessor.h"
#include "AmbidexEditor.h"

namespace {
// Default presets (factory presets, see PresetLibrary.cpp)
const String defaultPresetNames[] = { "Laurie", "Pharoh" };
// If they were deleted: voices from Dexed's startup cartridge (Dexed_01.syx)
constexpr int defaultPrograms[] = { 1, 3 }; // "2. LAURIE", "4. PHAROH"

// Morph changes are ramped over this time, to avoid zipper noise
constexpr double morphSmoothingSeconds = 0.05;

const Identifier stateId("ambidex");
const Identifier slotId("slot");
const Identifier presetNameId("presetName");
const Identifier modifiedId("modified");
} // namespace

//==============================================================================
AmbidexAudioProcessor::AmbidexAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("output", AudioChannelSet::stereo(), true)) {
    // (Ambidex starts fully on A, with preset A editable: the morph isn't saved in the state)
    addParameter(morph = new AudioParameterFloat(ParameterID("morph", 1), "Morph", 0.0f, 1.0f, 0.0f));

    for (auto& engine : engines)
        engine = std::make_unique<DexedAudioProcessor>();

    // (Dexed's data folder is known once an engine exists)
    presetLibrary = std::make_unique<PresetLibrary>(DexedAudioProcessor::dexedAppDir.getChildFile("Presets"));

    for (int slot = 0; slot < numEngines; ++slot) {
        AmbidexPreset preset;
        if (presetLibrary->load(defaultPresetNames[slot], preset)) {
            loadPreset(slot, preset);
        } else {
            engines[slot]->setCurrentProgram(defaultPrograms[slot]);
            for (int param = 0; param < fx::numParams; ++param)
                fxValues[slot][param] = fx::getParamInfo(param).defaultValue;
            presetNames[slot] = {};
            baselines[slot].clear();
        }
    }
}

AmbidexAudioProcessor::~AmbidexAudioProcessor() = default;

//==============================================================================
void AmbidexAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    for (auto& engine : engines) {
        engine->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
        engine->prepareToPlay(sampleRate, samplesPerBlock);
    }

    engineBBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    engineBMidi.ensureSize(4096);

    smoothedMorph.reset(sampleRate, morphSmoothingSeconds);
    smoothedMorph.setCurrentAndTargetValue(morph->get());

    fxChain.prepare(sampleRate);

    loadMeasurer.reset(sampleRate, samplesPerBlock);
}

void AmbidexAudioProcessor::releaseResources() {
    for (auto& engine : engines)
        engine->releaseResources();
}

bool AmbidexAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // (the FX chain is stereo)
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

std::pair<float, float> AmbidexAudioProcessor::getMorphGains(float morph) {
    // Both engines play different patches, i.e. uncorrelated signals: a sin/cos
    // law keeps the total power (perceived loudness) constant across the morph.
    const float angle = morph * MathConstants<float>::halfPi;
    return { std::cos(angle), std::sin(angle) };
}

void AmbidexAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    AudioProcessLoadMeasurer::ScopedTimer loadTimer(loadMeasurer, numSamples);

    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // Both engines get the same MIDI. Dexed replaces the MIDI buffer content
    // with its own output (sysex), which Ambidex doesn't forward.
    engineBMidi.clear();
    engineBMidi.addEvents(midiMessages, 0, numSamples, 0);

    // (doesn't reallocate, unless the host sends a bigger block than announced)
    engineBBuffer.setSize(numChannels, numSamples, false, false, true);

    engines[0]->processBlock(buffer, midiMessages);
    engines[1]->processBlock(engineBBuffer, engineBMidi);
    midiMessages.clear();

    smoothedMorph.setTargetValue(morph->get());

    for (int i = 0; i < numSamples; ++i) {
        const auto [gainA, gainB] = getMorphGains(smoothedMorph.getNextValue());

        for (int ch = 0; ch < numChannels; ++ch) {
            float* out = buffer.getWritePointer(ch);
            out[i] = gainA * out[i] + gainB * engineBBuffer.getSample(ch, i);
        }
    }

    FxChain::Values fxTargets;
    for (int param = 0; param < fx::numParams; ++param)
        fxTargets[(size_t) param] = getMorphedFxValueNormalised(param);
    // (the mixes fade out, like a module whose mix is turned down)
    if (fxBypassed)
        for (int module = 0; module < fx::numModules; ++module)
            fxTargets[(size_t) fx::getModuleInfo(module).mixParam] = 0.0f;
    fxChain.process(buffer, fxTargets);
}

float AmbidexAudioProcessor::getMorphedFxValueNormalised(int param) const {
    const float a = fx::toNormalised(param, fxValues[0][param]);
    const float b = fx::toNormalised(param, fxValues[1][param]);

    // A module that one preset doesn't use (mix at 0) keeps the settings of the
    // other preset: only its mix is morphed
    const int module = fx::getModuleOf(param);
    const int mixParam = module >= 0 ? fx::getModuleInfo(module).mixParam : -1;
    if (module >= 0 && param != mixParam) {
        const bool usedByA = fxValues[0][mixParam] > 0.0f;
        const bool usedByB = fxValues[1][mixParam] > 0.0f;
        if (usedByA != usedByB)
            return usedByA ? a : b;
    }

    // Interpolated on the knob's (perceptual) scale
    return a + (b - a) * morph->get();
}

//==============================================================================
AmbidexPreset AmbidexAudioProcessor::capturePreset(int slot) const {
    AmbidexPreset preset;
    preset.name = presetNames[slot];
    const uint8_t* voice = engines[slot]->activeProgram.getRawData();
    std::copy_n(voice, preset.voice.size(), preset.voice.begin());
    engines[slot]->getStateInformation(preset.engineState);
    for (int param = 0; param < fx::numParams; ++param)
        preset.fxValues[(size_t) param] = getFxValue(slot, param);
    return preset;
}

void AmbidexAudioProcessor::loadPreset(int slot, const AmbidexPreset& preset) {
    auto& engine = *engines[slot];
    if (preset.engineState.isEmpty()) {
        for (auto* param : engine.getParameters())
            param->setValueNotifyingHost(param->getDefaultValue());
    } else {
        engine.setStateInformation(preset.engineState.getData(), (int) preset.engineState.getSize());
    }
    // (after the state: Dexed's state doesn't restore the voice name, nor the
    // levels of the pitch envelope)
    engine.applyProgram(Program(preset.voice.data()));

    for (int param = 0; param < fx::numParams; ++param)
        setFxValue(slot, param, preset.fxValues[(size_t) param]);

    presetNames[slot] = preset.name;
    baselines[slot] = getSignature(slot);
}

void AmbidexAudioProcessor::loadFmPatch(int slot, const uint8_t* voice) {
    engines[slot]->applyProgram(Program(voice));
}

void AmbidexAudioProcessor::setPresetSaved(int slot, const String& name) {
    presetNames[slot] = name;
    baselines[slot] = getSignature(slot);
}

void AmbidexAudioProcessor::setPresetName(int slot, const String& name) {
    presetNames[slot] = name;
}

void AmbidexAudioProcessor::setPresetUnsaved(int slot) {
    baselines[slot].clear();
}

bool AmbidexAudioProcessor::isPresetModified(int slot) const {
    return baselines[slot].empty() || getSignature(slot) != baselines[slot];
}

std::vector<float> AmbidexAudioProcessor::getSignature(int slot) const {
    // Everything that a preset holds: Dexed's parameters (voice and settings),
    // the raw voice (for its name, and what isn't mapped to parameters), and the FX
    std::vector<float> signature;
    for (auto* param : engines[slot]->getParameters())
        signature.push_back(param->getValue());
    const uint8_t* voice = engines[slot]->activeProgram.getRawData();
    signature.insert(signature.end(), voice, voice + AmbidexPreset::voiceSize);
    for (int param = 0; param < fx::numParams; ++param)
        signature.push_back(getFxValue(slot, param));
    return signature;
}

//==============================================================================
AudioProcessorEditor* AmbidexAudioProcessor::createEditor() {
    return new AmbidexAudioProcessorEditor(*this);
}

//==============================================================================
void AmbidexAudioProcessor::getStateInformation(MemoryBlock& destData) {
    // The presets of both slots, as they are (with their unsaved edits)
    ValueTree state(stateId);

    for (int slot = 0; slot < numEngines; ++slot) {
        ValueTree slotTree(slotId);
        slotTree.setProperty(presetNameId, presetNames[slot], nullptr);
        slotTree.setProperty(modifiedId, isPresetModified(slot), nullptr);
        slotTree.appendChild(capturePreset(slot).toValueTree(), nullptr);
        state.appendChild(slotTree, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AmbidexAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(stateId.toString()))
        return;

    auto state = ValueTree::fromXml(*xml);

    for (int slot = 0; slot < numEngines && slot < state.getNumChildren(); ++slot) {
        const auto slotTree = state.getChild(slot);
        AmbidexPreset preset;
        if (!slotTree.hasType(slotId) || !preset.fromValueTree(slotTree.getChild(0)))
            continue;

        preset.name = slotTree[presetNameId].toString();
        loadPreset(slot, preset);
        if ((bool) slotTree[modifiedId])
            setPresetUnsaved(slot);
    }
}

//==============================================================================
// This creates the app's processor (Dexed's own factory is renamed, see CMakeLists.txt)
AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AmbidexAudioProcessor();
}
