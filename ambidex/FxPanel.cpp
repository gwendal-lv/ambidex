// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "FxPanel.h"
#include "ui/DXLookNFeel.h"

namespace {
constexpr int moduleGap = 6;
constexpr int modulePadding = 8;
constexpr int titleHeight = 18;
constexpr int maxKnobsPerModule = 5;
// As in Dexed's editor
constexpr int knobSize = 34;
constexpr int labelHeight = 14;
constexpr int headerHeight = 28;
constexpr int chainSwitchWidth = 64;
} // namespace

//==============================================================================
FxModule::FxModule(int module) : info(fx::getModuleInfo(module)) {
    for (int i = 0; i < info.numParams; ++i) {
        const int param = info.firstParam + i;
        const auto& paramInfo = fx::getParamInfo(param);

        auto* knob = knobs.add(new Slider(Slider::RotaryVerticalDrag, Slider::NoTextBox));
        knob->setTitle(info.name.toLowerCase() + " " + paramInfo.label.toLowerCase());
        knob->setRange(0.0, 1.0);
        knob->setDoubleClickReturnValue(true, fx::toNormalised(param, paramInfo.defaultValue));
        knob->textFromValueFunction = [param](double normalised) {
            return fx::getParamInfo(param).toText(fx::fromNormalised(param, (float) normalised));
        };
        knob->setPopupDisplayEnabled(true, true, nullptr);
        addAndMakeVisible(knob);
    }
}

void FxModule::setBypassedLook(bool shouldLookBypassed) {
    if (bypassedLook == shouldLookBypassed)
        return;

    bypassedLook = shouldLookBypassed;
    for (int i = 0; i < knobs.size(); ++i)
        if (getParam(i) != info.mixParam)
            knobs[i]->setAlpha(bypassedLook ? 0.35f : 1.0f);
    repaint();
}

void FxModule::paint(Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(Colours::black.withAlpha(0.15f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(DXLookNFeel::lightBackground);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    static const Typeface::Ptr bold = Typeface::createSystemTypefaceFor(BinaryData::NotoSansBold_ttf,
                                                                        BinaryData::NotoSansBold_ttfSize);
    g.setFont(FontOptions(bold).withHeight(13.0f));
    g.setColour(DXLookNFeel::fillColour.withAlpha(bypassedLook ? 0.4f : 1.0f));
    g.drawText(info.name, getLocalBounds().reduced(modulePadding, 0).removeFromTop(modulePadding + titleHeight).withTrimmedTop(modulePadding),
               Justification::centredLeft);

    g.setFont(FontOptions(11.0f));
    for (int i = 0; i < knobs.size(); ++i) {
        const bool dimmed = bypassedLook && getParam(i) != info.mixParam;
        g.setColour(Colours::white.withAlpha(dimmed ? 0.3f : 0.75f));
        const auto knobBounds = knobs[i]->getBounds();
        g.drawText(fx::getParamInfo(getParam(i)).label,
                   Rectangle<int>(knobBounds.getCentreX() - 30, knobBounds.getBottom(), 60, labelHeight),
                   Justification::centred);
    }
}

void FxModule::resized() {
    auto area = getLocalBounds().reduced(modulePadding).withTrimmedTop(titleHeight);
    const int columnWidth = area.getWidth() / maxKnobsPerModule;
    const int knobsTop = area.getY() + (area.getHeight() - knobSize - labelHeight) / 2;

    for (auto* knob : knobs)
        knob->setBounds(Rectangle<int>(knobSize, knobSize)
                            .withCentre({ area.removeFromLeft(columnWidth).getCentreX(), 0 })
                            .withY(knobsTop));
}

//==============================================================================
FxPanel::FxPanel(AmbidexAudioProcessor& processor) : ambidex(processor) {
    // (lit when the FX are on, as the A/B buttons)
    chainSwitch.setTitle("FX on/off");
    chainSwitch.setClickingTogglesState(true);
    chainSwitch.setColour(TextButton::textColourOffId, Colours::white.withAlpha(0.5f));
    chainSwitch.setColour(TextButton::textColourOnId, Colours::white);
    chainSwitch.setToggleState(!ambidex.fxBypassed, dontSendNotification);
    chainSwitch.onClick = [this] {
        ambidex.fxBypassed = !chainSwitch.getToggleState();
        updateChainSwitch();
    };
    addAndMakeVisible(chainSwitch);

    for (int m = 0; m < fx::numModules; ++m) {
        auto* module = modules.add(new FxModule(m));
        for (int i = 0; i < module->getNumKnobs(); ++i)
            module->getKnob(i).onValueChange = [this, module, i] { knobChanged(*module, i); };
        addAndMakeVisible(module);
    }
    updateChainSwitch();
}

void FxPanel::updateChainSwitch() {
    const bool on = chainSwitch.getToggleState();
    chainSwitch.setButtonText(on ? "ON" : "OFF");
    // (the modules can still be edited, while bypassed)
    for (auto* module : modules)
        module->setAlpha(on ? 1.0f : 0.5f);
    repaint();
}

Rectangle<int> FxPanel::getModulesArea() const {
    return getLocalBounds().withTrimmedTop(headerHeight);
}

void FxPanel::showValues(int newEditableEngine) {
    const bool wasEditable = editableEngine >= 0;
    editableEngine = newEditableEngine;

    for (auto* module : modules) {
        for (int i = 0; i < module->getNumKnobs(); ++i) {
            const int param = module->getParam(i);
            auto& knob = module->getKnob(i);
            knob.setEnabled(editableEngine >= 0);
            knob.setValue(editableEngine >= 0 ? fx::toNormalised(param, ambidex.getFxValue(editableEngine, param))
                                              : ambidex.getMorphedFxValueNormalised(param),
                          dontSendNotification);
        }
        updateBypassedLook(*module);
    }

    if (wasEditable != (editableEngine >= 0))
        repaint();
}

void FxPanel::knobChanged(FxModule& module, int knobIndex) {
    if (editableEngine < 0)
        return;

    const int param = module.getParam(knobIndex);
    ambidex.setFxValue(editableEngine, param, fx::fromNormalised(param, (float) module.getKnob(knobIndex).getValue()));
    updateBypassedLook(module);
}

void FxPanel::updateBypassedLook(FxModule& module) {
    const int mixParam = fx::getModuleInfo(modules.indexOf(&module)).mixParam;
    module.setBypassedLook(module.getKnob(mixParam - module.getParam(0)).getValue() <= 0.0);
}

void FxPanel::paint(Graphics& g) {
    g.fillAll(DXLookNFeel::background);

    static const Typeface::Ptr bold = Typeface::createSystemTypefaceFor(BinaryData::NotoSansBold_ttf,
                                                                        BinaryData::NotoSansBold_ttfSize);
    const bool on = chainSwitch.getToggleState();
    auto header = getLocalBounds().removeFromTop(headerHeight).withTrimmedTop(moduleGap).reduced(moduleGap + modulePadding, 0);
    g.setFont(FontOptions(bold).withHeight(14.0f));
    g.setColour(DXLookNFeel::fillColour);
    g.drawText("FX CHAIN", header, Justification::centredLeft);
    if (!on) {
        g.setFont(FontOptions(12.0f));
        g.setColour(Colours::white.withAlpha(0.6f));
        g.drawText("bypassed", header.withTrimmedRight(chainSwitchWidth + 8), Justification::centredRight);
    }
}

void FxPanel::paintOverChildren(Graphics& g) {
    // Greyed while morphing (the knobs still move with the morph). The switch
    // stays usable.
    if (editableEngine < 0) {
        // (as the overlay on the Dexed editor)
        g.setColour(DXLookNFeel::background.darker(0.6f).withAlpha(0.6f));
        g.fillRect(getModulesArea());
    }
}

void FxPanel::resized() {
    auto header = getLocalBounds().removeFromTop(headerHeight).withTrimmedTop(moduleGap).reduced(moduleGap, 0);
    chainSwitch.setBounds(header.removeFromRight(chainSwitchWidth).reduced(0, 1));

    auto area = getModulesArea().reduced(moduleGap);
    const int moduleHeight = (area.getHeight() - (fx::numModules - 1) * moduleGap) / fx::numModules;

    for (auto* module : modules) {
        module->setBounds(area.removeFromTop(moduleHeight));
        area.removeFromTop(moduleGap);
    }
}
