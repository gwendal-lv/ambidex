// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "FxPanel.h"
#include "FxPlots.h"
#include "ui/DXLookNFeel.h"

namespace {
constexpr int moduleGap = 6;
constexpr int modulePadding = 8;
constexpr int titleHeight = 18;
constexpr int maxKnobsPerModule = 5;
constexpr int knobColumnWidth = 52;
constexpr int plotGap = 8;
// As in Dexed's editor
constexpr int knobSize = 34;
constexpr int labelHeight = 14;
constexpr int headerHeight = 42;
constexpr int chainSwitchWidth = 56;
constexpr int outputLabelWidth = 44;
constexpr int titleWidth = 84;
constexpr int meterHeight = 8;

// Meters: decay per timer tick (30 Hz), once the level drops
constexpr float peakDecay = 0.8f;
constexpr float gainReductionDecayDb = 1.5f;
// The modulation's LFO moves while its input is louder than this (and a bit after)
constexpr float soundThreshold = 1.0e-4f;
constexpr double soundHoldMs = 300.0;

Slider* createKnob(int param, const String& title) {
    const auto& paramInfo = fx::getParamInfo(param);
    auto* knob = new Slider(Slider::RotaryVerticalDrag, Slider::NoTextBox);
    knob->setTitle(title);
    knob->setRange(0.0, 1.0);
    knob->setDoubleClickReturnValue(true, fx::toNormalised(param, paramInfo.defaultValue));
    knob->textFromValueFunction = [param](double normalised) {
        return fx::getParamInfo(param).toText(fx::fromNormalised(param, (float) normalised));
    };
    knob->setPopupDisplayEnabled(true, true, nullptr);
    return knob;
}
} // namespace

//==============================================================================
FxModule::FxModule(int module, const FxLiveData& liveData) : info(fx::getModuleInfo(module)) {
    for (int i = 0; i < info.numParams; ++i) {
        const int param = info.firstParam + i;
        auto* knob = knobs.add(createKnob(param, info.name.toLowerCase() + " " + fx::getParamInfo(param).label.toLowerCase()));
        addAndMakeVisible(knob);
    }

    plot = FxPlot::create(module, *this, liveData);
    plot->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(*plot);
}

FxModule::~FxModule() = default;

float FxModule::getValue(int param) const {
    return fx::fromNormalised(param, (float) knobs[param - info.firstParam]->getValue());
}

void FxModule::valuesChanged() {
    repaintPlot();
}

void FxModule::repaintPlot() {
    plot->repaint();
}

void FxModule::setBypassedLook(bool shouldLookBypassed) {
    if (bypassedLook == shouldLookBypassed)
        return;

    bypassedLook = shouldLookBypassed;
    for (int i = 0; i < knobs.size(); ++i)
        if (getParam(i) != info.mixParam)
            knobs[i]->setAlpha(bypassedLook ? 0.35f : 1.0f);
    repaint();
    repaintPlot();
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
    // Knobs on the left, plot on the right
    auto area = getLocalBounds().reduced(modulePadding);
    plot->setBounds(area.removeFromRight(area.getWidth() - maxKnobsPerModule * knobColumnWidth - plotGap));
    area = area.withTrimmedTop(titleHeight).withWidth(maxKnobsPerModule * knobColumnWidth);
    const int columnWidth = knobColumnWidth;
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
    addAndMakeVisible(outputMeter);

    outputKnob.reset(createKnob(fx::outputGain, "output gain"));
    outputKnob->onValueChange = [this] {
        if (editableEngine >= 0)
            ambidex.setFxValue(editableEngine, fx::outputGain, fx::fromNormalised(fx::outputGain, (float) outputKnob->getValue()));
    };
    addAndMakeVisible(*outputKnob);

    for (int m = 0; m < fx::numModules; ++m) {
        auto* module = modules.add(new FxModule(m, liveData));
        for (int i = 0; i < module->getNumKnobs(); ++i)
            module->getKnob(i).onValueChange = [this, module, i] { knobChanged(*module, i); };
        addAndMakeVisible(module);
    }
    updateChainSwitch();

    lastTimerMs = Time::getMillisecondCounterHiRes();
    startTimerHz(30);
}

void FxPanel::timerCallback() {
    auto& meters = ambidex.getFxMeters();
    const double nowMs = Time::getMillisecondCounterHiRes();
    const double elapsedSeconds = jmin(0.2, (nowMs - lastTimerMs) * 0.001);
    lastTimerMs = nowMs;

    // Peaks since the last tick, held then decaying
    auto updatePeak = [](float& shown, std::atomic<float>& meter) {
        const float previous = shown;
        shown = jmax(meter.exchange(0.0f), shown * peakDecay);
        if (shown < 1.0e-4f)
            shown = 0.0f;
        return shown != previous;
    };
    const bool distortionChanged = updatePeak(liveData.distortionInput, meters.distortionInput);
    bool compressorChanged = updatePeak(liveData.compressorInput, meters.compressorInput);
    const float previousReduction = liveData.compressorGainReduction;
    liveData.compressorGainReduction = jmax(meters.compressorGainReduction.exchange(0.0f),
                                            liveData.compressorGainReduction - gainReductionDecayDb, 0.0f);
    compressorChanged = compressorChanged || liveData.compressorGainReduction != previousReduction;
    if (updatePeak(liveData.output, meters.output))
        outputMeter.repaint();

    if (distortionChanged)
        modules[fx::distortion]->repaintPlot();
    if (compressorChanged)
        modules[fx::compressor]->repaintPlot();

    // The LFO moves while sound goes through the modulation
    if (meters.modulationInput.exchange(0.0f) > soundThreshold)
        lastModulationSoundMs = nowMs;
    if (nowMs - lastModulationSoundMs < soundHoldMs) {
        liveData.modulationPhase += modules[fx::modulation]->getValue(fx::modulationRate) * elapsedSeconds;
        liveData.modulationPhase -= std::floor(liveData.modulationPhase);
        modules[fx::modulation]->repaintPlot();
    }
}

//==============================================================================
void OutputMeter::paint(Graphics& g) {
    constexpr float minDb = -48.0f;
    constexpr float softClipDb = -3.0f; // (where the soft clipper starts)
    const auto area = getLocalBounds().toFloat();
    auto toX = [&](float db) { return area.getX() + (jlimit(minDb, 0.0f, db) - minDb) / -minDb * area.getWidth(); };

    g.setColour(Colour(0xff141212));
    g.fillRoundedRectangle(area, 2.0f);

    const float levelX = toX(Decibels::gainToDecibels(live.output, minDb - 1.0f));
    const float clipX = toX(softClipDb);
    g.setColour(DXLookNFeel::fillColour);
    g.fillRect(area.withRight(jmin(levelX, clipX)).reduced(0.0f, 1.0f));
    if (levelX > clipX) {
        g.setColour(Colour(0xffe0a030));
        g.fillRect(area.withLeft(clipX).withRight(levelX).reduced(0.0f, 1.0f));
    }
    g.setColour(Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(roundToInt(clipX), area.getY(), area.getBottom());
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

Rectangle<int> FxPanel::getHeaderArea() const {
    return getLocalBounds().removeFromTop(headerHeight).withTrimmedTop(moduleGap).reduced(moduleGap + modulePadding, 0);
}

Rectangle<int> FxPanel::getOutputArea() const {
    return getHeaderArea().removeFromRight(outputLabelWidth + knobSize);
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
        module->valuesChanged();
    }

    outputKnob->setEnabled(editableEngine >= 0);
    outputKnob->setValue(editableEngine >= 0 ? fx::toNormalised(fx::outputGain, ambidex.getFxValue(editableEngine, fx::outputGain))
                                             : ambidex.getMorphedFxValueNormalised(fx::outputGain),
                         dontSendNotification);

    if (wasEditable != (editableEngine >= 0))
        repaint();
}

void FxPanel::knobChanged(FxModule& module, int knobIndex) {
    if (editableEngine < 0)
        return;

    const int param = module.getParam(knobIndex);
    ambidex.setFxValue(editableEngine, param, fx::fromNormalised(param, (float) module.getKnob(knobIndex).getValue()));
    updateBypassedLook(module);
    module.valuesChanged();
}

void FxPanel::updateBypassedLook(FxModule& module) {
    const int mixParam = fx::getModuleInfo(modules.indexOf(&module)).mixParam;
    module.setBypassedLook(module.getKnob(mixParam - module.getParam(0)).getValue() <= 0.0);
}

void FxPanel::paint(Graphics& g) {
    g.fillAll(DXLookNFeel::background);

    static const Typeface::Ptr bold = Typeface::createSystemTypefaceFor(BinaryData::NotoSansBold_ttf,
                                                                        BinaryData::NotoSansBold_ttfSize);
    auto header = getHeaderArea();
    g.setFont(FontOptions(bold).withHeight(14.0f));
    g.setColour(DXLookNFeel::fillColour);
    g.drawText("FX CHAIN", header, Justification::centredLeft);

    g.setFont(FontOptions(11.0f));
    g.setColour(Colours::white.withAlpha(0.75f));
    g.drawText(fx::getParamInfo(fx::outputGain).label, getOutputArea().removeFromLeft(outputLabelWidth),
               Justification::centredRight);

}

void FxPanel::paintOverChildren(Graphics& g) {
    // Greyed while morphing (the knobs still move with the morph). The switch
    // stays usable.
    if (editableEngine < 0) {
        // (as the overlay on the Dexed editor)
        g.setColour(DXLookNFeel::background.darker(0.6f).withAlpha(0.6f));
        g.fillRect(getModulesArea());
        g.fillRect(getOutputArea());
    }
}

void FxPanel::resized() {
    // FX CHAIN ... [ON/OFF]  Output (knob)
    auto header = getHeaderArea();
    outputKnob->setBounds(header.removeFromRight(knobSize).withSizeKeepingCentre(knobSize, knobSize));
    header.removeFromRight(outputLabelWidth + 12);
    chainSwitch.setBounds(header.removeFromRight(chainSwitchWidth).withSizeKeepingCentre(chainSwitchWidth, 24));
    header.removeFromLeft(titleWidth);
    outputMeter.setBounds(header.withTrimmedRight(12).withSizeKeepingCentre(header.getWidth() - 12, meterHeight));

    auto area = getModulesArea().reduced(moduleGap);
    const int moduleHeight = (area.getHeight() - (fx::numModules - 1) * moduleGap) / fx::numModules;

    for (auto* module : modules) {
        module->setBounds(area.removeFromTop(moduleHeight));
        area.removeFromTop(moduleGap);
    }
}
