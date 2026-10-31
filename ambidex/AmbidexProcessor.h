// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

/**
 * Runs two unmodified Dexed engines (A and B) on the same MIDI input, and
 * crossfades their outputs with the "morph" parameter: 0 = only A, 1 = only B.
 */
class AmbidexAudioProcessor : public AudioProcessor {
public:
    static constexpr int numEngines = 2;

    AmbidexAudioProcessor();
    ~AmbidexAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets are handled by each Dexed engine
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    //==============================================================================
    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    DexedAudioProcessor& getEngine(int index) { return *engines[index]; }

    /** Equal-power crossfade gains for a morph position in [0, 1]. */
    static std::pair<float, float> getMorphGains(float morph);

    // 0 = only engine A is heard, 1 = only engine B
    AudioParameterFloat* morph;

    // Engine shown in the editor (0 = A, 1 = B). Message thread only.
    int editedEngine = 0;

    // On-screen keyboard, played into both engines
    MidiKeyboardState keyboardState;

private:
    std::unique_ptr<DexedAudioProcessor> engines[numEngines];

    // Engine A renders in place into the output buffer, engine B into this one
    AudioBuffer<float> engineBBuffer;
    MidiBuffer engineBMidi;

    SmoothedValue<float> smoothedMorph;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbidexAudioProcessor)
};
