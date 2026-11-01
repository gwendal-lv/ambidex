// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "AmbidexProcessor.h"
#include "ui/DXLookNFeel.h"

/** Dexed's look, plus a visible toggle state on buttons and a drawn track on linear sliders. */
class AmbidexLookAndFeel : public DXLookNFeel {
public:
    static const Colour ledColour;

    void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;
    int getSliderThumbRadius(Slider& slider) override;
    void drawLinearSlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, const Slider::SliderStyle style,
                          Slider& slider) override;
};

//==============================================================================
/** "CPU", a small bar and a percentage. */
class CpuMeter : public Component {
public:
    void setLoad(double proportion);
    void paint(Graphics& g) override;

private:
    int percent = 0;
};

//==============================================================================
/** Dims the Dexed editor and blocks its mouse input, while it can't be edited. */
class DisabledOverlay : public Component {
public:
    DisabledOverlay();
    void setEngineName(const String& engineName);
    void paint(Graphics& g) override;

private:
    String message;
};

//==============================================================================
/**
 * Top bar with the A/B buttons and the morph slider, the original Dexed editor
 * of the engine being edited, and a keyboard playing both engines.
 */
class AmbidexAudioProcessorEditor : public AudioProcessorEditor,
                                    private ComponentListener,
                                    private Timer {
public:
    explicit AmbidexAudioProcessorEditor(AmbidexAudioProcessor& processor);
    ~AmbidexAudioProcessorEditor() override;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent& event) override;

private:
    // Moves the morph slider fully to the engine's side (which shows it, editable)
    void activateEngine(int index);
    // Shows the engine's Dexed editor (editable or not)
    void showEngine(int index);
    void morphChanged();
    // An engine can only be edited while it's the only one heard
    bool isEditable() const;
    void updateEditability();
    void updateSize();
    void updateMixShares();
    int getDexedAreaHeight() const;
    // Rounded panel holding the A/B buttons, program names and morph slider
    Rectangle<int> getMorphPanelBounds() const;
    // Button, program name and mix share of an engine (framed when edited)
    Rectangle<int> getEngineArea(int index) const;

    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;
    void timerCallback() override;

    static constexpr int numEngines = AmbidexAudioProcessor::numEngines;

    AmbidexAudioProcessor& ambidex;

    AmbidexLookAndFeel lookAndFeel;
    Label title;
    CpuMeter cpuMeter;
    TextButton engineButtons[numEngines];
    Label programNames[numEngines];
    // Power of each engine in the mix, in percent
    Label mixShares[numEngines];
    Slider morphSlider;
    SliderParameterAttachment morphAttachment;

    // Shows the top part of the Dexed editors, hiding their own keyboard
    Component dexedArea;
    std::unique_ptr<AudioProcessorEditor> dexedEditors[numEngines];
    DisabledOverlay disabledOverlay;

    MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbidexAudioProcessorEditor)
};
