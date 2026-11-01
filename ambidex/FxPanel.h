// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "AmbidexProcessor.h"

class FxPlot;

/** Live levels of the FX chain, as shown (smoothed for the eye). */
struct FxLiveData {
    float distortionInput = 0.0f;          // linear
    float compressorInput = 0.0f;          // linear
    float compressorGainReduction = 0.0f;  // dB, positive
    float output = 0.0f;                   // linear
    double modulationPhase = 0.0;          // LFO phase, in cycles (moves while sound plays)
};

//==============================================================================
/** One FX of the chain: its name, a row of Dexed knobs, and a plot. */
class FxModule : public Component {
public:
    FxModule(int module, const FxLiveData& liveData);
    ~FxModule() override;

    int getNumKnobs() const { return knobs.size(); }
    Slider& getKnob(int index) { return *knobs[index]; }
    int getParam(int index) const { return info.firstParam + index; }
    /** Value of a parameter of this module, as shown by its knob. */
    float getValue(int param) const;

    /** Dims the module (except its mix knob) while the mix is at 0. */
    void setBypassedLook(bool shouldLookBypassed);
    bool hasBypassedLook() const { return bypassedLook; }

    /** After the knobs' values changed: updates the plot. */
    void valuesChanged();
    void repaintPlot();

    void paint(Graphics& g) override;
    void resized() override;

private:
    const fx::ModuleInfo& info;
    OwnedArray<Slider> knobs;
    std::unique_ptr<FxPlot> plot;
    bool bypassedLook = false;
};

//==============================================================================
/** Peak level of the output, after the soft clipper. */
class OutputMeter : public Component {
public:
    explicit OutputMeter(const FxLiveData& liveData) : live(liveData) {}
    void paint(Graphics& g) override;

private:
    const FxLiveData& live;
};

//==============================================================================
/**
 * The FX chain, shown on the right of the Dexed editor. Its knobs edit the FX
 * values of the engine being edited. While morphing, they show (greyed) the
 * values heard, which move with the morph slider. The header holds the switch
 * that bypasses the whole chain, the output meter, and the output gain.
 */
class FxPanel : public Component, private Timer {
public:
    static constexpr int width = 435;

    explicit FxPanel(AmbidexAudioProcessor& processor);

    /** Shows the FX values of an engine that can be edited, or of the morph (-1). */
    void showValues(int editableEngine);

    void paint(Graphics& g) override;
    void paintOverChildren(Graphics& g) override;
    void resized() override;

private:
    void knobChanged(FxModule& module, int knobIndex);
    void updateBypassedLook(FxModule& module);
    void updateChainSwitch();
    // Reads the meters, and moves the modulation's LFO while sound plays
    void timerCallback() override;
    Rectangle<int> getModulesArea() const;
    Rectangle<int> getHeaderArea() const;
    // The output gain knob and its label
    Rectangle<int> getOutputArea() const;

    AmbidexAudioProcessor& ambidex;
    int editableEngine = -1;
    FxLiveData liveData;
    double lastTimerMs = 0.0, lastModulationSoundMs = -1.0e9;

    TextButton chainSwitch;
    OutputMeter outputMeter { liveData };
    std::unique_ptr<Slider> outputKnob;
    OwnedArray<FxModule> modules;
};
