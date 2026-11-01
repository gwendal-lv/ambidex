// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "FxChain.h"
#include "OscServer.h"
#include "PresetLibrary.h"
#include "PluginProcessor.h"

/**
 * Runs two unmodified Dexed engines (A and B) on the same MIDI input, and
 * crossfades their outputs with the "morph" parameter: 0 = only A, 1 = only B.
 * The mix then goes through the FX chain, whose parameters are also morphed
 * between their A and B values.
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

    /** FX parameter values of an engine (thread safe). */
    float getFxValue(int engine, int param) const { return fxValues[engine][param]; }
    void setFxValue(int engine, int param, float value) {
        const auto& range = fx::getParamInfo(param).range;
        fxValues[engine][param] = jlimit(range.start, range.end, value);
    }
    /** FX parameter value heard at the current morph position, normalised (0..1). */
    float getMorphedFxValueNormalised(int param) const;

    /** Live levels of the FX chain, for the GUI. */
    FxChain::Meters& getFxMeters() { return fxChain.meters; }

    // Bypasses the whole FX chain, to listen to the presets dry (not saved)
    std::atomic<bool> fxBypassed { false };

    //==============================================================================
    // Preset slots (A and B), message thread only. Each slot holds a preset from
    // the library, and its unsaved edits.
    PresetLibrary& getPresetLibrary() { return *presetLibrary; }

    /** The slot's preset, as it is now (with its edits). */
    AmbidexPreset capturePreset(int slot) const;
    void loadPreset(int slot, const AmbidexPreset& preset);
    /** Replaces the DX7 voice of the slot's preset (155 bytes, unpacked). */
    void loadFmPatch(int slot, const uint8_t* voice);

    /** Name of the library preset the slot comes from (may have been deleted). */
    const String& getPresetName(int slot) const { return presetNames[slot]; }
    void setPresetName(int slot, const String& name);
    bool isPresetModified(int slot) const;
    /** After the slot's preset has been saved to the library, under this name. */
    void setPresetSaved(int slot, const String& name);
    /** Marks the slot as modified, until it's saved or loaded. */
    void setPresetUnsaved(int slot);

    /** Remote control over OSC (standalone app only, otherwise nullptr). */
    OscServer* getOscServer() { return oscServer.get(); }

    /** Equal-power crossfade gains for a morph position in [0, 1]. */
    static std::pair<float, float> getMorphGains(float morph);

    // 0 = only engine A is heard, 1 = only engine B
    AudioParameterFloat* morph;

    // Engine shown in the editor (0 = A, 1 = B). Message thread only.
    int editedEngine = 0;

    // On-screen keyboard, played into both engines
    MidiKeyboardState keyboardState;

    // Time spent in processBlock, relative to the duration of the block (thread safe)
    AudioProcessLoadMeasurer loadMeasurer;

private:
    std::unique_ptr<DexedAudioProcessor> engines[numEngines];

    // Engine A renders in place into the output buffer, engine B into this one
    AudioBuffer<float> engineBBuffer;
    MidiBuffer engineBMidi;

    SmoothedValue<float> smoothedMorph;

    std::atomic<float> fxValues[numEngines][fx::numParams];
    FxChain fxChain;

    std::unique_ptr<PresetLibrary> presetLibrary;
    String presetNames[numEngines];
    // Signature of each slot when its preset was loaded or saved (empty = modified)
    std::vector<float> baselines[numEngines];
    std::vector<float> getSignature(int slot) const;

    // (last: deleted first, as it uses the rest)
    std::unique_ptr<OscServer> oscServer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbidexAudioProcessor)
};
