// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "AmbidexEditor.h"
#include "core/Cartridge.h"
#include "ui/PluginEditor.h"

namespace {
constexpr int topBarHeight = 58;
constexpr int titleWidth = 110;
// Between the title column and the morph panel
constexpr int titleGap = 10;
constexpr int engineButtonWidth = 40;
constexpr int presetNameWidth = 150;
constexpr int presetNameHeight = 19;
// Around the frame of the engine being edited, inside the morph panel
constexpr int engineFrameMargin = 4;

// Dexed's editor shows its own keyboard in its bottom 94 pixels (see
// DexedAudioProcessorEditor::resetSize()). Ambidex hides that part, and shows
// its own keyboard instead, with the same layout.
constexpr int keyboardAreaHeight = 94;
constexpr int keyboardHeight = 90;

constexpr int morphThumbRadius = 9;

// The Dexed editor shows engine B once the morph goes above this, and engine A
// once it goes below 1 - this (in between, it keeps showing the same engine)
constexpr double showEngineBAbove = 0.6;

const String engineNames[] = { "A", "B" };

// Load FM patch menu: the cartridges in a folder and its subfolders, with their
// voices (item IDs: 1 + cartridge index * 32 + voice index)
PopupMenu createCartridgeMenu(const File& folder, Array<File>& cartridges) {
    PopupMenu menu;

    auto subfolders = folder.findChildFiles(File::findDirectories, false);
    subfolders.sort();
    for (const auto& subfolder : subfolders)
        menu.addSubMenu(subfolder.getFileName(), createCartridgeMenu(subfolder, cartridges));

    auto files = folder.findChildFiles(File::findFiles, false, "*.syx;*.SYX");
    files.sort();
    for (const auto& file : files) {
        Cartridge cartridge;
        if (cartridge.load(file) == -1)
            continue;

        PopupMenu voices;
        for (int voice = 0; voice < 32; ++voice)
            voices.addItem(1 + cartridges.size() * 32 + voice, String(voice + 1) + ". " + cartridge.getProgramName(voice));
        menu.addSubMenu(file.getFileNameWithoutExtension(), voices);
        cartridges.add(file);
    }
    return menu;
}
} // namespace

// The red of Dexed's lit LEDs (Light_28x28.png)
const Colour AmbidexLookAndFeel::ledColour { 0xfff80000 };

//==============================================================================
void AmbidexLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                                              const Colour& backgroundColour,
                                              bool isMouseOverButton, bool isButtonDown) {
    const bool isOn = button.getClickingTogglesState() && button.getToggleState();
    DXLookNFeel::drawButtonBackground(g, button, backgroundColour, isMouseOverButton,
                                      isButtonDown || isOn);

    // Toggle buttons that are on stay pressed, and lit: brighten the pressed
    // button's face, but not its shadowed top edge
    if (isOn) {
        g.setColour(fillColour.withMultipliedSaturation(1.2f).withMultipliedBrightness(1.4f).withAlpha(0.75f));
        g.fillRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.5f).withTrimmedTop(1.5f), 2.0f);
    }
}

int AmbidexLookAndFeel::getSliderThumbRadius(Slider&) {
    return morphThumbRadius;
}

void AmbidexLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float, const Slider::SliderStyle,
                                          Slider& slider) {
    // (x and width are already inset by the thumb radius, so that the thumb
    // centre goes from x to x + width): the track also covers the thumb at both ends
    const float thumbRadius = (float) getSliderThumbRadius(slider);
    const float centreY = (float) y + (float) height * 0.5f;

    Rectangle<float> track((float) x - thumbRadius, centreY - 3.0f,
                           (float) width + 2.0f * thumbRadius, 6.0f);
    g.setColour(roundBackground.darker(0.5f));
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(lightBackground.brighter(0.2f));
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
    g.drawVerticalLine(roundToInt(track.getCentreX()), centreY - 8.0f, centreY + 8.0f);

    auto thumb = Rectangle<float>(2.0f * thumbRadius, 2.0f * thumbRadius).withCentre({ sliderPos, centreY });
    g.setColour(fillColour);
    g.fillEllipse(thumb);
    g.setColour(Colours::black.withAlpha(0.5f));
    g.drawEllipse(thumb, 1.0f);
}

//==============================================================================
void CpuMeter::setLoad(double proportion) {
    const int newPercent = jlimit(0, 100, roundToInt(proportion * 100.0));
    if (newPercent != percent) {
        percent = newPercent;
        repaint();
    }
}

void CpuMeter::paint(Graphics& g) {
    auto bounds = getLocalBounds();
    const auto textColour = Colours::white.withAlpha(0.5f);
    const auto barColour = percent >= 80 ? AmbidexLookAndFeel::ledColour : DXLookNFeel::fillColour;

    g.setFont(FontOptions(11.0f));
    g.setColour(textColour);
    g.drawText("CPU", bounds.removeFromLeft(26), Justification::centredLeft);
    g.setColour(percent >= 80 ? barColour : textColour);
    g.drawText(String(percent) + "%", bounds.removeFromRight(30), Justification::centredRight);

    auto track = bounds.toFloat().withSizeKeepingCentre((float) bounds.getWidth(), 4.0f);
    g.setColour(DXLookNFeel::roundBackground.darker(0.5f));
    g.fillRoundedRectangle(track, 2.0f);
    g.setColour(barColour);
    g.fillRoundedRectangle(track.withWidth(track.getWidth() * (float) percent / 100.0f), 2.0f);
    g.setColour(DXLookNFeel::lightBackground.brighter(0.2f));
    g.drawRoundedRectangle(track, 2.0f, 1.0f);
}

//==============================================================================
void PresetSelector::setText(const String& newText) {
    if (text != newText) {
        text = newText;
        repaint();
    }
}

void PresetSelector::setDimmed(bool shouldBeDimmed) {
    if (dimmed != shouldBeDimmed) {
        dimmed = shouldBeDimmed;
        repaint();
    }
}

void PresetSelector::paint(Graphics& g) {
    constexpr float arrowSize = 8.0f;
    // (a drop-down box)
    g.setColour(Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);

    auto area = getLocalBounds().reduced(5, 0).toFloat();
    const bool arrowOnLeft = justification.testFlags(Justification::right);
    const auto arrowArea = arrowOnLeft ? area.removeFromLeft(arrowSize) : area.removeFromRight(arrowSize);
    area = arrowOnLeft ? area.withTrimmedLeft(5.0f) : area.withTrimmedRight(5.0f);

    const auto colour = Colours::white.withAlpha(dimmed ? 0.5f : 1.0f);
    g.setColour(colour);
    g.setFont(FontOptions(15.0f));
    g.drawText(text, area, justification, true);

    Path arrow;
    const auto centre = arrowArea.getCentre();
    arrow.addTriangle(centre.x - arrowSize * 0.5f, centre.y - 2.0f, centre.x + arrowSize * 0.5f, centre.y - 2.0f,
                      centre.x, centre.y + 3.0f);
    g.setColour(colour.withMultipliedAlpha(0.8f));
    g.fillPath(arrow);
}

void PresetSelector::mouseDown(const MouseEvent&) {
    if (onClick)
        onClick();
}

//==============================================================================
DisabledOverlay::DisabledOverlay() {
    // (blocks the mouse clicks to the editor below)
    setInterceptsMouseClicks(true, true);
}

void DisabledOverlay::setEngineName(const String& engineName) {
    message = "Move the morph slider fully to " + engineName + " (or click " + engineName + ") to edit this preset";
    repaint();
}

void DisabledOverlay::paint(Graphics& g) {
    g.fillAll(DXLookNFeel::background.darker(0.6f).withAlpha(0.6f));

    const Font font(FontOptions(16.0f));
    const auto messageBox = getLocalBounds().toFloat()
        .withSizeKeepingCentre((float) GlyphArrangement::getStringWidthInt(font, message) + 40.0f, 40.0f);
    g.setColour(DXLookNFeel::background.withAlpha(0.9f));
    g.fillRoundedRectangle(messageBox, 6.0f);
    g.setColour(DXLookNFeel::lightBackground);
    g.drawRoundedRectangle(messageBox.reduced(0.5f), 6.0f, 1.0f);

    g.setColour(Colours::white.withAlpha(0.8f));
    g.setFont(font);
    g.drawText(message, messageBox, Justification::centred);
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

    cpuMeter.setTitle("CPU load");
    addAndMakeVisible(cpuMeter);

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
                activateEngine(i);
        };
        addAndMakeVisible(button);

        auto& presetSelector = presetSelectors[i];
        presetSelector.setTitle("Preset " + engineName);
        presetSelector.setJustification(i == 0 ? Justification::centredLeft : Justification::centredRight);
        presetSelector.onClick = [this, i] {
            activateEngine(i);
            showPresetMenu(i);
        };
        addAndMakeVisible(presetSelector);

        // (clicks on the mix share go to the editor, which activates the engine)
        auto& mixShare = mixShares[i];
        mixShare.setTitle("Share of preset " + engineName + " in the mix");
        mixShare.setFont(FontOptions(11.0f));
        mixShare.setJustificationType(i == 0 ? Justification::centredLeft : Justification::centredRight);
        mixShare.setColour(Label::textColourId, Colours::white.withAlpha(0.5f));
        mixShare.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(mixShare);

        dexedEditors[i].reset(ambidex.getEngine(i).createEditorIfNeeded());
        dexedEditors[i]->addComponentListener(this);
        dexedArea.addChildComponent(dexedEditors[i].get());
        dexedEditorCustomisers[i] = std::make_unique<DexedEditorCustomiser>(
            *dexedEditors[i], [this, i](Component& button) { showFmPatchMenu(i, button); });
    }
    dexedArea.addChildComponent(disabledOverlay);

    fxPanel.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(fxPanel);

    morphSlider.setSliderStyle(Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    morphSlider.setLookAndFeel(&lookAndFeel);
    morphSlider.setDoubleClickReturnValue(true, 0.5);
    morphSlider.setTitle("Morph from preset A to preset B");
    // (also called when the parameter changes, through the attachment)
    morphSlider.onValueChange = [this] { morphChanged(); };
    addAndMakeVisible(morphSlider);

    addAndMakeVisible(dexedArea);

    // The DX7 is a badass on the bass, keep it that way (as in Dexed)
    keyboard.setLowestVisibleKey(24);
    keyboard.setTitle("Keyboard keys");
    addAndMakeVisible(keyboard);

    showEngine(ambidex.editedEngine);
    morphChanged();
    timerCallback();
    startTimerHz(10);
}

AmbidexAudioProcessorEditor::~AmbidexAudioProcessorEditor() {
    stopTimer();
    for (int i = 0; i < numEngines; ++i) {
        dexedEditors[i]->removeComponentListener(this);
        dexedEditorCustomisers[i].reset();
        // (what a plugin host does before deleting an editor)
        ambidex.getEngine(i).editorBeingDeleted(dexedEditors[i].get());
        dexedEditors[i].reset();
    }
    for (auto& button : engineButtons)
        button.setLookAndFeel(nullptr);
    morphSlider.setLookAndFeel(nullptr);
    fxPanel.setLookAndFeel(nullptr);
}

//==============================================================================
void AmbidexAudioProcessorEditor::paint(Graphics& g) {
    g.fillAll(DXLookNFeel::background);
    g.setColour(DXLookNFeel::lightBackground);
    g.drawHorizontalLine(topBarHeight - 1, 0.0f, (float) getWidth());

    const auto morphPanel = getMorphPanelBounds().toFloat();
    g.setColour(Colours::black.withAlpha(0.15f));
    g.fillRoundedRectangle(morphPanel, 6.0f);
    g.setColour(DXLookNFeel::lightBackground);
    g.drawRoundedRectangle(morphPanel.reduced(0.5f), 6.0f, 1.0f);

    // Frame the engine shown, dimmed when it can't be edited
    g.setColour(AmbidexLookAndFeel::ledColour.withAlpha(isEditable() ? 1.0f : 0.35f));
    g.drawRoundedRectangle(getEngineArea(ambidex.editedEngine).toFloat(), 4.0f, 2.0f);
}

void AmbidexAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();

    auto titleColumn = bounds.removeFromTop(topBarHeight).withTrimmedLeft(8).withWidth(titleWidth);
    titleColumn = titleColumn.withSizeKeepingCentre(titleWidth, 38);
    title.setBounds(titleColumn.removeFromTop(26));
    // (aligned with the title's text, which the label insets by 5 pixels)
    cpuMeter.setBounds(titleColumn.removeFromTop(12).withTrimmedLeft(5).withTrimmedRight(12));

    auto panel = getMorphPanelBounds().reduced(2 * engineFrameMargin);
    engineButtons[0].setBounds(panel.removeFromLeft(engineButtonWidth));
    engineButtons[1].setBounds(panel.removeFromRight(engineButtonWidth));
    for (int i = 0; i < numEngines; ++i) {
        auto nameColumn = i == 0 ? panel.removeFromLeft(presetNameWidth) : panel.removeFromRight(presetNameWidth);
        presetSelectors[i].setBounds(nameColumn.removeFromTop(presetNameHeight));
        mixShares[i].setBounds(nameColumn);
    }
    morphSlider.setBounds(panel.reduced(12, 0));

    auto middle = bounds.removeFromTop(getDexedAreaHeight());
    dexedArea.setBounds(middle.removeFromLeft(dexedEditors[ambidex.editedEngine]->getWidth()));
    disabledOverlay.setBounds(dexedArea.getLocalBounds());

    // (zoomed as the Dexed editor)
    const float zoom = getZoomFactor();
    fxPanel.setTransform(AffineTransform::scale(zoom));
    fxPanel.setBounds(roundToInt((float) middle.getX() / zoom), roundToInt((float) middle.getY() / zoom),
                      FxPanel::width, roundToInt((float) middle.getHeight() / zoom));
    keyboard.setBounds(bounds.reduced(4, 0).withTrimmedTop(1).withHeight(keyboardHeight));
}

void AmbidexAudioProcessorEditor::mouseDown(const MouseEvent& event) {
    for (int i = 0; i < numEngines; ++i)
        if (getEngineArea(i).contains(event.getPosition()))
            activateEngine(i);
}

Rectangle<int> AmbidexAudioProcessorEditor::getMorphPanelBounds() const {
    const int left = 8 + titleWidth + titleGap;
    return { left, 5, getWidth() - left - 6, topBarHeight - 10 };
}

Rectangle<int> AmbidexAudioProcessorEditor::getEngineArea(int index) const {
    return engineButtons[index].getBounds()
        .getUnion(presetSelectors[index].getBounds())
        .getUnion(mixShares[index].getBounds())
        .expanded(engineFrameMargin);
}

//==============================================================================
void AmbidexAudioProcessorEditor::activateEngine(int index) {
    // (shows the engine, through morphChanged())
    morphSlider.setValue(index == 0 ? 0.0 : 1.0, sendNotificationSync);
    showEngine(index);
}

void AmbidexAudioProcessorEditor::showEngine(int index) {
    ambidex.editedEngine = index;
    engineButtons[index].setToggleState(true, dontSendNotification);

    for (int i = 0; i < numEngines; ++i) {
        dexedEditors[i]->setVisible(i == index);
        presetSelectors[i].setDimmed(i != index);
    }
    disabledOverlay.setEngineName(index == 0 ? "A" : "B");

    updateSize();
    updateEditability();
    repaint(getMorphPanelBounds());
}

void AmbidexAudioProcessorEditor::morphChanged() {
    updateMixShares();

    const double morph = morphSlider.getValue();
    if (morph > showEngineBAbove && ambidex.editedEngine != 1)
        showEngine(1);
    else if (morph < 1.0 - showEngineBAbove && ambidex.editedEngine != 0)
        showEngine(0);
    else
        updateEditability();
}

bool AmbidexAudioProcessorEditor::isEditable() const {
    // Only when the engine shown is the only one heard
    const double morph = morphSlider.getValue();
    return ambidex.editedEngine == 0 ? morph <= 0.0 : morph >= 1.0;
}

void AmbidexAudioProcessorEditor::updateEditability() {
    const bool editable = isEditable();
    // (also blocks Dexed's keyboard shortcuts)
    for (int i = 0; i < numEngines; ++i)
        dexedEditors[i]->setEnabled(editable && i == ambidex.editedEngine);

    if (disabledOverlay.isVisible() == editable) {
        disabledOverlay.setVisible(!editable);
        // (for the frame)
        repaint(getMorphPanelBounds());
    }

    fxPanel.showValues(editable ? ambidex.editedEngine : -1);
}

int AmbidexAudioProcessorEditor::getDexedAreaHeight() const {
    const auto& editor = *dexedEditors[ambidex.editedEngine];
    const int heightWithoutKeyboard = (int) ((DexedAudioProcessorEditor::WINDOW_SIZE_Y - keyboardAreaHeight) * getZoomFactor());

    // (the Dexed editor is already that small when its "show keyboard" option is off)
    return jmin(editor.getHeight(), heightWithoutKeyboard);
}

void AmbidexAudioProcessorEditor::updateSize() {
    const auto& editor = *dexedEditors[ambidex.editedEngine];
    setSize(editor.getWidth() + roundToInt(FxPanel::width * getZoomFactor()), topBarHeight + getDexedAreaHeight() + keyboardAreaHeight);
    resized();
}

float AmbidexAudioProcessorEditor::getZoomFactor() const {
    return ambidex.getEngine(ambidex.editedEngine).getZoomFactor();
}

void AmbidexAudioProcessorEditor::updateMixShares() {
    // Equal-power crossfade: the squared gains add up to 1
    const float gainA = AmbidexAudioProcessor::getMorphGains((float) morphSlider.getValue()).first;
    int percentA = roundToInt(gainA * gainA * 100.0f);
    // (100% only when the other engine is really silent)
    const double morph = morphSlider.getValue();
    if (morph > 0.0 && morph < 1.0)
        percentA = jlimit(1, 99, percentA);
    mixShares[0].setText(String(percentA) + "%", dontSendNotification);
    mixShares[1].setText(String(100 - percentA) + "%", dontSendNotification);
}

void AmbidexAudioProcessorEditor::componentMovedOrResized(Component& component, bool, bool wasResized) {
    // e.g. after a zoom change in Dexed's parameters dialog
    if (wasResized && &component == dexedEditors[ambidex.editedEngine].get())
        updateSize();
}

void AmbidexAudioProcessorEditor::timerCallback() {
    for (int i = 0; i < numEngines; ++i) {
        const auto& name = ambidex.getPresetName(i);
        presetSelectors[i].setText((name.isEmpty() ? String("Untitled") : name) + (ambidex.isPresetModified(i) ? " *" : ""));
        dexedEditorCustomisers[i]->setFmPatchName(ambidex.getEngine(i).activeProgram.getName());
    }

    cpuMeter.setLoad(ambidex.loadMeasurer.getLoadAsProportion());
}

//==============================================================================
void AmbidexAudioProcessorEditor::showPresetMenu(int slot) {
    enum { saveId = 1, saveAsNewId, renameId, revertId, deleteId, firstPresetId = 100 };

    const auto& library = ambidex.getPresetLibrary();
    const StringArray names = library.getNames();
    const String current = ambidex.getPresetName(slot);
    const bool inLibrary = library.contains(current);
    const bool modified = ambidex.isPresetModified(slot);

    PopupMenu menu;
    menu.addSectionHeader("Preset " + engineNames[slot]);
    for (int i = 0; i < names.size(); ++i)
        menu.addItem(PopupMenu::Item(names[i]).setID(firstPresetId + i).setTicked(names[i] == current));
    menu.addSeparator();
    menu.addItem(PopupMenu::Item("Save").setID(saveId).setEnabled(inLibrary && modified));
    menu.addItem(saveAsNewId, "Save as new preset...");
    menu.addItem(PopupMenu::Item("Rename...").setID(renameId).setEnabled(inLibrary));
    menu.addItem(PopupMenu::Item("Revert to saved").setID(revertId).setEnabled(inLibrary && modified));
    menu.addItem(PopupMenu::Item("Delete...").setID(deleteId).setEnabled(inLibrary));

    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&presetSelectors[slot]),
                       [safeThis, slot, names, current](int result) {
        if (safeThis == nullptr || result == 0)
            return;
        auto& editor = *safeThis;

        if (result >= firstPresetId) {
            editor.choosePreset(slot, names[result - firstPresetId]);
        } else if (result == saveId) {
            editor.savePreset(slot, current);
        } else if (result == saveAsNewId) {
            editor.savePresetAsNew(slot);
        } else if (result == renameId) {
            editor.renamePreset(slot);
        } else if (result == revertId) {
            editor.confirm("Revert to saved", "Discard the changes made to \"" + current + "\"?", "Revert",
                           [safeThis, slot, current] {
                if (safeThis != nullptr)
                    safeThis->loadPresetFromLibrary(slot, current);
            });
        } else if (result == deleteId) {
            editor.deletePreset(slot);
        }
    });
}

void AmbidexAudioProcessorEditor::choosePreset(int slot, const String& name) {
    if (!ambidex.isPresetModified(slot)) {
        loadPresetFromLibrary(slot, name);
        return;
    }

    const auto& current = ambidex.getPresetName(slot);
    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    confirm("Unsaved changes",
            "Preset " + engineNames[slot] + " (\"" + (current.isEmpty() ? String("Untitled") : current)
                + "\") has unsaved changes. Discard them, and load \"" + name + "\"?",
            "Discard and load", [safeThis, slot, name] {
        if (safeThis != nullptr)
            safeThis->loadPresetFromLibrary(slot, name);
    });
}

void AmbidexAudioProcessorEditor::loadPresetFromLibrary(int slot, const String& name) {
    AmbidexPreset preset;
    if (!ambidex.getPresetLibrary().load(name, preset)) {
        showError("Preset not loaded", "Unable to load the preset \"" + name + "\".");
        return;
    }
    ambidex.loadPreset(slot, preset);
    presetChanged(slot);
}

void AmbidexAudioProcessorEditor::savePreset(int slot, const String& name) {
    AmbidexPreset preset = ambidex.capturePreset(slot);
    preset.name = name;
    if (!ambidex.getPresetLibrary().save(preset)) {
        showError("Preset not saved", "Unable to save the preset \"" + name + "\".");
        return;
    }
    ambidex.setPresetSaved(slot, name);

    // (the other slot may hold the same preset: it now differs from the library)
    const int otherSlot = 1 - slot;
    if (ambidex.getPresetName(otherSlot) == name)
        ambidex.setPresetUnsaved(otherSlot);
    presetChanged(slot);
}

void AmbidexAudioProcessorEditor::savePresetAsNew(int slot) {
    const auto& current = ambidex.getPresetName(slot);
    const String initialName = current.isNotEmpty() ? current : ambidex.getEngine(slot).activeProgram.getName().trim();

    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    askForPresetName("Save as new preset", initialName, [safeThis, slot](const String& typedName) {
        if (safeThis == nullptr)
            return;
        auto& editor = *safeThis;

        const String name = PresetLibrary::toValidName(typedName);
        if (name.isEmpty()) {
            editor.showError("Preset not saved", "Please enter a name.");
        } else if (editor.ambidex.getPresetLibrary().contains(name)) {
            editor.confirm("Replace preset", "A preset named \"" + name + "\" already exists. Replace it?", "Replace",
                           [safeThis, slot, name] {
                if (safeThis != nullptr)
                    safeThis->savePreset(slot, name);
            });
        } else {
            editor.savePreset(slot, name);
        }
    });
}

void AmbidexAudioProcessorEditor::renamePreset(int slot) {
    const String oldName = ambidex.getPresetName(slot);

    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    askForPresetName("Rename preset", oldName, [safeThis, oldName](const String& typedName) {
        if (safeThis == nullptr)
            return;
        auto& editor = *safeThis;

        const String newName = PresetLibrary::toValidName(typedName);
        if (newName.isEmpty() || newName == oldName)
            return;
        if (editor.ambidex.getPresetLibrary().contains(newName)) {
            editor.showError("Preset not renamed", "A preset named \"" + newName + "\" already exists.");
            return;
        }
        if (!editor.ambidex.getPresetLibrary().rename(oldName, newName)) {
            editor.showError("Preset not renamed", "Unable to rename the preset \"" + oldName + "\".");
            return;
        }
        for (int s = 0; s < numEngines; ++s)
            if (editor.ambidex.getPresetName(s) == oldName)
                editor.ambidex.setPresetName(s, newName);
        editor.timerCallback();
    });
}

void AmbidexAudioProcessorEditor::deletePreset(int slot) {
    const String name = ambidex.getPresetName(slot);

    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    confirm("Delete preset",
            "Delete the preset \"" + name + "\" from the library? Preset " + engineNames[slot]
                + " keeps its sound, as an unsaved preset.",
            "Delete", [safeThis, name] {
        if (safeThis == nullptr)
            return;
        auto& editor = *safeThis;

        if (!editor.ambidex.getPresetLibrary().remove(name)) {
            editor.showError("Preset not deleted", "Unable to delete the preset \"" + name + "\".");
            return;
        }
        for (int s = 0; s < numEngines; ++s)
            if (editor.ambidex.getPresetName(s) == name)
                editor.ambidex.setPresetUnsaved(s);
        editor.timerCallback();
    });
}

void AmbidexAudioProcessorEditor::presetChanged(int slot) {
    // (the FX knobs show the new values, if this slot is the one edited)
    if (slot == ambidex.editedEngine)
        updateEditability();
    timerCallback();
}

void AmbidexAudioProcessorEditor::showFmPatchMenu(int slot, Component& button) {
    auto cartridges = std::make_shared<Array<File>>();
    PopupMenu menu = createCartridgeMenu(DexedAudioProcessor::dexedCartDir, *cartridges);
    if (cartridges->isEmpty()) {
        showError("No FM patches", "No cartridges found in " + DexedAudioProcessor::dexedCartDir.getFullPathName() + ".");
        return;
    }

    Component::SafePointer<AmbidexAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&button), [safeThis, slot, cartridges](int result) {
        if (safeThis == nullptr || result <= 0)
            return;

        Cartridge cartridge;
        if (cartridge.load((*cartridges)[(result - 1) / 32]) == -1)
            return;
        const Program program = cartridge.unpackProgram((result - 1) % 32);
        safeThis->ambidex.loadFmPatch(slot, program.getRawData());
        safeThis->timerCallback();
    });
}

//==============================================================================
void AmbidexAudioProcessorEditor::askForPresetName(const String& title, const String& initialName,
                                                   std::function<void(const String&)> onOk) {
    auto* window = new AlertWindow(title, "Preset name:", MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", initialName);
    window->addButton("OK", 1, KeyPress(KeyPress::returnKey));
    window->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
    window->enterModalState(true, ModalCallbackFunction::create([window, onOk](int result) {
        if (result == 1)
            onOk(window->getTextEditorContents("name"));
    }), true);
}

void AmbidexAudioProcessorEditor::confirm(const String& title, const String& message, const String& okText,
                                          std::function<void()> onOk) {
    AlertWindow::showAsync(MessageBoxOptions()
                               .withIconType(MessageBoxIconType::QuestionIcon)
                               .withTitle(title)
                               .withMessage(message)
                               .withButton(okText)
                               .withButton("Cancel")
                               .withAssociatedComponent(this),
                           [onOk](int result) {
        if (result == 1)
            onOk();
    });
}

void AmbidexAudioProcessorEditor::showError(const String& title, const String& message) {
    AlertWindow::showAsync(MessageBoxOptions()
                               .withIconType(MessageBoxIconType::WarningIcon)
                               .withTitle(title)
                               .withMessage(message)
                               .withButton("OK")
                               .withAssociatedComponent(this),
                           nullptr);
}
