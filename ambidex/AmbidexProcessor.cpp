// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "AmbidexProcessor.h"
#include "AmbidexEditor.h"

namespace {
// Default presets, from Dexed's startup cartridge (Dexed_01.syx)
constexpr int defaultProgramA = 1; // "2. LAURIE"
constexpr int defaultProgramB = 3; // "4. PHAROH"

// Morph changes are ramped over this time, to avoid zipper noise
constexpr double morphSmoothingSeconds = 0.05;

const Identifier stateId("ambidex");
const Identifier engineId("engine");
const Identifier dexedStateId("dexedState");
} // namespace

//==============================================================================
AmbidexAudioProcessor::AmbidexAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("output", AudioChannelSet::stereo(), true)) {
    // (Ambidex starts fully on A, with preset A editable: the morph isn't saved in the state)
    addParameter(morph = new AudioParameterFloat(ParameterID("morph", 1), "Morph", 0.0f, 1.0f, 0.0f));

    for (auto& engine : engines)
        engine = std::make_unique<DexedAudioProcessor>();

    engines[0]->setCurrentProgram(defaultProgramA);
    engines[1]->setCurrentProgram(defaultProgramB);
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

    loadMeasurer.reset(sampleRate, samplesPerBlock);
}

void AmbidexAudioProcessor::releaseResources() {
    for (auto& engine : engines)
        engine->releaseResources();
}

bool AmbidexAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
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
}

//==============================================================================
AudioProcessorEditor* AmbidexAudioProcessor::createEditor() {
    return new AmbidexAudioProcessorEditor(*this);
}

//==============================================================================
void AmbidexAudioProcessor::getStateInformation(MemoryBlock& destData) {
    ValueTree state(stateId);

    for (auto& engine : engines) {
        MemoryBlock engineState;
        engine->getStateInformation(engineState);

        ValueTree engineTree(engineId);
        engineTree.setProperty(dexedStateId, engineState.toBase64Encoding(), nullptr);
        state.appendChild(engineTree, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AmbidexAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(stateId.toString()))
        return;

    auto state = ValueTree::fromXml(*xml);

    for (int i = 0; i < numEngines && i < state.getNumChildren(); ++i) {
        MemoryBlock engineState;
        if (engineState.fromBase64Encoding(state.getChild(i)[dexedStateId].toString()))
            engines[i]->setStateInformation(engineState.getData(), (int) engineState.getSize());
    }
}

//==============================================================================
// This creates the app's processor (Dexed's own factory is renamed, see CMakeLists.txt)
AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AmbidexAudioProcessor();
}
