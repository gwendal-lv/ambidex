// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "AmbidexProcessor.h"
#include "ui/DXLookNFeel.h"

/** Dexed's look, plus a visible toggle state on buttons and a drawn track on linear sliders. */
class AmbidexLookAndFeel : public DXLookNFeel {
public:
    void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;
    void drawLinearSlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, const Slider::SliderStyle style,
                          Slider& slider) override;
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

private:
    void selectEngine(int index);
    void updateSize();
    int getDexedAreaHeight() const;

    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;
    void timerCallback() override;

    static constexpr int numEngines = AmbidexAudioProcessor::numEngines;

    AmbidexAudioProcessor& ambidex;

    AmbidexLookAndFeel lookAndFeel;
    Label title;
    TextButton engineButtons[numEngines];
    Label programNames[numEngines];
    Slider morphSlider;
    SliderParameterAttachment morphAttachment;

    // Shows the top part of the Dexed editors, hiding their own keyboard
    Component dexedArea;
    std::unique_ptr<AudioProcessorEditor> dexedEditors[numEngines];

    MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbidexAudioProcessorEditor)
};
