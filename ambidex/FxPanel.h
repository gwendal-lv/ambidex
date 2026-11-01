// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "AmbidexProcessor.h"

/** One FX of the chain: its name and a row of Dexed knobs. */
class FxModule : public Component {
public:
    explicit FxModule(int module);

    int getNumKnobs() const { return knobs.size(); }
    Slider& getKnob(int index) { return *knobs[index]; }
    int getParam(int index) const { return info.firstParam + index; }

    /** Dims the module (except its mix knob) while the mix is at 0. */
    void setBypassedLook(bool shouldLookBypassed);

    void paint(Graphics& g) override;
    void resized() override;

private:
    const fx::ModuleInfo& info;
    OwnedArray<Slider> knobs;
    bool bypassedLook = false;
};

//==============================================================================
/**
 * The FX chain, shown on the right of the Dexed editor. Its knobs edit the FX
 * values of the engine being edited. While morphing, they show (greyed) the
 * values heard, which move with the morph slider. The switch in the header
 * bypasses the whole chain.
 */
class FxPanel : public Component {
public:
    static constexpr int width = 290;

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
    Rectangle<int> getModulesArea() const;

    AmbidexAudioProcessor& ambidex;
    int editableEngine = -1;
    TextButton chainSwitch;
    OwnedArray<FxModule> modules;
};
