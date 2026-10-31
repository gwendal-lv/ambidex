// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "AmbidexEditor.h"
#include "ui/PluginEditor.h"

namespace {
constexpr int topBarHeight = 48;
constexpr int titleWidth = 110;
constexpr int engineButtonWidth = 40;
constexpr int programNameWidth = 110;

// Dexed's editor shows its own keyboard in its bottom 94 pixels (see
// DexedAudioProcessorEditor::resetSize()). Ambidex hides that part, and shows
// its own keyboard instead, with the same layout.
constexpr int keyboardAreaHeight = 94;
constexpr int keyboardHeight = 90;
} // namespace

//==============================================================================
void AmbidexLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                                              const Colour& backgroundColour,
                                              bool isMouseOverButton, bool isButtonDown) {
    // Toggle buttons are lit (drawn as Dexed's normal teal button) when on
    const bool isLit = !button.getClickingTogglesState() || button.getToggleState();
    DXLookNFeel::drawButtonBackground(g, button, backgroundColour, isMouseOverButton,
                                      isButtonDown || !isLit);
}

void AmbidexLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float, const Slider::SliderStyle,
                                          Slider& slider) {
    // The thumb centre moves between both ends of the track
    const float thumbTravelInset = (float) getSliderThumbRadius(slider);
    const float centreY = (float) y + (float) height * 0.5f;

    Rectangle<float> track((float) x + thumbTravelInset, centreY - 3.0f,
                           (float) width - 2.0f * thumbTravelInset, 6.0f);
    g.setColour(roundBackground.darker(0.5f));
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(lightBackground.brighter(0.2f));
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
    g.drawVerticalLine(roundToInt(track.getCentreX()), centreY - 8.0f, centreY + 8.0f);

    const float thumbRadius = 9.0f;
    auto thumb = Rectangle<float>(2.0f * thumbRadius, 2.0f * thumbRadius).withCentre({ sliderPos, centreY });
    g.setColour(fillColour);
    g.fillEllipse(thumb);
    g.setColour(Colours::black.withAlpha(0.5f));
    g.drawEllipse(thumb, 1.0f);
}

//==============================================================================
AmbidexAudioProcessorEditor::AmbidexAudioProcessorEditor(AmbidexAudioProcessor& processor)
    : AudioProcessorEditor(processor), ambidex(processor),
      morphAttachment(*processor.morph, morphSlider),
      keyboard(processor.keyboardState, MidiKeyboardComponent::horizontalKeyboard) {

    title.setText("AMBIDEX", dontSendNotification);
    title.setFont(FontOptions(Typeface::createSystemTypefaceFor(BinaryData::NotoSansBold_ttf,
                                                                BinaryData::NotoSansBold_ttfSize))
                      .withHeight(22.0f));
    title.setColour(Label::textColourId, DXLookNFeel::fillColour);
    addAndMakeVisible(title);

    for (int i = 0; i < numEngines; ++i) {
        const String engineName = i == 0 ? "A" : "B";

        auto& button = engineButtons[i];
        button.setButtonText(engineName);
        button.setTitle("Edit preset " + engineName);
        button.setLookAndFeel(&lookAndFeel);
        button.setColour(TextButton::textColourOffId, Colours::white.withAlpha(0.5f));
        button.setColour(TextButton::textColourOnId, Colours::white);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(1);
        // (the radio group also "clicks" the button it turns off)
        button.onClick = [this, i] {
            if (engineButtons[i].getToggleState())
                selectEngine(i);
        };
        addAndMakeVisible(button);

        auto& programName = programNames[i];
        programName.setJustificationType(i == 0 ? Justification::centredLeft : Justification::centredRight);
        programName.setColour(Label::textColourId, Colours::white);
        addAndMakeVisible(programName);

        dexedEditors[i].reset(ambidex.getEngine(i).createEditorIfNeeded());
        dexedEditors[i]->addComponentListener(this);
        dexedArea.addChildComponent(dexedEditors[i].get());
    }

    morphSlider.setSliderStyle(Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    morphSlider.setLookAndFeel(&lookAndFeel);
    morphSlider.setDoubleClickReturnValue(true, 0.5);
    morphSlider.setTitle("Morph from preset A to preset B");
    addAndMakeVisible(morphSlider);

    addAndMakeVisible(dexedArea);

    // The DX7 is a badass on the bass, keep it that way (as in Dexed)
    keyboard.setLowestVisibleKey(24);
    keyboard.setTitle("Keyboard keys");
    addAndMakeVisible(keyboard);

    selectEngine(ambidex.editedEngine);
    timerCallback();
    startTimerHz(10);
}

AmbidexAudioProcessorEditor::~AmbidexAudioProcessorEditor() {
    stopTimer();
    for (int i = 0; i < numEngines; ++i) {
        dexedEditors[i]->removeComponentListener(this);
        // (what a plugin host does before deleting an editor)
        ambidex.getEngine(i).editorBeingDeleted(dexedEditors[i].get());
        dexedEditors[i].reset();
    }
    for (auto& button : engineButtons)
        button.setLookAndFeel(nullptr);
    morphSlider.setLookAndFeel(nullptr);
}

//==============================================================================
void AmbidexAudioProcessorEditor::paint(Graphics& g) {
    g.fillAll(DXLookNFeel::background);
    g.setColour(DXLookNFeel::lightBackground);
    g.drawHorizontalLine(topBarHeight - 1, 0.0f, (float) getWidth());
}

void AmbidexAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();

    auto topBar = bounds.removeFromTop(topBarHeight).reduced(8);
    title.setBounds(topBar.removeFromLeft(titleWidth));
    engineButtons[0].setBounds(topBar.removeFromLeft(engineButtonWidth));
    engineButtons[1].setBounds(topBar.removeFromRight(engineButtonWidth));
    programNames[0].setBounds(topBar.removeFromLeft(programNameWidth));
    programNames[1].setBounds(topBar.removeFromRight(programNameWidth));
    morphSlider.setBounds(topBar.reduced(8, 0));

    dexedArea.setBounds(bounds.removeFromTop(getDexedAreaHeight()));
    keyboard.setBounds(bounds.reduced(4, 0).withTrimmedTop(1).withHeight(keyboardHeight));
}

//==============================================================================
void AmbidexAudioProcessorEditor::selectEngine(int index) {
    ambidex.editedEngine = index;
    engineButtons[index].setToggleState(true, dontSendNotification);

    for (int i = 0; i < numEngines; ++i)
        dexedEditors[i]->setVisible(i == index);

    updateSize();
}

int AmbidexAudioProcessorEditor::getDexedAreaHeight() const {
    const auto& editor = *dexedEditors[ambidex.editedEngine];
    const float zoom = ambidex.getEngine(ambidex.editedEngine).getZoomFactor();
    const int heightWithoutKeyboard = (int) ((DexedAudioProcessorEditor::WINDOW_SIZE_Y - keyboardAreaHeight) * zoom);

    // (the Dexed editor is already that small when its "show keyboard" option is off)
    return jmin(editor.getHeight(), heightWithoutKeyboard);
}

void AmbidexAudioProcessorEditor::updateSize() {
    const auto& editor = *dexedEditors[ambidex.editedEngine];
    setSize(editor.getWidth(), topBarHeight + getDexedAreaHeight() + keyboardAreaHeight);
    resized();
}

void AmbidexAudioProcessorEditor::componentMovedOrResized(Component& component, bool, bool wasResized) {
    // e.g. after a zoom change in Dexed's parameters dialog
    if (wasResized && &component == dexedEditors[ambidex.editedEngine].get())
        updateSize();
}

void AmbidexAudioProcessorEditor::timerCallback() {
    for (int i = 0; i < numEngines; ++i)
        programNames[i].setText(ambidex.getEngine(i).activeProgram.getName(), dontSendNotification);
}
