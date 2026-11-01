// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "AmbidexProcessor.h"
#include "DexedEditorCustomiser.h"
#include "FxPanel.h"
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
/** Name of a slot's preset, with a drop-down arrow: opens the preset menu. */
class PresetSelector : public Component {
public:
    std::function<void()> onClick;

    void setText(const String& newText);
    void setJustification(Justification newJustification) { justification = newJustification; }
    void setDimmed(bool shouldBeDimmed);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent&) override;

private:
    String text;
    Justification justification = Justification::centredLeft;
    bool dimmed = false;
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
 * of the engine being edited with the FX panel on its right, and a keyboard
 * playing both engines.
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
    // Dexed's zoom factor (applied to the FX panel too)
    float getZoomFactor() const;
    // Rounded panel holding the A/B buttons, program names and morph slider
    Rectangle<int> getMorphPanelBounds() const;
    // Button, preset name and mix share of an engine (framed when edited)
    Rectangle<int> getEngineArea(int index) const;

    // Presets (see doc/Presets.md)
    void showPresetMenu(int slot);
    void choosePreset(int slot, const String& name);
    void loadPresetFromLibrary(int slot, const String& name);
    void savePreset(int slot, const String& name);
    void savePresetAsNew(int slot);
    void renamePreset(int slot);
    void deletePreset(int slot);
    void presetChanged(int slot);
    void showFmPatchMenu(int slot, Component& button);
    // Asynchronous dialogs
    void askForPresetName(const String& title, const String& initialName, std::function<void(const String&)> onOk);
    void confirm(const String& title, const String& message, const String& okText, std::function<void()> onOk);
    void showError(const String& title, const String& message);

    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;
    void timerCallback() override;

    static constexpr int numEngines = AmbidexAudioProcessor::numEngines;

    AmbidexAudioProcessor& ambidex;

    AmbidexLookAndFeel lookAndFeel;
    Label title;
    CpuMeter cpuMeter;
    TextButton engineButtons[numEngines];
    PresetSelector presetSelectors[numEngines];
    // Power of each engine in the mix, in percent
    Label mixShares[numEngines];
    Slider morphSlider;
    SliderParameterAttachment morphAttachment;

    // Shows the top part of the Dexed editors, hiding their own keyboard
    Component dexedArea;
    std::unique_ptr<AudioProcessorEditor> dexedEditors[numEngines];
    std::unique_ptr<DexedEditorCustomiser> dexedEditorCustomisers[numEngines];
    DisabledOverlay disabledOverlay;

    FxPanel fxPanel { ambidex };

    MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbidexAudioProcessorEditor)
};
